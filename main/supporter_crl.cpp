/*
 *  supporter_crl.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Modified work Copyright 2025 Xerolux
 *
 *  Licensed under CC BY-NC-SA 4.0
 */

#include "supporter_crl.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ota_config.h"
#include "cJSON.h"
#include "psa/crypto.h"

#include <string.h>

static const char *TAG = "SupporterCRL";

// The published revocation list lives in the repo root so it is served via
// raw.githubusercontent.com. It is a JSON array of 128-bit SHA-256
// fingerprints (32 hex chars each) — opaque, unrecoverable to a key.
static const char *CRL_URL =
    "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/revoked_keys.json";

// Outbound TLS fetches are serialised on this mutex (declared in
// monitoring.cpp) so concurrent handshakes don't exhaust the heap.
extern SemaphoreHandle_t g_net_fetch_mutex;

// Hard cap on how many revoked fingerprints we keep. Realistically < 10;
// 128 gives generous headroom at 16 bytes each = 2 KB static RAM.
#define CRL_MAX_ENTRIES 128
#define CRL_FP_BYTES    16   // 128-bit truncated SHA-256
#define CRL_FETCH_CAP   4096 // revoked_keys.json is tiny; cap memory use

static uint8_t s_fp[CRL_MAX_ENTRIES][CRL_FP_BYTES];
static int s_count = 0;
static SemaphoreHandle_t s_mutex = NULL;
static TaskHandle_t s_refresh_task = NULL;
static SemaphoreHandle_t s_start_mutex = NULL;

static const char *NVS_NAMESPACE = "HB-RF-ETH";
static const char *NVS_KEY = "supCrl"; // 6 chars — within NVS 15-char limit

// ---- helpers ----

static int hex_pair(char hi, char lo)
{
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    int h = val(hi), l = val(lo);
    if (h < 0 || l < 0) return -1;
    return (h << 4) | l;
}

// Normalise a key the same way the tool does before hashing: uppercase,
// drop dashes and spaces. Output is written to `out` (must hold >= 17 bytes).
static size_t normalise_key(const char *key, char *out, size_t outSize)
{
    if (!key || !out || outSize == 0) return 0;
    size_t n = 0;
    for (size_t i = 0; key[i] && n + 1 < outSize; i++) {
        char c = key[i];
        if (c == '-' || c == ' ') continue;
        if (c >= 'a' && c <= 'z') c -= 32;
        out[n++] = c;
    }
    out[n] = 0;
    return n;
}

static void compute_fingerprint(const char *normalised, uint8_t out[CRL_FP_BYTES])
{
    // mbedTLS 4.x (ESP-IDF 6.0) dropped the standalone mbedtls/sha256.h in
    // favour of the PSA Crypto one-shot API. PSA uses the ESP32 hardware SHA
    // accelerator when available, so this stays cheap.
    static bool psa_ready = false;
    if (!psa_ready) {
        psa_crypto_init();
        psa_ready = true;
    }
    unsigned char full[32];
    size_t out_len = 0;
    psa_hash_compute(PSA_ALG_SHA_256,
                     (const uint8_t *)normalised, strlen(normalised),
                     full, sizeof(full), &out_len);
    memcpy(out, full, CRL_FP_BYTES);
}

// ---- persistence ----

static void load_from_nvs()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = 0;
    if (nvs_get_blob(h, NVS_KEY, NULL, &len) == ESP_OK) {
        size_t cnt = len / CRL_FP_BYTES;
        if (cnt > CRL_MAX_ENTRIES) cnt = CRL_MAX_ENTRIES;
        if (cnt > 0) {
            nvs_get_blob(h, NVS_KEY, s_fp, &len);
            s_count = (int)cnt;
        } else {
            s_count = 0;
        }
    }
    nvs_close(h);
    ESP_LOGI(TAG, "Loaded %d revoked fingerprint(s) from NVS", s_count);
}

static void save_to_nvs()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    if (s_count > 0) {
        nvs_set_blob(h, NVS_KEY, s_fp, (size_t)s_count * CRL_FP_BYTES);
    } else {
        nvs_erase_key(h, NVS_KEY);
    }
    nvs_commit(h);
    nvs_close(h);
}

// ---- public API ----

bool supporter_crl_is_revoked(const char *key)
{
    if (!key || !s_mutex) return false;
    char norm[20];
    if (normalise_key(key, norm, sizeof(norm)) == 0) return false;

    uint8_t fp[CRL_FP_BYTES];
    compute_fingerprint(norm, fp);

    bool revoked = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        if (memcmp(s_fp[i], fp, CRL_FP_BYTES) == 0) { revoked = true; break; }
    }
    xSemaphoreGive(s_mutex);
    return revoked;
}

bool supporter_crl_refresh()
{
    // Serialise with all other outbound TLS fetches (update check, changelog
    // proxy, OTA) — two concurrent handshakes exhaust the WROOM-32 heap.
    if (g_net_fetch_mutex && xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) != pdTRUE) {
        ESP_LOGW(TAG, "Refresh skipped: another HTTPS fetch is in progress");
        return false;
    }

    char *buf = (char *)malloc(CRL_FETCH_CAP);
    esp_http_client_handle_t client = NULL;
    bool ok = false;

    if (!buf) {
        ESP_LOGE(TAG, "Could not allocate CRL fetch buffer");
        goto done;
    }

    {
        esp_http_client_config_t config = {};
        configure_ota_http_client(config, CRL_URL);
        config.timeout_ms = 10000;
        config.buffer_size = 1024;

        client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "HTTP client init failed");
            goto done;
        }
        esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");
        esp_http_client_set_header(client, "Accept", "application/json");
        esp_http_client_set_header(client, "Accept-Encoding", "identity");

        if (esp_http_client_open(client, 0) != ESP_OK) {
            ESP_LOGW(TAG, "HTTP open failed");
            goto done;
        }
        esp_http_client_fetch_headers(client);

        int total = 0;
        while (total < CRL_FETCH_CAP - 1) {
            int n = esp_http_client_read(client, buf + total, CRL_FETCH_CAP - 1 - total);
            if (n <= 0) break;
            total += n;
        }
        buf[total] = 0;
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        client = NULL;

        if (status != 200 || total == 0) {
            // 404 is normal when nobody has been revoked yet — not an error.
            ESP_LOGI(TAG, "CRL fetch returned status %d, len %d (keeping cached list)", status, total);
            goto done;
        }
    }

    // Parse the JSON array of hex fingerprints into binary.
    {
        cJSON *root = cJSON_Parse(buf);
        if (!root || !cJSON_IsArray(root)) {
            ESP_LOGW(TAG, "CRL JSON parse failed");
            if (root) cJSON_Delete(root);
            goto done;
        }

        // Heap-allocate the staging matrix (CRL_MAX_ENTRIES * CRL_FP_BYTES =
        // 128 * 16 = 2048 bytes). With this array on the stack, the combined
        // demand of the parse frame + esp_http_client + mbedTLS handshake
        // overflowed the task stack at the first 60-second refresh and
        // panicked the CPU (reset reason "Exception/Panic", no log output
        // because panic_print bypasses the LogManager vprintf hook). The
        // allocation is bounded and freed below before returning.
        uint8_t *tmp = (uint8_t *)malloc((size_t)CRL_MAX_ENTRIES * CRL_FP_BYTES);
        if (!tmp) {
            ESP_LOGE(TAG, "Could not allocate CRL parse buffer");
            cJSON_Delete(root);
            goto done;
        }
        int cnt = 0;
        cJSON *item;
        cJSON_ArrayForEach(item, root) {
            if (cnt >= CRL_MAX_ENTRIES) break;
            if (!cJSON_IsString(item)) continue;
            const char *hex = item->valuestring;
            if (strlen(hex) < (size_t)(CRL_FP_BYTES * 2)) continue;
            bool valid = true;
            for (int b = 0; b < CRL_FP_BYTES; b++) {
                int v = hex_pair(hex[b * 2], hex[b * 2 + 1]);
                if (v < 0) { valid = false; break; }
                tmp[cnt * CRL_FP_BYTES + b] = (uint8_t)v;
            }
            if (valid) cnt++;
        }
        cJSON_Delete(root);

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_count = cnt;
        if (cnt > 0) memcpy(s_fp, tmp, (size_t)cnt * CRL_FP_BYTES);
        xSemaphoreGive(s_mutex);
        free(tmp);
        tmp = NULL;
        save_to_nvs();

        ESP_LOGI(TAG, "CRL refreshed: %d revoked fingerprint(s)", cnt);
        ok = true;
    }

done:
    if (client) esp_http_client_cleanup(client);
    free(buf);
    if (g_net_fetch_mutex) xSemaphoreGive(g_net_fetch_mutex);
    return ok;
}

static void crl_refresh_task(void *)
{
    // Let the boot sequence + first update-check settle before the first fetch.
    vTaskDelay(pdMS_TO_TICKS(60000));
    supporter_crl_refresh();
    // Refresh roughly every 6 h. pdMS_TO_TICKS overflows for large values on
    // high-tick-rate chips, so loop in 1 h chunks (see CLAUDE.md note).
    for (;;) {
        for (int i = 0; i < 6; i++) vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));
        supporter_crl_refresh();
    }
}

void supporter_crl_init()
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    load_from_nvs();
}

void supporter_crl_start_refresh_task()
{
    if (!s_start_mutex) s_start_mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(s_start_mutex, portMAX_DELAY);
    if (s_refresh_task == NULL)
    {
        // 8 KB matches the other TLS-doing tasks (events, syslog, ext_proxy)
        // and leaves comfortable headroom for the mbedTLS handshake inside
        // esp_http_client_open. 4 KB was too tight and crashed at the first
        // 60-second refresh once heap was loaded with the boot-time handshake.
        if (xTaskCreate(crl_refresh_task, "crl_refresh", 8192, NULL, 2, &s_refresh_task) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create CRL refresh task");
        }
        else
        {
            ESP_LOGI(TAG, "CRL refresh task started");
        }
    }
    xSemaphoreGive(s_start_mutex);
}
