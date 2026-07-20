#include "theme_api.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"

#include "security_headers.h"

extern esp_err_t validate_auth(httpd_req_t *req);

namespace
{
constexpr const char *TAG = "ThemeAPI";
constexpr const char *NVS_NAMESPACE = "ui_theme";
constexpr const char *NVS_SCHEME_KEY = "scheme";
constexpr const char *NVS_COLOR_KEY = "primary";
constexpr const char *DEFAULT_SCHEME = "system";
constexpr const char *DEFAULT_COLOR = "#f26a3d";

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

void load_theme(char scheme[8], char color[8])
{
    snprintf(scheme, 8, "%s", DEFAULT_SCHEME);
    snprintf(color, 8, "%s", DEFAULT_COLOR);

    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;

    size_t scheme_size = 8;
    if (nvs_get_str(handle, NVS_SCHEME_KEY, scheme, &scheme_size) != ESP_OK ||
        !valid_scheme(scheme))
    {
        snprintf(scheme, 8, "%s", DEFAULT_SCHEME);
    }

    size_t color_size = 8;
    if (nvs_get_str(handle, NVS_COLOR_KEY, color, &color_size) != ESP_OK ||
        !valid_color(color))
    {
        snprintf(color, 8, "%s", DEFAULT_COLOR);
    }
    nvs_close(handle);
}

esp_err_t send_theme(httpd_req_t *req)
{
    char scheme[8] = {};
    char color[8] = {};
    load_theme(scheme, color);

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

    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_set_str(handle, NVS_SCHEME_KEY, scheme);
    if (result == ESP_OK) result = nvs_set_str(handle, NVS_COLOR_KEY, color);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle) nvs_close(handle);
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
