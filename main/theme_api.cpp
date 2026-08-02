#include "theme_api.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_storage_lock.h"

#include "security_headers.h"

extern esp_err_t validate_auth(httpd_req_t *req);

namespace
{
constexpr const char *TAG = "ThemeAPI";
constexpr const char *NVS_NAMESPACE = "ui_theme";
constexpr const char *NVS_SCHEME_KEY = "scheme";
constexpr const char *NVS_COLOR_KEY = "primary";
constexpr const char *NVS_CONFIG_KEY = "config";
constexpr const char *DEFAULT_SCHEME = "system";
constexpr const char *DEFAULT_COLOR = "#f26a3d";
constexpr uint32_t THEME_CONFIG_MAGIC = 0x314d4854U; // "THM1" on ESP32.
constexpr uint16_t THEME_CONFIG_VERSION = 1;

struct ThemeConfigBlob {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    char scheme[8];
    char color[8];
    uint32_t checksum;
};

static_assert(sizeof(ThemeConfigBlob) == 28,
              "Theme NVS format must remain stable");

uint32_t checksum_bytes(const void *data, size_t size)
{
    // FNV-1a catches truncated, partially overwritten, or otherwise malformed
    // payloads in addition to the per-entry CRC already provided by NVS.
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t checksum = 2166136261U;
    for (size_t index = 0; index < size; ++index)
    {
        checksum ^= bytes[index];
        checksum *= 16777619U;
    }
    return checksum;
}

uint32_t config_checksum(const ThemeConfigBlob &config)
{
    return checksum_bytes(&config,
                          sizeof(config) - sizeof(config.checksum));
}

bool valid_scheme(const char *value)
{
    return value &&
           (strcmp(value, "system") == 0 ||
            strcmp(value, "light") == 0 ||
            strcmp(value, "dark") == 0);
}

bool valid_color(const char *value)
{
    if (!value || strlen(value) != 7 || value[0] != '#') return false;
    for (size_t index = 1; index < 7; ++index)
    {
        if (!isxdigit(static_cast<unsigned char>(value[index]))) return false;
    }
    return true;
}

bool valid_blob(const ThemeConfigBlob &config)
{
    return config.magic == THEME_CONFIG_MAGIC &&
           config.version == THEME_CONFIG_VERSION &&
           config.size == sizeof(config) &&
           config.scheme[sizeof(config.scheme) - 1] == '\0' &&
           config.color[sizeof(config.color) - 1] == '\0' &&
           valid_scheme(config.scheme) &&
           valid_color(config.color) &&
           config.checksum == config_checksum(config);
}

void set_default_theme(char *scheme, size_t scheme_size,
                       char *color, size_t color_size)
{
    snprintf(scheme, scheme_size, "%s", DEFAULT_SCHEME);
    snprintf(color, color_size, "%s", DEFAULT_COLOR);
}

void load_legacy_theme(nvs_handle_t handle,
                       char *scheme, size_t scheme_size,
                       char *color, size_t color_size)
{
    char legacy_scheme[8] = {};
    char legacy_color[8] = {};
    size_t stored_scheme_size = sizeof(legacy_scheme);
    size_t stored_color_size = sizeof(legacy_color);

    const esp_err_t scheme_result = nvs_get_str(
        handle, NVS_SCHEME_KEY, legacy_scheme, &stored_scheme_size);
    const esp_err_t color_result = nvs_get_str(
        handle, NVS_COLOR_KEY, legacy_color, &stored_color_size);

    // Treat the two legacy entries as one logical value. Accepting just one
    // would recreate the split state that the blob format is meant to avoid.
    if (scheme_result == ESP_OK && color_result == ESP_OK &&
        legacy_scheme[sizeof(legacy_scheme) - 1] == '\0' &&
        legacy_color[sizeof(legacy_color) - 1] == '\0' &&
        valid_scheme(legacy_scheme) && valid_color(legacy_color))
    {
        snprintf(scheme, scheme_size, "%s", legacy_scheme);
        snprintf(color, color_size, "%s", legacy_color);
    }
}

void load_theme(char *scheme, size_t scheme_size,
                char *color, size_t color_size)
{
    set_default_theme(scheme, scheme_size, color, color_size);

    NvsStorageLock storage_lock;
    if (!storage_lock)
    {
        ESP_LOGE(TAG, "Could not acquire NVS storage lock while loading theme");
        return;
    }

    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;

    size_t stored_blob_size = 0;
    const esp_err_t blob_size_result = nvs_get_blob(
        handle, NVS_CONFIG_KEY, nullptr, &stored_blob_size);
    if (blob_size_result == ESP_OK)
    {
        ThemeConfigBlob config = {};
        if (stored_blob_size == sizeof(config))
        {
            size_t read_size = sizeof(config);
            const esp_err_t read_result = nvs_get_blob(
                handle, NVS_CONFIG_KEY, &config, &read_size);
            if (read_result == ESP_OK && read_size == sizeof(config) &&
                valid_blob(config))
            {
                snprintf(scheme, scheme_size, "%s", config.scheme);
                snprintf(color, color_size, "%s", config.color);
                nvs_close(handle);
                return;
            }
        }

        // Do not resurrect stale legacy entries after a newer-format record is
        // detected but fails validation. Defaults are the safe deterministic
        // response to a corrupt or unknown record.
        ESP_LOGW(TAG, "Ignoring invalid persisted theme blob");
        nvs_close(handle);
        return;
    }

    if (blob_size_result == ESP_ERR_NVS_NOT_FOUND)
    {
        load_legacy_theme(handle, scheme, scheme_size, color, color_size);
    }
    else
    {
        ESP_LOGW(TAG, "Could not read persisted theme blob: %s",
                 esp_err_to_name(blob_size_result));
    }
    nvs_close(handle);
}

esp_err_t normalize_erase_result(esp_err_t result)
{
    return result == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : result;
}

esp_err_t send_theme(httpd_req_t *req)
{
    char scheme[8] = {};
    char color[8] = {};
    load_theme(scheme, sizeof(scheme), color, sizeof(color));

    char response[96];
    snprintf(response, sizeof(response),
             "{\"colorScheme\":\"%s\",\"primaryColor\":\"%s\"}",
             scheme, color);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t get_theme(httpd_req_t *req)
{
    add_security_headers(req);
    return send_theme(req);
}

esp_err_t post_theme(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, nullptr);
    }
    if (req->content_len <= 0 || req->content_len >= 256)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Invalid theme payload size");
    }

    char body[256] = {};
    size_t received = 0;
    while (received < static_cast<size_t>(req->content_len))
    {
        const int count = httpd_req_recv(req, body + received,
                                         req->content_len - received);
        if (count <= 0)
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Could not read theme payload");
        }
        received += static_cast<size_t>(count);
    }
    body[received] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    const cJSON *scheme_item = cJSON_GetObjectItemCaseSensitive(root,
                                                                "colorScheme");
    const cJSON *color_item = cJSON_GetObjectItemCaseSensitive(root,
                                                               "primaryColor");
    const char *scheme = cJSON_IsString(scheme_item)
        ? scheme_item->valuestring
        : nullptr;
    const char *color = cJSON_IsString(color_item)
        ? color_item->valuestring
        : nullptr;

    if (!valid_scheme(scheme) || !valid_color(color))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Invalid theme values");
    }

    esp_err_t result = theme_api_set_config(scheme, color);
    cJSON_Delete(root);

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not save theme: %s", esp_err_to_name(result));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Could not save theme");
    }

    return send_theme(req);
}

httpd_uri_t get_theme_uri = {
    .uri = "/api/theme",
    .method = HTTP_GET,
    .handler = get_theme,
    .user_ctx = nullptr,
};

httpd_uri_t post_theme_uri = {
    .uri = "/api/theme",
    .method = HTTP_POST,
    .handler = post_theme,
    .user_ctx = nullptr,
};
} // namespace

esp_err_t theme_api_get_config(char *scheme, size_t scheme_size,
                               char *color, size_t color_size)
{
    if (!scheme || scheme_size < 8 || !color || color_size < 8)
    {
        return ESP_ERR_INVALID_ARG;
    }
    load_theme(scheme, scheme_size, color, color_size);
    return ESP_OK;
}

esp_err_t theme_api_set_config(const char *scheme, const char *color)
{
    if (!valid_scheme(scheme) || !valid_color(color))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ThemeConfigBlob config = {};
    config.magic = THEME_CONFIG_MAGIC;
    config.version = THEME_CONFIG_VERSION;
    config.size = static_cast<uint16_t>(sizeof(config));
    snprintf(config.scheme, sizeof(config.scheme), "%s", scheme);
    snprintf(config.color, sizeof(config.color), "%s", color);
    config.checksum = config_checksum(config);

    NvsStorageLock storage_lock;
    if (!storage_lock) return ESP_ERR_NO_MEM;

    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result == ESP_OK)
    {
        result = nvs_set_blob(handle, NVS_CONFIG_KEY, &config, sizeof(config));
    }
    if (result == ESP_OK) result = nvs_commit(handle);

    // The new blob is committed before legacy cleanup. Consequently, even if
    // either erase or the cleanup commit fails, readers still observe the
    // complete new pair. Keep returning cleanup errors so callers can report
    // persistence maintenance failures and retry later.
    if (result == ESP_OK)
    {
        const esp_err_t scheme_erase = nvs_erase_key(handle,
                                                      NVS_SCHEME_KEY);
        const esp_err_t color_erase = nvs_erase_key(handle,
                                                    NVS_COLOR_KEY);
        const esp_err_t scheme_cleanup = normalize_erase_result(scheme_erase);
        const esp_err_t color_cleanup = normalize_erase_result(color_erase);
        const bool cleanup_changed = scheme_erase == ESP_OK ||
                                     color_erase == ESP_OK;

        if (scheme_cleanup != ESP_OK)
        {
            result = scheme_cleanup;
        }
        else if (color_cleanup != ESP_OK)
        {
            result = color_cleanup;
        }
        else if (cleanup_changed)
        {
            result = nvs_commit(handle);
        }
    }
    if (handle) nvs_close(handle);
    return result;
}

esp_err_t theme_api_register(httpd_handle_t server)
{
    if (!server) return ESP_ERR_INVALID_ARG;
    const esp_err_t get_result = httpd_register_uri_handler(server,
                                                             &get_theme_uri);
    const esp_err_t post_result = httpd_register_uri_handler(server,
                                                              &post_theme_uri);
    if (get_result != ESP_OK || post_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not register theme API: GET=%s POST=%s",
                 esp_err_to_name(get_result), esp_err_to_name(post_result));
        return get_result != ESP_OK ? get_result : post_result;
    }
    return ESP_OK;
}
