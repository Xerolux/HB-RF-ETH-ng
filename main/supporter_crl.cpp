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
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ota_config.h"
#include "nvs_storage_lock.h"
#include "cJSON.h"
#include "psa/crypto.h"

#include <atomic>
#include <stdlib.h>
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
static StaticSemaphore_t s_data_mutex_buffer;
static std::atomic<TaskHandle_t> s_refresh_task{NULL};
static std::atomic<bool> s_stop_requested{false};
static std::atomic<bool> s_restart_requested{false};
static std::atomic<bool> s_psa_ready{false};
static StaticSemaphore_t s_lifecycle_mutex_buffer;

static constexpr int CRL_REFRESH_TOTAL_TIMEOUT_MS = 15000;
static constexpr int CRL_ASYNC_RETRY_MS = 10;

static SemaphoreHandle_t crl_lifecycle_mutex()
{
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateMutexStatic(&s_lifecycle_mutex_buffer);
    return mutex;
}

static const char *NVS_NAMESPACE = "supporter_crl";
static const char *LEGACY_NVS_NAMESPACE = "HB-RF-ETH";
static const char *NVS_KEY = "supCrl"; // 6 chars — within NVS 15-char limit
static const char *NVS_CACHE_PRESENT_KEY = "cacheValid";

// ---- helpers ----

static int crl_remaining_ms(int64_t deadline_us)
{
    const int64_t remaining_us = deadline_us - esp_timer_get_time();
    if (remaining_us <= 0) return 0;
    const int64_t rounded_ms = (remaining_us + 999) / 1000;
    return rounded_ms > INT32_MAX ? INT32_MAX : static_cast<int>(rounded_ms);
}

static bool crl_cancelled_or_expired(int64_t deadline_us)
{
    return s_stop_requested.load(std::memory_order_acquire) ||
           crl_remaining_ms(deadline_us) <= 0;
}

static bool crl_set_remaining_timeout(esp_http_client_handle_t client,
                                      int64_t deadline_us)
{
    const int remaining_ms = crl_remaining_ms(deadline_us);
    return remaining_ms > 0 &&
           esp_http_client_set_timeout_ms(client, remaining_ms) == ESP_OK;
}

static void crl_async_retry_delay(int64_t deadline_us)
{
    int delay_ms = crl_remaining_ms(deadline_us);
    if (delay_ms > CRL_ASYNC_RETRY_MS) delay_ms = CRL_ASYNC_RETRY_MS;
    if (delay_ms > 0) vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

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

static bool compute_fingerprint(const char *normalised,
                                uint8_t out[CRL_FP_BYTES])
{
    // mbedTLS 4.x (ESP-IDF 6.0) dropped the standalone mbedtls/sha256.h in
    // favour of the PSA Crypto one-shot API. PSA uses the ESP32 hardware SHA
    // accelerator when available, so this stays cheap.
    memset(out, 0, CRL_FP_BYTES);
    if (!normalised || !s_psa_ready.load(std::memory_order_acquire)) {
        ESP_LOGE(TAG, "Cannot compute fingerprint: PSA Crypto is unavailable");
        return false;
    }

    unsigned char full[32] = {};
    size_t out_len = 0;
    const psa_status_t hash_status =
        psa_hash_compute(PSA_ALG_SHA_256,
                         (const uint8_t *)normalised, strlen(normalised),
                         full, sizeof(full), &out_len);
    if (hash_status != PSA_SUCCESS || out_len != sizeof(full)) {
        ESP_LOGE(TAG, "Fingerprint hash failed: status=%d, len=%u",
                 (int)hash_status, (unsigned)out_len);
        return false;
    }

    memcpy(out, full, CRL_FP_BYTES);
    return true;
}

// ---- persistence ----

static esp_err_t read_cache_from_namespace(const char *nvs_namespace,
                                           bool accept_empty_marker,
                                           uint8_t **loaded_fp,
                                           size_t *loaded_len)
{
    if (!nvs_namespace || !loaded_fp || !loaded_len) return ESP_ERR_INVALID_ARG;
    *loaded_fp = NULL;
    *loaded_len = 0;

    nvs_handle_t h;
    esp_err_t result = nvs_open(nvs_namespace, NVS_READONLY, &h);
    if (result != ESP_OK) return result;

    size_t len = 0;
    result = nvs_get_blob(h, NVS_KEY, NULL, &len);
    if (result == ESP_ERR_NVS_NOT_FOUND && accept_empty_marker) {
        uint8_t cache_valid = 0;
        result = nvs_get_u8(h, NVS_CACHE_PRESENT_KEY, &cache_valid);
        nvs_close(h);
        if (result == ESP_OK && cache_valid == 1) return ESP_OK;
        if (result == ESP_OK) return ESP_ERR_INVALID_STATE;
        return result;
    }
    if (result != ESP_OK) {
        nvs_close(h);
        return result;
    }

    // Treat malformed metadata as a corrupt cache. In particular, never pass
    // an NVS-provided size larger than the 2 KiB in-memory matrix to the data
    // read, even if the blob changes between the size query and the read.
    if (len == 0 || len > sizeof(s_fp) || len % CRL_FP_BYTES != 0) {
        ESP_LOGW(TAG, "Ignoring invalid CRL blob length: %u", (unsigned)len);
        nvs_close(h);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *data = (uint8_t *)malloc(len);
    if (!data) {
        ESP_LOGE(TAG, "Could not allocate CRL NVS staging buffer");
        nvs_close(h);
        return ESP_ERR_NO_MEM;
    }

    size_t read_len = len;
    result = nvs_get_blob(h, NVS_KEY, data, &read_len);
    nvs_close(h);
    if (result != ESP_OK || read_len != len) {
        ESP_LOGW(TAG,
                 "Ignoring CRL blob read failure: status=%d, expected=%u, read=%u",
                 (int)result, (unsigned)len, (unsigned)read_len);
        free(data);
        return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
    }

    *loaded_fp = data;
    *loaded_len = len;
    return ESP_OK;
}

// Caller holds NvsStorageLock. A separate presence key makes an empty CRL a
// durable value, so a successfully refreshed empty list can never fall back to
// a stale legacy cache on the next boot.
static esp_err_t write_cache_to_new_namespace(const uint8_t *data, size_t len)
{
    nvs_handle_t h;
    esp_err_t result = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (result != ESP_OK) return result;

    if (len > 0) {
        result = nvs_set_blob(h, NVS_KEY, data, len);
    } else {
        result = nvs_erase_key(h, NVS_KEY);
        if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    }
    if (result == ESP_OK) {
        result = nvs_set_u8(h, NVS_CACHE_PRESENT_KEY, 1);
    }
    if (result == ESP_OK) result = nvs_commit(h);
    nvs_close(h);
    return result;
}

static void install_loaded_cache(const uint8_t *data, size_t len,
                                 const char *source_namespace)
{
    const int loaded_count = (int)(len / CRL_FP_BYTES);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (len > 0) memcpy(s_fp, data, len);
    s_count = loaded_count;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Loaded %d revoked fingerprint(s) from NVS namespace '%s'",
             loaded_count, source_namespace);
}

static void erase_legacy_cache_best_effort()
{
    nvs_handle_t legacy_handle;
    esp_err_t result = nvs_open(LEGACY_NVS_NAMESPACE, NVS_READWRITE,
                                &legacy_handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return;
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Could not open legacy CRL cache for cleanup: %s",
                 esp_err_to_name(result));
        return;
    }
    result = nvs_erase_key(legacy_handle, NVS_KEY);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(legacy_handle);
    nvs_close(legacy_handle);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Could not erase legacy CRL cache: %s",
                 esp_err_to_name(result));
    }
}

static void load_from_nvs()
{
    NvsStorageLock storage_lock;
    if (!storage_lock) {
        ESP_LOGE(TAG, "Could not reserve NVS storage while loading CRL cache");
        return;
    }

    uint8_t *loaded_fp = NULL;
    size_t loaded_len = 0;
    esp_err_t result = read_cache_from_namespace(
        NVS_NAMESPACE, true, &loaded_fp, &loaded_len);
    if (result == ESP_OK) {
        install_loaded_cache(loaded_fp, loaded_len, NVS_NAMESPACE);
        free(loaded_fp);
        return;
    }
    if (result != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Ignoring invalid CRL cache in new namespace: %s",
                 esp_err_to_name(result));
        return;
    }

    // Only genuine absence in the new namespace permits legacy fallback.
    result = read_cache_from_namespace(LEGACY_NVS_NAMESPACE, false,
                                       &loaded_fp, &loaded_len);
    if (result == ESP_ERR_NVS_NOT_FOUND) return;
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Ignoring invalid legacy CRL cache: %s",
                 esp_err_to_name(result));
        return;
    }

    install_loaded_cache(loaded_fp, loaded_len, LEGACY_NVS_NAMESPACE);
    const esp_err_t migration_result =
        write_cache_to_new_namespace(loaded_fp, loaded_len);
    if (migration_result == ESP_OK) {
        // The new copy is committed before this best-effort erase. A failed
        // migration always leaves the sole durable legacy copy untouched.
        erase_legacy_cache_best_effort();
    } else {
        ESP_LOGW(TAG,
                 "Could not migrate legacy CRL cache; retaining legacy value: %s",
                 esp_err_to_name(migration_result));
    }
    free(loaded_fp);
}

static esp_err_t save_to_nvs()
{
    NvsStorageLock storage_lock;
    if (!storage_lock) return ESP_ERR_NO_MEM;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const esp_err_t result = write_cache_to_new_namespace(
        &s_fp[0][0], (size_t)s_count * CRL_FP_BYTES);
    xSemaphoreGive(s_mutex);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not persist CRL cache: %s",
                 esp_err_to_name(result));
    }
    return result;
}

// ---- public API ----

bool supporter_crl_is_revoked(const char *key)
{
    if (!key || !s_mutex) return false;
    char norm[20];
    if (normalise_key(key, norm, sizeof(norm)) == 0) return false;

    uint8_t fp[CRL_FP_BYTES];
    if (!compute_fingerprint(norm, fp)) {
        ESP_LOGE(TAG, "Fingerprint unavailable; treating supporter key as revoked");
        return true;
    }

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
    // The deadline includes waiting for the shared TLS mutex, DNS/connect,
    // handshake, response headers and body. Async HTTPS keeps every library
    // step non-blocking so the absolute budget and cooperative stop flag are
    // checked throughout the transaction rather than only between requests.
    const int64_t deadline_us = esp_timer_get_time() +
        static_cast<int64_t>(CRL_REFRESH_TOTAL_TIMEOUT_MS) * 1000;
    bool net_mutex_held = false;

    // Serialise with all other outbound TLS users (syslog, events, MQTT) — two
    // concurrent handshakes can exhaust the WROOM-32 heap. Manual firmware
    // uploads use the same gate to reserve memory while flash is written.
    const int mutex_wait_ms = crl_remaining_ms(deadline_us);
    if (g_net_fetch_mutex &&
        (mutex_wait_ms <= 0 ||
         xSemaphoreTake(g_net_fetch_mutex,
                        pdMS_TO_TICKS(mutex_wait_ms)) != pdTRUE)) {
        ESP_LOGW(TAG, "Refresh skipped: another HTTPS fetch is in progress");
        return false;
    }
    net_mutex_held = g_net_fetch_mutex != NULL;

    // Stop may have been requested while this task waited for another TLS
    // user. Never begin an HTTP allocation/handshake after acquiring the
    // shared mutex in that state.
    if (crl_cancelled_or_expired(deadline_us)) {
        if (net_mutex_held) xSemaphoreGive(g_net_fetch_mutex);
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
        config.timeout_ms = crl_remaining_ms(deadline_us);
        config.buffer_size = 1024;
        config.is_async = true;

        client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "HTTP client init failed");
            goto done;
        }
        esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");
        esp_http_client_set_header(client, "Accept", "application/json");
        esp_http_client_set_header(client, "Accept-Encoding", "identity");

        esp_err_t open_result;
        do {
            if (crl_cancelled_or_expired(deadline_us) ||
                !crl_set_remaining_timeout(client, deadline_us)) {
                open_result = ESP_ERR_TIMEOUT;
                break;
            }
            open_result = esp_http_client_open(client, 0);
            if (open_result == ESP_ERR_HTTP_EAGAIN) {
                crl_async_retry_delay(deadline_us);
            }
        } while (open_result == ESP_ERR_HTTP_EAGAIN);
        if (open_result != ESP_OK) {
            ESP_LOGW(TAG, "HTTP open failed");
            goto done;
        }

        int64_t header_result;
        do {
            if (crl_cancelled_or_expired(deadline_us) ||
                !crl_set_remaining_timeout(client, deadline_us)) {
                header_result = -ESP_ERR_TIMEOUT;
                break;
            }
            header_result = esp_http_client_fetch_headers(client);
            if (header_result == -ESP_ERR_HTTP_EAGAIN) {
                crl_async_retry_delay(deadline_us);
            }
        } while (header_result == -ESP_ERR_HTTP_EAGAIN);
        if (header_result < 0) {
            ESP_LOGW(TAG, "HTTP headers failed");
            goto done;
        }

        int total = 0;
        while (total < CRL_FETCH_CAP - 1) {
            if (crl_cancelled_or_expired(deadline_us) ||
                !crl_set_remaining_timeout(client, deadline_us)) {
                ESP_LOGW(TAG, "CRL refresh cancelled or timed out");
                goto done;
            }
            const int remaining_capacity = CRL_FETCH_CAP - 1 - total;
            const int read_size = remaining_capacity < 256
                ? remaining_capacity : 256;
            int n = esp_http_client_read(client, buf + total, read_size);
            if (n == -ESP_ERR_HTTP_EAGAIN) {
                crl_async_retry_delay(deadline_us);
                continue;
            }
            if (n <= 0) break;
            total += n;
        }
        buf[total] = 0;
        int status = esp_http_client_get_status_code(client);
        const bool response_complete =
            esp_http_client_is_complete_data_received(client);
        esp_http_client_cleanup(client);
        client = NULL;

        if (status != 200 || total == 0 || !response_complete) {
            // 404 is normal when nobody has been revoked yet — not an error.
            // Logged at DEBUG so the default log doesn't show a non-issue every refresh.
            ESP_LOGD(TAG,
                     "CRL fetch returned status %d, len %d, complete=%d "
                     "(keeping cached list)",
                     status, total, response_complete ? 1 : 0);
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
        const esp_err_t save_result = save_to_nvs();
        if (save_result != ESP_OK) {
            ESP_LOGW(TAG,
                     "CRL is active in RAM but was not persisted; refresh will retry later");
            goto done;
        }

        ESP_LOGI(TAG, "CRL refreshed: %d revoked fingerprint(s)", cnt);
        ok = true;
    }

done:
    if (client) esp_http_client_cleanup(client);
    free(buf);
    if (net_mutex_held) xSemaphoreGive(g_net_fetch_mutex);
    return ok;
}

static bool crl_wait_or_stop(TickType_t delay)
{
    if (s_stop_requested.load(std::memory_order_acquire)) return true;
    (void)ulTaskNotifyTake(pdTRUE, delay);
    return s_stop_requested.load(std::memory_order_acquire);
}

static void crl_refresh_task(void *)
{
    // Keep the task/stack alive when start() races with a cooperative stop.
    // Publishing NULL and creating a replacement would otherwise leave a gap
    // in which the stop-timeout recovery path can permanently miss its start.
    for (;;) {
        // Every long sleep is notification-driven so OTA preparation can wake
        // the task immediately. If a refresh is already inside
        // esp_http_client, its configured timeout is allowed to expire and the
        // worker cleans up itself.
        if (!crl_wait_or_stop(pdMS_TO_TICKS(60000))) {
            supporter_crl_refresh();
        }

        // Refresh roughly every 6 h. pdMS_TO_TICKS overflows for large values
        // on high-tick-rate chips, so loop in 1 h chunks (see CLAUDE.md note).
        while (!s_stop_requested.load(std::memory_order_acquire)) {
            bool stopping = false;
            for (int i = 0; i < 6; i++) {
                if (crl_wait_or_stop(pdMS_TO_TICKS(60 * 60 * 1000))) {
                    stopping = true;
                    break;
                }
            }
            if (stopping) break;
            supporter_crl_refresh();
        }

        SemaphoreHandle_t lifecycle = crl_lifecycle_mutex();
        xSemaphoreTake(lifecycle, portMAX_DELAY);
        const bool restart =
            s_restart_requested.exchange(false, std::memory_order_acq_rel);
        if (restart) {
            s_stop_requested.store(false, std::memory_order_release);
        } else {
            s_refresh_task.store(NULL, std::memory_order_release);
        }
        xSemaphoreGive(lifecycle);

        if (!restart) break;

        // Consume the notification belonging to the superseded stop so the
        // restarted service observes its documented 60-second initial delay.
        (void)ulTaskNotifyTake(pdTRUE, 0);
        ESP_LOGI(TAG, "CRL refresh task restarted in place");
    }

    ESP_LOGI(TAG, "CRL refresh task stopped");
    vTaskDelete(NULL);
}

void supporter_crl_init()
{
    SemaphoreHandle_t lifecycle = crl_lifecycle_mutex();
    if (!lifecycle) {
        ESP_LOGE(TAG, "Failed to create CRL lifecycle mutex");
        return;
    }
    xSemaphoreTake(lifecycle, portMAX_DELAY);

    if (!s_mutex) s_mutex = xSemaphoreCreateMutexStatic(&s_data_mutex_buffer);
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create CRL data mutex");
        xSemaphoreGive(lifecycle);
        return;
    }

    // Initialise PSA exactly once from the lifecycle-serialised boot path.
    // Readers use an acquire load and fail closed until success is published.
    if (!s_psa_ready.load(std::memory_order_acquire)) {
        const psa_status_t init_status = psa_crypto_init();
        if (init_status == PSA_SUCCESS) {
            s_psa_ready.store(true, std::memory_order_release);
        } else {
            ESP_LOGE(TAG, "PSA Crypto init failed: status=%d",
                     (int)init_status);
        }
    }

    load_from_nvs();
    xSemaphoreGive(lifecycle);
}

void supporter_crl_start_refresh_task()
{
    SemaphoreHandle_t lifecycle = crl_lifecycle_mutex();
    if (!lifecycle) {
        ESP_LOGE(TAG, "Failed to create CRL lifecycle mutex");
        return;
    }
    xSemaphoreTake(lifecycle, portMAX_DELAY);

    TaskHandle_t existing = s_refresh_task.load(std::memory_order_acquire);
    if (existing != NULL) {
        if (s_stop_requested.load(std::memory_order_acquire)) {
            s_restart_requested.store(true, std::memory_order_release);
            ESP_LOGI(TAG, "CRL refresh task restart requested while stopping");
        }
        xSemaphoreGive(lifecycle);
        return;
    }

    s_restart_requested.store(false, std::memory_order_release);
    s_stop_requested.store(false, std::memory_order_release);
    TaskHandle_t task = NULL;
    {
        // 8 KB matches the other TLS-doing tasks (events, syslog, ext_proxy)
        // and leaves comfortable headroom for the mbedTLS handshake inside
        // esp_http_client_open. 4 KB was too tight and crashed at the first
        // 60-second refresh once heap was loaded with the boot-time handshake.
        if (xTaskCreate(crl_refresh_task, "crl_refresh", 8192, NULL, 2, &task) != pdPASS)
        {
            s_stop_requested.store(false, std::memory_order_release);
            ESP_LOGE(TAG, "Failed to create CRL refresh task");
        }
        else
        {
            s_refresh_task.store(task, std::memory_order_release);
            ESP_LOGI(TAG, "CRL refresh task started");
        }
    }
    xSemaphoreGive(lifecycle);
}

esp_err_t supporter_crl_stop_refresh_task()
{
    SemaphoreHandle_t lifecycle = crl_lifecycle_mutex();
    if (!lifecycle) return ESP_ERR_NO_MEM;

    xSemaphoreTake(lifecycle, portMAX_DELAY);
    // A stop issued after a recovery start request is authoritative. Clearing
    // this under the lifecycle mutex prevents the worker from reviving itself.
    s_restart_requested.store(false, std::memory_order_release);
    TaskHandle_t task = s_refresh_task.load(std::memory_order_acquire);
    if (task == NULL) {
        s_stop_requested.store(false, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        return ESP_OK;
    }

    s_stop_requested.store(true, std::memory_order_release);
    xTaskNotifyGive(task);
    xSemaphoreGive(lifecycle);

    // One refresh, including its mutex wait, has a 15 s absolute budget. One
    // extra second covers parser/NVS cleanup and scheduler latency. Never
    // delete a task which may own the mutex or mbedTLS/HTTP resources.
    for (int i = 0; i < (CRL_REFRESH_TOTAL_TIMEOUT_MS / 100) + 10 &&
         s_refresh_task.load(std::memory_order_acquire) != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_refresh_task.load(std::memory_order_acquire) != NULL) {
        ESP_LOGW(TAG, "CRL refresh task still stopping after timeout");
        return ESP_ERR_TIMEOUT;
    } else {
        // A self-deleted task's stack is reclaimed by the idle task.
        vTaskDelay(1);
        ESP_LOGI(TAG, "CRL refresh task stopped (freed ~8 KB task stack for OTA)");
    }
    return ESP_OK;
}
