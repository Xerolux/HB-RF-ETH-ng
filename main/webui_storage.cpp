#include "webui_storage.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/md.h"

#include "esp_http_server.h"
#include "monitoring.h"
#include "security_headers.h"

// Implemented in webui.cpp. Keeping authentication in one place guarantees
// that firmware and WWW updates use the same admin token and constant-time
// comparison path.
extern esp_err_t validate_auth(httpd_req_t *req);

namespace
{
constexpr const char *TAG = "WebUIStorage";
constexpr const char *PARTITION_LABEL = "spiffs";
constexpr const char *BASE_PATH = "/www";
constexpr size_t SHA256_HEX_LENGTH = 64;
constexpr size_t UPDATE_BUFFER_SIZE = 2048;

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
        : _locked(ensure_mutex() &&
                  xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE)
    {
    }

    ~StorageLock()
    {
        if (_locked) xSemaphoreGive(s_mutex);
    }

    explicit operator bool() const { return _locked; }

private:
    bool _locked;
};

void clear_error_locked()
{
    s_status.lastError[0] = '\0';
}

void set_error_locked(const char *message, esp_err_t error = ESP_OK)
{
    if (error == ESP_OK)
    {
        snprintf(s_status.lastError, sizeof(s_status.lastError), "%s", message);
    }
    else
    {
        snprintf(s_status.lastError, sizeof(s_status.lastError), "%s: %s",
                 message, esp_err_to_name(error));
    }
    ESP_LOGE(TAG, "%s", s_status.lastError);
}

bool is_regular_nonempty_file(const char *path)
{
    struct stat st = {};
    return stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

void read_version_locked()
{
    s_status.version[0] = '\0';

    FILE *file = fopen(BASE_PATH "/webui-version.txt", "r");
    if (!file) return;

    if (fgets(s_status.version, sizeof(s_status.version), file))
    {
        size_t length = strlen(s_status.version);
        while (length > 0 &&
               isspace(static_cast<unsigned char>(s_status.version[length - 1])))
        {
            s_status.version[--length] = '\0';
        }
    }
    fclose(file);
}

bool validate_files_locked()
{
    if (!s_status.mounted)
    {
        s_status.valid = false;
        return false;
    }

    // Only the New Design is supported. These are its mandatory build outputs;
    // optional PWA assets may be absent without making the UI unusable.
    const bool valid =
        is_regular_nonempty_file(BASE_PATH "/index.html.gz") &&
        is_regular_nonempty_file(BASE_PATH "/main.js.gz") &&
        is_regular_nonempty_file(BASE_PATH "/main.css.gz");

    s_status.valid = valid;
    if (valid)
    {
        read_version_locked();
    }
    else
    {
        s_status.version[0] = '\0';
    }
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
        // An empty partition is normal on devices upgraded from an older
        // firmware. Never format automatically: the embedded New Design remains
        // the recovery fallback until a verified WWW image has been installed.
        set_error_locked("SPIFFS mount failed", result);
        return result;
    }

    s_status.mounted = true;
    clear_error_locked();
    update_usage_locked();
    validate_files_locked();

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

    for (size_t i = 0; i < SHA256_HEX_LENGTH; ++i)
    {
        if (!isxdigit(static_cast<unsigned char>(value[i]))) return false;
    }
    return true;
}

void digest_to_hex(const unsigned char digest[32],
                   char output[SHA256_HEX_LENGTH + 1])
{
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i)
    {
        output[i * 2] = HEX[(digest[i] >> 4) & 0x0F];
        output[i * 2 + 1] = HEX[digest[i] & 0x0F];
    }
    output[SHA256_HEX_LENGTH] = '\0';
}

void sha_cleanup_locked()
{
    if (s_sha_ready)
    {
        mbedtls_md_free(&s_sha_context);
        s_sha_ready = false;
    }
}

const char *external_path_for_uri(const char *uri)
{
    if (!uri) return nullptr;
    if (strcmp(uri, "/main.js") == 0) return BASE_PATH "/main.js.gz";
    if (strcmp(uri, "/main.css") == 0) return BASE_PATH "/main.css.gz";
    if (strcmp(uri, "/favicon.ico") == 0) return BASE_PATH "/favicon.ico.gz";
    if (strcmp(uri, "/manifest.webmanifest") == 0) return BASE_PATH "/manifest.webmanifest.gz";
    if (strcmp(uri, "/icon-256.png") == 0) return BASE_PATH "/icon-256.png.gz";
    if (strcmp(uri, "/*") == 0) return BASE_PATH "/index.html.gz";
    return nullptr;
}

const char *content_type_for_uri(const char *uri)
{
    if (strcmp(uri, "/main.js") == 0) return "application/javascript";
    if (strcmp(uri, "/main.css") == 0) return "text/css";
    if (strcmp(uri, "/favicon.ico") == 0) return "image/x-icon";
    if (strcmp(uri, "/manifest.webmanifest") == 0) return "application/manifest+json";
    if (strcmp(uri, "/icon-256.png") == 0) return "image/png";
    return "text/html";
}

struct WrappedRoute
{
    bool used = false;
    httpd_uri_t replacement = {};
    httpd_uri_func originalHandler = nullptr;
    void *originalUserContext = nullptr;
    const char *externalPath = nullptr;
    const char *contentType = nullptr;
    bool firmwareUpdateRoute = false;
};

WrappedRoute s_wrapped_routes[8];

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

esp_err_t stream_external_file(httpd_req_t *req, WrappedRoute *route)
{
    FILE *file = fopen(route->externalPath, "rb");
    if (!file) return ESP_ERR_NOT_FOUND;

    add_security_headers(req);
    httpd_resp_set_type(req, route->contentType);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    // Asset names are currently stable rather than content-hashed. Disable
    // browser caching so a successful WWW update is visible immediately.
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
    if (!route || !route->originalHandler) return ESP_FAIL;

    if (webui_storage_is_valid())
    {
        const esp_err_t result = stream_external_file(req, route);
        if (result != ESP_ERR_NOT_FOUND)
        {
            return result;
        }
        ESP_LOGW(TAG, "External asset missing despite valid image: %s",
                 route->externalPath);
    }

    // Transition/recovery fallback. This is the same New Design embedded in the
    // migration firmware, never the retired old UI.
    return route->originalHandler(req);
}

esp_err_t wrapped_firmware_update_handler(httpd_req_t *req)
{
    auto *route = static_cast<WrappedRoute *>(req->user_ctx);
    if (!route || !route->originalHandler) return ESP_FAIL;

    const WebUIStorageStatus status = webui_storage_get_status();
    if (status.updateActive)
    {
        add_security_headers(req);
        return httpd_resp_send_err(req, HTTPD_409_CONFLICT,
                                   "WWW update already in progress");
    }
    return route->originalHandler(req);
}

esp_err_t get_webui_status_handler(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, nullptr);
    }

    const WebUIStorageStatus status = webui_storage_get_status();
    char response[512];
    snprintf(response, sizeof(response),
             "{\"partitionFound\":%s,\"mounted\":%s,\"valid\":%s,"
             "\"updateActive\":%s,\"partitionSize\":%u,\"totalBytes\":%u,"
             "\"usedBytes\":%u,\"bytesWritten\":%u,\"version\":\"%s\","
             "\"lastError\":\"%s\"}",
             status.partitionFound ? "true" : "false",
             status.mounted ? "true" : "false",
             status.valid ? "true" : "false",
             status.updateActive ? "true" : "false",
             static_cast<unsigned>(status.partitionSize),
             static_cast<unsigned>(status.totalBytes),
             static_cast<unsigned>(status.usedBytes),
             static_cast<unsigned>(status.bytesWritten),
             status.version,
             status.lastError);

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
                                   "Send the raw SPIFFS image as request body");
    }

    if (net_fetch_ota_active())
    {
        return httpd_resp_send_err(req, HTTPD_409_CONFLICT,
                                   "Firmware OTA already in progress");
    }

    bool net_locked = false;
    if (g_net_fetch_mutex)
    {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            return httpd_resp_send_err(req, HTTPD_503_SERVICE_UNAVAILABLE,
                                       "Update subsystem busy");
        }
        net_locked = true;
    }

    char expected_sha256[SHA256_HEX_LENGTH + 1] = {};
    const esp_err_t header_result = httpd_req_get_hdr_value_str(
        req, "X-WebUI-SHA256", expected_sha256, sizeof(expected_sha256));
    if (header_result != ESP_OK)
    {
        expected_sha256[0] = '\0';
    }

    esp_err_t result = webui_storage_update_begin(
        static_cast<size_t>(req->content_len), expected_sha256);
    if (result != ESP_OK)
    {
        if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Could not start WWW update");
    }

    uint8_t buffer[UPDATE_BUFFER_SIZE];
    size_t received = 0;
    int timeout_retries = 5;

    while (received < static_cast<size_t>(req->content_len))
    {
        const size_t remaining =
            static_cast<size_t>(req->content_len) - received;
        const size_t wanted = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        const int count = httpd_req_recv(req, reinterpret_cast<char *>(buffer),
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
    char response[160];
    snprintf(response, sizeof(response),
             "{\"success\":true,\"reload\":true,\"version\":\"%s\"}",
             status.version);
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

    webui_storage_init();

    const esp_err_t status_result =
        httpd_register_uri_handler(server, &s_webui_status_uri);
    const esp_err_t update_result =
        httpd_register_uri_handler(server, &s_webui_update_uri);

    if (status_result != ESP_OK || update_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not register WWW update endpoints: status=%s update=%s",
                 esp_err_to_name(status_result), esp_err_to_name(update_result));
        return;
    }

    s_registered_server = server;
}

} // namespace

esp_err_t webui_storage_init()
{
    StorageLock lock;
    if (!lock) return ESP_ERR_NO_MEM;

    if (s_status.updateActive) return ESP_ERR_INVALID_STATE;
    return mount_locked();
}

WebUIStorageStatus webui_storage_get_status()
{
    StorageLock lock;
    if (!lock)
    {
        WebUIStorageStatus failed = {};
        snprintf(failed.lastError, sizeof(failed.lastError),
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

esp_err_t webui_storage_update_begin(size_t expectedSize,
                                     const char *expectedSha256Hex)
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
    if (expectedSize == 0 || expectedSize > s_partition->size)
    {
        set_error_locked("WWW image does not fit partition");
        return ESP_ERR_INVALID_SIZE;
    }
    if (!valid_sha256_hex(expectedSha256Hex))
    {
        set_error_locked("Invalid WWW SHA-256 header");
        return ESP_ERR_INVALID_ARG;
    }

    unmount_locked();
    sha_cleanup_locked();

    const esp_err_t erase_result =
        esp_partition_erase_range(s_partition, 0, s_partition->size);
    if (erase_result != ESP_OK)
    {
        set_error_locked("Could not erase WWW partition", erase_result);
        mount_locked();
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
        set_error_locked("Could not initialize WWW SHA-256");
        mount_locked();
        return ESP_ERR_NO_MEM;
    }

    s_sha_ready = true;
    s_expected_size = expectedSize;
    s_expected_sha256[0] = '\0';
    if (expectedSha256Hex && expectedSha256Hex[0])
    {
        snprintf(s_expected_sha256, sizeof(s_expected_sha256), "%s",
                 expectedSha256Hex);
    }

    s_status.updateActive = true;
    s_status.bytesWritten = 0;
    s_status.valid = false;
    clear_error_locked();

    ESP_LOGI(TAG, "Starting separate WWW update: %u bytes%s",
             static_cast<unsigned>(expectedSize),
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
        set_error_locked("Incomplete WWW image");
        sha_cleanup_locked();
        s_status.updateActive = false;
        mount_locked();
        return ESP_ERR_INVALID_SIZE;
    }

    unsigned char digest[32] = {};
    if (mbedtls_md_finish(&s_sha_context, digest) != 0)
    {
        set_error_locked("Could not finalize WWW SHA-256");
        sha_cleanup_locked();
        s_status.updateActive = false;
        mount_locked();
        return ESP_FAIL;
    }
    sha_cleanup_locked();

    char actual_sha256[SHA256_HEX_LENGTH + 1];
    digest_to_hex(digest, actual_sha256);
    if (s_expected_sha256[0] &&
        strcasecmp(actual_sha256, s_expected_sha256) != 0)
    {
        set_error_locked("WWW SHA-256 mismatch");
        s_status.updateActive = false;
        mount_locked();
        return ESP_ERR_INVALID_CRC;
    }

    s_status.updateActive = false;
    const esp_err_t mount_result = mount_locked();
    if (mount_result != ESP_OK || !validate_files_locked())
    {
        if (!s_status.lastError[0])
        {
            set_error_locked("WWW image is missing mandatory New Design files");
        }
        return mount_result == ESP_OK ? ESP_ERR_INVALID_RESPONSE : mount_result;
    }

    update_usage_locked();
    clear_error_locked();
    ESP_LOGI(TAG, "Separate WWW update installed successfully, version=%s",
             s_status.version[0] ? s_status.version : "unknown");
    return ESP_OK;
}

void webui_storage_update_abort()
{
    StorageLock lock;
    if (!lock) return;

    sha_cleanup_locked();
    s_status.updateActive = false;
    if (!s_status.lastError[0])
    {
        set_error_locked("WWW update aborted");
    }
    mount_locked();
}

esp_err_t hb_webui_register_uri_handler(httpd_handle_t server,
                                        const httpd_uri_t *uri_handler)
{
    if (!server || !uri_handler || !uri_handler->uri)
    {
        return ESP_ERR_INVALID_ARG;
    }

    register_storage_endpoints_once(server);

    const char *external_path = external_path_for_uri(uri_handler->uri);
    const bool firmware_update_route =
        strcmp(uri_handler->uri, "/ota_update") == 0 ||
        strcmp(uri_handler->uri, "/api/ota_url") == 0;

    if (!external_path && !firmware_update_route)
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
    route->originalHandler = uri_handler->handler;
    route->originalUserContext = uri_handler->user_ctx;
    route->firmwareUpdateRoute = firmware_update_route;

    if (firmware_update_route)
    {
        route->replacement.handler = wrapped_firmware_update_handler;
        route->replacement.user_ctx = route;
    }
    else
    {
        route->externalPath = external_path;
        route->contentType = content_type_for_uri(uri_handler->uri);
        route->replacement.handler = wrapped_asset_handler;
        route->replacement.user_ctx = route;
    }

    return httpd_register_uri_handler(server, &route->replacement);
}
