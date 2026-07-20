#include "webui_storage.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/md.h"
#include "nvs.h"

#include "monitoring.h"
#include "security_headers.h"

// Defined in webui.cpp. Both update paths deliberately share the existing
// constant-time admin-token validation.
extern esp_err_t validate_auth(httpd_req_t *req);

namespace
{
constexpr const char *TAG = "WebUIStorage";
constexpr const char *PARTITION_LABEL = "spiffs";
#define BASE_PATH "/www"
constexpr size_t SHA256_HEX_LENGTH = 64;
constexpr size_t UPDATE_BUFFER_SIZE = 2048;
constexpr size_t INVALIDATE_SECTOR_SIZE = 0x1000;
constexpr size_t MANIFEST_MAX_SIZE = 1024;
constexpr const char *TRANSACTION_NVS_NAMESPACE = "webui";
constexpr const char *TRANSACTION_NVS_KEY = "pending";

SemaphoreHandle_t s_mutex = nullptr;
const esp_partition_t *s_partition = nullptr;
WebUIStorageStatus s_status = {};
size_t s_expected_size = 0;
char s_expected_sha256[SHA256_HEX_LENGTH + 1] = {};
mbedtls_md_context_t s_sha_context;
bool s_sha_ready = false;
httpd_handle_t s_registered_server = nullptr;

bool ensure_mutex()
{
    if (s_mutex) return true;
    s_mutex = xSemaphoreCreateMutex();
    return s_mutex != nullptr;
}

class StorageLock
{
public:
    StorageLock()
        : locked(ensure_mutex() &&
                 xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE)
    {
    }

    ~StorageLock()
    {
        if (locked) xSemaphoreGive(s_mutex);
    }

    explicit operator bool() const { return locked; }

private:
    bool locked;
};

void copy_safe(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0) return;
    destination[0] = '\0';
    if (!source) return;
    snprintf(destination, destination_size, "%s", source);
}

void copy_json_safe(char *destination, size_t destination_size,
                    const char *source)
{
    if (!destination || destination_size == 0) return;
    destination[0] = '\0';
    if (!source) return;

    size_t out = 0;
    for (size_t in = 0; source[in] && out + 1 < destination_size; ++in)
    {
        const unsigned char value = static_cast<unsigned char>(source[in]);
        if (value < 0x20 || value == '"' || value == '\\')
        {
            destination[out++] = '_';
        }
        else
        {
            destination[out++] = static_cast<char>(value);
        }
    }
    destination[out] = '\0';
}

void clear_error_locked()
{
    s_status.lastError[0] = '\0';
}

void set_error_locked(const char *message, esp_err_t error = ESP_OK)
{
    // Copy first so callers may safely pass s_status.lastError itself.
    char local_message[sizeof(s_status.lastError)] = {};
    copy_safe(local_message, sizeof(local_message), message ? message : "error");

    if (error == ESP_OK)
    {
        copy_safe(s_status.lastError, sizeof(s_status.lastError), local_message);
    }
    else
    {
        snprintf(s_status.lastError, sizeof(s_status.lastError), "%s: %s",
                 local_message, esp_err_to_name(error));
    }
    ESP_LOGE(TAG, "%s", s_status.lastError);
}

esp_err_t set_transaction_pending_locked(bool pending)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(TRANSACTION_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;

    if (pending)
    {
        result = nvs_set_u8(handle, TRANSACTION_NVS_KEY, 1);
    }
    else
    {
        result = nvs_erase_key(handle, TRANSACTION_NVS_KEY);
        if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    }

    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

bool transaction_pending_locked()
{
    nvs_handle_t handle = 0;
    if (nvs_open(TRANSACTION_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    {
        return false;
    }

    uint8_t value = 0;
    const esp_err_t result = nvs_get_u8(handle, TRANSACTION_NVS_KEY, &value);
    nvs_close(handle);
    return result == ESP_OK && value == 1;
}

bool is_regular_nonempty_file(const char *path)
{
    struct stat file_info = {};
    return stat(path, &file_info) == 0 &&
           S_ISREG(file_info.st_mode) && file_info.st_size > 0;
}

bool validate_manifest_locked()
{
    s_status.version[0] = '\0';

    FILE *file = fopen(BASE_PATH "/webui-manifest.json", "rb");
    if (!file) return false;

    char manifest[MANIFEST_MAX_SIZE + 1] = {};
    const size_t length = fread(manifest, 1, MANIFEST_MAX_SIZE, file);
    const bool too_large =
        length == MANIFEST_MAX_SIZE && fgetc(file) != EOF;
    const bool read_error = ferror(file);
    fclose(file);

    if (read_error || too_large || length == 0) return false;
    manifest[length] = '\0';

    cJSON *root = cJSON_Parse(manifest);
    if (!root) return false;

    const cJSON *format = cJSON_GetObjectItemCaseSensitive(root, "format");
    const cJSON *product = cJSON_GetObjectItemCaseSensitive(root, "product");
    const cJSON *design = cJSON_GetObjectItemCaseSensitive(root, "design");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *encodings = cJSON_GetObjectItemCaseSensitive(root, "encodings");
    const cJSON *js_encoding = encodings
        ? cJSON_GetObjectItemCaseSensitive(encodings, "main.js")
        : nullptr;
    const cJSON *css_encoding = encodings
        ? cJSON_GetObjectItemCaseSensitive(encodings, "main.css")
        : nullptr;

    const bool valid =
        cJSON_IsNumber(format) && format->valueint == 1 &&
        cJSON_IsString(product) && product->valuestring &&
        strcmp(product->valuestring, "HB-RF-ETH-ng") == 0 &&
        cJSON_IsString(design) && design->valuestring &&
        strcmp(design->valuestring, "newdesign") == 0 &&
        cJSON_IsString(version) && version->valuestring &&
        version->valuestring[0] != '\0' &&
        cJSON_IsString(js_encoding) && js_encoding->valuestring &&
        strcmp(js_encoding->valuestring, "gzip") == 0 &&
        cJSON_IsString(css_encoding) && css_encoding->valuestring &&
        strcmp(css_encoding->valuestring, "gzip") == 0;

    if (valid)
    {
        copy_json_safe(s_status.version, sizeof(s_status.version),
                       version->valuestring);
    }

    cJSON_Delete(root);
    return valid && s_status.version[0] != '\0';
}

bool validate_files_locked()
{
    if (!s_status.mounted)
    {
        s_status.valid = false;
        return false;
    }

    // Only the New Design is accepted. All standalone assets use gzip.
    const bool valid =
        is_regular_nonempty_file(BASE_PATH "/index.html.gz") &&
        is_regular_nonempty_file(BASE_PATH "/main.js.gz") &&
        is_regular_nonempty_file(BASE_PATH "/main.css.gz") &&
        is_regular_nonempty_file(BASE_PATH "/webui-manifest.json") &&
        validate_manifest_locked();

    s_status.valid = valid;
    if (!valid) s_status.version[0] = '\0';
    return valid;
}

void update_usage_locked()
{
    s_status.totalBytes = 0;
    s_status.usedBytes = 0;
    if (!s_status.mounted) return;

    size_t total = 0;
    size_t used = 0;
    if (esp_spiffs_info(PARTITION_LABEL, &total, &used) == ESP_OK)
    {
        s_status.totalBytes = total;
        s_status.usedBytes = used;
    }
}

void find_partition_locked()
{
    if (!s_partition)
    {
        s_partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
            PARTITION_LABEL);
    }

    s_status.partitionFound = s_partition != nullptr;
    s_status.partitionSize = s_partition ? s_partition->size : 0;
}

void unmount_locked()
{
    if (esp_spiffs_mounted(PARTITION_LABEL))
    {
        esp_vfs_spiffs_unregister(PARTITION_LABEL);
    }

    s_status.mounted = false;
    s_status.valid = false;
    s_status.totalBytes = 0;
    s_status.usedBytes = 0;
    s_status.version[0] = '\0';
}

void sha_cleanup_locked()
{
    if (!s_sha_ready) return;
    mbedtls_md_free(&s_sha_context);
    s_sha_ready = false;
}

void invalidate_image_locked(const char *message,
                             esp_err_t original_error = ESP_OK)
{
    char preserved_message[sizeof(s_status.lastError)] = {};
    copy_safe(preserved_message, sizeof(preserved_message), message);

    sha_cleanup_locked();
    s_status.updateActive = false;
    unmount_locked();

    // Destroy the filesystem header so a partial/hash-mismatched image can
    // never become active after reboot. Embedded New Design remains reachable.
    esp_err_t erase_result = ESP_ERR_NOT_FOUND;
    if (s_partition)
    {
        erase_result = esp_partition_erase_range(
            s_partition, 0, INVALIDATE_SECTOR_SIZE);
        if (erase_result != ESP_OK)
        {
            ESP_LOGW(TAG, "Could not invalidate failed WWW image: %s",
                     esp_err_to_name(erase_result));
        }
    }

    // Clear the persistent marker only after the filesystem header was actually
    // invalidated. Otherwise the next boot retries the cleanup before mounting.
    if (erase_result == ESP_OK)
    {
        const esp_err_t marker_result = set_transaction_pending_locked(false);
        if (marker_result != ESP_OK)
        {
            ESP_LOGW(TAG, "Could not clear WWW transaction marker: %s",
                     esp_err_to_name(marker_result));
        }
    }

    set_error_locked(preserved_message[0] ? preserved_message : "WWW update failed",
                     original_error);
}

esp_err_t mount_locked()
{
    find_partition_locked();

    s_status.mounted = false;
    s_status.valid = false;
    s_status.totalBytes = 0;
    s_status.usedBytes = 0;
    s_status.version[0] = '\0';

    if (!s_partition)
    {
        set_error_locked("SPIFFS partition not found");
        return ESP_ERR_NOT_FOUND;
    }

    if (esp_spiffs_mounted(PARTITION_LABEL))
    {
        s_status.mounted = true;
        update_usage_locked();
        validate_files_locked();
        return ESP_OK;
    }

    const esp_vfs_spiffs_conf_t config = {
        .base_path = BASE_PATH,
        .partition_label = PARTITION_LABEL,
        .max_files = 10,
        .format_if_mount_failed = false,
    };

    const esp_err_t result = esp_vfs_spiffs_register(&config);
    if (result != ESP_OK)
    {
        // Empty/unformatted is expected immediately after installing the
        // transition firmware. Never auto-format during boot.
        if (result == ESP_FAIL)
        {
            copy_safe(s_status.lastError, sizeof(s_status.lastError),
                      "Standalone New Design not installed");
            ESP_LOGI(TAG, "%s; using embedded New Design",
                     s_status.lastError);
        }
        else
        {
            set_error_locked("SPIFFS mount failed", result);
        }
        return result;
    }

    s_status.mounted = true;
    clear_error_locked();
    update_usage_locked();
    validate_files_locked();

    if (!s_status.valid)
    {
        copy_safe(s_status.lastError, sizeof(s_status.lastError),
                  "No valid New Design manifest found");
    }

    ESP_LOGI(TAG,
             "External New Design mounted: valid=%d, used=%u/%u bytes, version=%s",
             s_status.valid ? 1 : 0,
             static_cast<unsigned>(s_status.usedBytes),
             static_cast<unsigned>(s_status.totalBytes),
             s_status.version[0] ? s_status.version : "unknown");
    return ESP_OK;
}

bool valid_sha256_hex(const char *value)
{
    if (!value || value[0] == '\0') return true;
    if (strlen(value) != SHA256_HEX_LENGTH) return false;
    for (size_t index = 0; index < SHA256_HEX_LENGTH; ++index)
    {
        if (!isxdigit(static_cast<unsigned char>(value[index]))) return false;
    }
    return true;
}

void digest_to_hex(const unsigned char digest[32],
                   char output[SHA256_HEX_LENGTH + 1])
{
    static const char HEX[] = "0123456789abcdef";
    for (size_t index = 0; index < 32; ++index)
    {
        output[index * 2] = HEX[(digest[index] >> 4) & 0x0F];
        output[index * 2 + 1] = HEX[digest[index] & 0x0F];
    }
    output[SHA256_HEX_LENGTH] = '\0';
}

struct AssetSpec
{
    const char *uri;
    const char *path;
    const char *content_type;
    const char *content_encoding;
};

constexpr AssetSpec ASSET_SPECS[] = {
    {"/main.js", BASE_PATH "/main.js.gz", "application/javascript", "gzip"},
    {"/main.css", BASE_PATH "/main.css.gz", "text/css", "gzip"},
    {"/favicon.ico", BASE_PATH "/favicon.ico.gz", "image/x-icon", "gzip"},
    {"/manifest.webmanifest", BASE_PATH "/manifest.webmanifest.gz", "application/manifest+json", "gzip"},
    {"/icon-256.png", BASE_PATH "/icon-256.png.gz", "image/png", "gzip"},
    {"/*", BASE_PATH "/index.html.gz", "text/html", "gzip"},
};

const AssetSpec *find_asset_spec(const char *uri)
{
    if (!uri) return nullptr;
    for (const auto &spec : ASSET_SPECS)
    {
        if (strcmp(uri, spec.uri) == 0) return &spec;
    }
    return nullptr;
}

struct WrappedRoute
{
    bool used = false;
    httpd_uri_t replacement = {};
    httpd_uri_func original_handler = nullptr;
    const AssetSpec *asset = nullptr;
};

WrappedRoute s_wrapped_routes[10];

WrappedRoute *allocate_route()
{
    for (auto &route : s_wrapped_routes)
    {
        if (!route.used)
        {
            route.used = true;
            return &route;
        }
    }
    return nullptr;
}

esp_err_t stream_external_file(httpd_req_t *req, const AssetSpec *asset)
{
    if (!asset) return ESP_ERR_INVALID_ARG;

    FILE *file = fopen(asset->path, "rb");
    if (!file) return ESP_ERR_NOT_FOUND;

    add_security_headers(req);
    httpd_resp_set_type(req, asset->content_type);
    httpd_resp_set_hdr(req, "Content-Encoding", asset->content_encoding);
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");

    char buffer[2048];
    esp_err_t result = ESP_OK;
    while (!feof(file))
    {
        const size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count > 0)
        {
            result = httpd_resp_send_chunk(req, buffer, count);
            if (result != ESP_OK) break;
        }
        if (ferror(file))
        {
            result = ESP_FAIL;
            break;
        }
    }

    fclose(file);
    if (result == ESP_OK)
    {
        result = httpd_resp_send_chunk(req, nullptr, 0);
    }
    return result;
}

esp_err_t wrapped_asset_handler(httpd_req_t *req)
{
    auto *route = static_cast<WrappedRoute *>(req->user_ctx);
    if (!route || !route->original_handler) return ESP_FAIL;

    if (webui_storage_is_valid())
    {
        const esp_err_t result = stream_external_file(req, route->asset);
        if (result != ESP_ERR_NOT_FOUND) return result;

        // Optional assets intentionally remain embedded.
        ESP_LOGD(TAG, "Using embedded fallback asset for %s",
                 route->asset ? route->asset->uri : "unknown");
    }

    return route->original_handler(req);
}

esp_err_t wrapped_firmware_update_handler(httpd_req_t *req)
{
    auto *route = static_cast<WrappedRoute *>(req->user_ctx);
    if (!route || !route->original_handler) return ESP_FAIL;

    if (webui_storage_get_status().updateActive)
    {
        add_security_headers(req);
        return httpd_resp_send_custom_err(req, "409 Conflict",
                                          "WWW update already in progress");
    }
    return route->original_handler(req);
}

esp_err_t get_webui_status_handler(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, nullptr);
    }

    const WebUIStorageStatus status = webui_storage_get_status();
    char safe_version[sizeof(status.version)] = {};
    char safe_error[sizeof(status.lastError)] = {};
    copy_json_safe(safe_version, sizeof(safe_version), status.version);
    copy_json_safe(safe_error, sizeof(safe_error), status.lastError);

    char response[512];
    snprintf(response, sizeof(response),
             "{\"partitionFound\":%s,\"mounted\":%s,\"valid\":%s,"
             "\"updateActive\":%s,\"partitionSize\":%u,\"totalBytes\":%u,"
             "\"usedBytes\":%u,\"bytesWritten\":%u,\"version\":\"%s\","
             "\"design\":\"newdesign\",\"lastError\":\"%s\"}",
             status.partitionFound ? "true" : "false",
             status.mounted ? "true" : "false",
             status.valid ? "true" : "false",
             status.updateActive ? "true" : "false",
             static_cast<unsigned>(status.partitionSize),
             static_cast<unsigned>(status.totalBytes),
             static_cast<unsigned>(status.usedBytes),
             static_cast<unsigned>(status.bytesWritten),
             safe_version,
             safe_error);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t post_webui_update_handler(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, nullptr);
    }
    if (req->content_len <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Empty WWW image");
    }

    char content_type[64] = {};
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                    sizeof(content_type)) == ESP_OK &&
        strstr(content_type, "multipart/form-data") != nullptr)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Send raw spiffs.bin as request body");
    }

    if (net_fetch_ota_active())
    {
        return httpd_resp_send_custom_err(req, "409 Conflict",
                                          "Firmware OTA already in progress");
    }

    bool net_locked = false;
    if (g_net_fetch_mutex)
    {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            return httpd_resp_send_custom_err(req, "503 Service Unavailable",
                                              "Update subsystem busy");
        }
        net_locked = true;
    }

    char expected_sha256[SHA256_HEX_LENGTH + 1] = {};
    if (httpd_req_get_hdr_value_str(req, "X-WebUI-SHA256", expected_sha256,
                                    sizeof(expected_sha256)) != ESP_OK)
    {
        expected_sha256[0] = '\0';
    }

    esp_err_t result = webui_storage_update_begin(
        static_cast<size_t>(req->content_len), expected_sha256);
    if (result != ESP_OK)
    {
        if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
        const WebUIStorageStatus status = webui_storage_get_status();
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   status.lastError[0]
                                       ? status.lastError
                                       : "Could not start WWW update");
    }

    uint8_t *buffer = static_cast<uint8_t *>(malloc(UPDATE_BUFFER_SIZE));
    if (!buffer)
    {
        webui_storage_update_abort();
        if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Could not allocate WWW upload buffer");
    }

    size_t received = 0;
    int timeout_retries = 5;

    while (received < static_cast<size_t>(req->content_len))
    {
        const size_t remaining =
            static_cast<size_t>(req->content_len) - received;
        const size_t wanted = remaining < UPDATE_BUFFER_SIZE
            ? remaining
            : UPDATE_BUFFER_SIZE;
        const int count = httpd_req_recv(req,
                                         reinterpret_cast<char *>(buffer),
                                         wanted);

        if (count == HTTPD_SOCK_ERR_TIMEOUT && timeout_retries-- > 0)
        {
            continue;
        }
        if (count <= 0)
        {
            result = ESP_FAIL;
            break;
        }

        timeout_retries = 5;
        result = webui_storage_update_write(buffer,
                                            static_cast<size_t>(count));
        if (result != ESP_OK) break;
        received += static_cast<size_t>(count);

        if ((received & 0x3FFFU) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    free(buffer);

    if (result == ESP_OK &&
        received == static_cast<size_t>(req->content_len))
    {
        result = webui_storage_update_finish();
    }
    else
    {
        webui_storage_update_abort();
    }

    if (net_locked) xSemaphoreGive(g_net_fetch_mutex);

    if (result != ESP_OK)
    {
        const WebUIStorageStatus status = webui_storage_get_status();
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   status.lastError[0]
                                       ? status.lastError
                                       : "WWW update failed");
    }

    const WebUIStorageStatus status = webui_storage_get_status();
    char safe_version[sizeof(status.version)] = {};
    copy_json_safe(safe_version, sizeof(safe_version), status.version);

    char response[192];
    snprintf(response, sizeof(response),
             "{\"success\":true,\"reload\":true,\"design\":\"newdesign\","
             "\"version\":\"%s\"}",
             safe_version);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

httpd_uri_t s_webui_status_uri = {
    .uri = "/api/webui/status",
    .method = HTTP_GET,
    .handler = get_webui_status_handler,
    .user_ctx = nullptr,
};

httpd_uri_t s_webui_update_uri = {
    .uri = "/api/webui/update",
    .method = HTTP_POST,
    .handler = post_webui_update_handler,
    .user_ctx = nullptr,
};

void register_storage_endpoints_once(httpd_handle_t server)
{
    if (s_registered_server == server) return;

    // Mark first to avoid recursive duplicate registration through the shim.
    s_registered_server = server;
    for (auto &route : s_wrapped_routes)
    {
        route = WrappedRoute{};
    }
    webui_storage_init();

    const esp_err_t status_result =
        httpd_register_uri_handler(server, &s_webui_status_uri);
    const esp_err_t update_result =
        httpd_register_uri_handler(server, &s_webui_update_uri);

    if (status_result != ESP_OK || update_result != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "Could not register WWW update endpoints: status=%s update=%s",
                 esp_err_to_name(status_result),
                 esp_err_to_name(update_result));
    }
}

} // namespace

esp_err_t webui_storage_init()
{
    StorageLock lock;
    if (!lock) return ESP_ERR_NO_MEM;
    if (s_status.updateActive) return ESP_ERR_INVALID_STATE;

    find_partition_locked();
    if (transaction_pending_locked())
    {
        ESP_LOGW(TAG, "Interrupted WWW update detected; discarding partial image");
        unmount_locked();
        if (!s_partition)
        {
            set_error_locked("Interrupted WWW update but SPIFFS partition is missing");
            return ESP_ERR_NOT_FOUND;
        }

        const esp_err_t erase_result = esp_partition_erase_range(
            s_partition, 0, INVALIDATE_SECTOR_SIZE);
        if (erase_result != ESP_OK)
        {
            set_error_locked("Could not discard interrupted WWW image", erase_result);
            return erase_result;
        }

        const esp_err_t marker_result = set_transaction_pending_locked(false);
        if (marker_result != ESP_OK)
        {
            set_error_locked("Could not clear interrupted WWW transaction", marker_result);
            return marker_result;
        }
    }

    return mount_locked();
}

WebUIStorageStatus webui_storage_get_status()
{
    StorageLock lock;
    if (!lock)
    {
        WebUIStorageStatus failed = {};
        copy_safe(failed.lastError, sizeof(failed.lastError),
                  "storage mutex unavailable");
        return failed;
    }
    return s_status;
}

bool webui_storage_is_valid()
{
    StorageLock lock;
    return lock && s_status.mounted && s_status.valid &&
           !s_status.updateActive;
}

esp_err_t webui_storage_update_begin(size_t expected_size,
                                     const char *expected_sha256_hex)
{
    StorageLock lock;
    if (!lock) return ESP_ERR_NO_MEM;

    find_partition_locked();
    clear_error_locked();

    if (s_status.updateActive)
    {
        set_error_locked("WWW update already active");
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_partition)
    {
        set_error_locked("SPIFFS partition not found");
        return ESP_ERR_NOT_FOUND;
    }
    // spiffs_create_partition_image() creates a full partition-sized image.
    if (expected_size != s_partition->size)
    {
        set_error_locked("Falsche Datei: erwartet wird ein 327680-Byte-WebUI-Abbild; Firmware unter System -> Firmware installieren");
        return ESP_ERR_INVALID_SIZE;
    }
    if (!valid_sha256_hex(expected_sha256_hex))
    {
        set_error_locked("Invalid WWW SHA-256 header");
        return ESP_ERR_INVALID_ARG;
    }

    // Commit the marker before erasing or writing flash. A power interruption
    // leaves it set, so the next boot rejects the unverified image.
    const esp_err_t marker_result = set_transaction_pending_locked(true);
    if (marker_result != ESP_OK)
    {
        set_error_locked("Could not start safe WWW update transaction", marker_result);
        return marker_result;
    }

    unmount_locked();
    sha_cleanup_locked();

    const esp_err_t erase_result =
        esp_partition_erase_range(s_partition, 0, s_partition->size);
    if (erase_result != ESP_OK)
    {
        invalidate_image_locked("Could not erase WWW partition", erase_result);
        return erase_result;
    }

    mbedtls_md_init(&s_sha_context);
    const mbedtls_md_info_t *sha_info =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!sha_info ||
        mbedtls_md_setup(&s_sha_context, sha_info, 0) != 0 ||
        mbedtls_md_starts(&s_sha_context) != 0)
    {
        mbedtls_md_free(&s_sha_context);
        invalidate_image_locked("Could not initialize WWW SHA-256");
        return ESP_ERR_NO_MEM;
    }

    s_sha_ready = true;
    s_expected_size = expected_size;
    s_expected_sha256[0] = '\0';
    if (expected_sha256_hex && expected_sha256_hex[0])
    {
        copy_safe(s_expected_sha256, sizeof(s_expected_sha256),
                  expected_sha256_hex);
    }

    s_status.updateActive = true;
    s_status.bytesWritten = 0;
    s_status.valid = false;
    clear_error_locked();

    ESP_LOGI(TAG, "Starting separate New Design update: %u bytes%s",
             static_cast<unsigned>(expected_size),
             s_expected_sha256[0] ? " with SHA-256 verification" : "");
    return ESP_OK;
}

esp_err_t webui_storage_update_write(const uint8_t *data, size_t length)
{
    StorageLock lock;
    if (!lock) return ESP_ERR_NO_MEM;

    if (!s_status.updateActive || !s_partition || !s_sha_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || length == 0 ||
        s_status.bytesWritten + length > s_expected_size)
    {
        set_error_locked("Invalid WWW image chunk");
        return ESP_ERR_INVALID_SIZE;
    }

    const esp_err_t write_result = esp_partition_write(
        s_partition, s_status.bytesWritten, data, length);
    if (write_result != ESP_OK)
    {
        set_error_locked("Could not write WWW partition", write_result);
        return write_result;
    }

    if (mbedtls_md_update(&s_sha_context, data, length) != 0)
    {
        set_error_locked("Could not update WWW SHA-256");
        return ESP_FAIL;
    }

    s_status.bytesWritten += length;
    return ESP_OK;
}

esp_err_t webui_storage_update_finish()
{
    StorageLock lock;
    if (!lock) return ESP_ERR_NO_MEM;

    if (!s_status.updateActive || !s_sha_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_status.bytesWritten != s_expected_size)
    {
        invalidate_image_locked("Incomplete WWW image");
        return ESP_ERR_INVALID_SIZE;
    }

    unsigned char digest[32] = {};
    if (mbedtls_md_finish(&s_sha_context, digest) != 0)
    {
        invalidate_image_locked("Could not finalize WWW SHA-256");
        return ESP_FAIL;
    }
    sha_cleanup_locked();

    char actual_sha256[SHA256_HEX_LENGTH + 1] = {};
    digest_to_hex(digest, actual_sha256);
    if (s_expected_sha256[0] &&
        strcasecmp(actual_sha256, s_expected_sha256) != 0)
    {
        invalidate_image_locked("WWW SHA-256 mismatch");
        return ESP_ERR_INVALID_CRC;
    }

    s_status.updateActive = false;
    const esp_err_t mount_result = mount_locked();
    if (mount_result != ESP_OK || !validate_files_locked())
    {
        const esp_err_t validation_error = mount_result == ESP_OK
            ? ESP_ERR_INVALID_RESPONSE
            : mount_result;
        invalidate_image_locked(
            "WWW image is not a valid HB-RF-ETH-ng New Design image",
            validation_error);
        return validation_error;
    }

    const esp_err_t marker_result = set_transaction_pending_locked(false);
    if (marker_result != ESP_OK)
    {
        invalidate_image_locked("Could not commit safe WWW update transaction", marker_result);
        return marker_result;
    }

    update_usage_locked();
    clear_error_locked();
    ESP_LOGI(TAG, "Separate New Design installed successfully, version=%s",
             s_status.version[0] ? s_status.version : "unknown");
    return ESP_OK;
}

void webui_storage_update_abort()
{
    StorageLock lock;
    if (!lock) return;

    char reason[sizeof(s_status.lastError)] = {};
    copy_safe(reason, sizeof(reason),
              s_status.lastError[0]
                  ? s_status.lastError
                  : "WWW update aborted");
    invalidate_image_locked(reason);
}

esp_err_t hb_webui_register_uri_handler(httpd_handle_t server,
                                        const httpd_uri_t *uri_handler)
{
    if (!server || !uri_handler || !uri_handler->uri)
    {
        return ESP_ERR_INVALID_ARG;
    }

    register_storage_endpoints_once(server);

    const AssetSpec *asset = find_asset_spec(uri_handler->uri);
    const bool firmware_update_route =
        strcmp(uri_handler->uri, "/ota_update") == 0 ||
        strcmp(uri_handler->uri, "/api/ota_url") == 0;

    if (!asset && !firmware_update_route)
    {
        return httpd_register_uri_handler(server, uri_handler);
    }

    WrappedRoute *route = allocate_route();
    if (!route)
    {
        ESP_LOGE(TAG, "No wrapper slot available for %s", uri_handler->uri);
        return ESP_ERR_NO_MEM;
    }

    route->replacement = *uri_handler;
    route->original_handler = uri_handler->handler;
    route->asset = asset;
    route->replacement.user_ctx = route;
    route->replacement.handler = firmware_update_route
        ? wrapped_firmware_update_handler
        : wrapped_asset_handler;

    return httpd_register_uri_handler(server, &route->replacement);
}
