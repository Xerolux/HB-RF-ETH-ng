#include "analyzer.h"
#include "esp_log.h"
#include <cstring>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "Analyzer";
static Analyzer *_instance = NULL;

Analyzer::Analyzer(RadioModuleConnector *radioModuleConnector) : _radioModuleConnector(radioModuleConnector)
{
    _mutex = xSemaphoreCreateMutex();
    _instance = this;
}

Analyzer::~Analyzer()
{
    if (_radioModuleConnector) {
        _radioModuleConnector->removeFrameHandler(this);
    }
    vSemaphoreDelete(_mutex);
    _instance = NULL;
}

void Analyzer::handleFrame(unsigned char *buffer, uint16_t len)
{
    if (!hasClients()) {
        return;
    }

    // Sanity check on frame length
    if (len > 1024) {
        ESP_LOGW(TAG, "Frame too large for analyzer: %d bytes", len);
        return;
    }

    // Convert frame to JSON
    // Format: { "ts": <timestamp_ms>, "rssi": <rssi>, "len": <len>, "data": "<hex_string>" }
    // Optimization: Use manual string formatting instead of cJSON to reduce heap allocations
    // Buffer size calculation:
    // {"ts": (5 chars) + 20 chars (int64) + ",\"len\": (7) + 5 (uint16) + ",\"data\":\"" (9) + len*2 + "\"}" (2) + null (1)
    // = ~50 + len * 2
    // We use a safe stack buffer for small frames, and heap for large ones.

    char stack_buffer[2048];
    char* json_str = stack_buffer;
    bool heap_allocated = false;
    size_t required_size = 64 + len * 2;

    if (required_size > sizeof(stack_buffer)) {
        json_str = (char*)malloc(required_size);
        if (!json_str) {
            ESP_LOGE(TAG, "Failed to allocate memory for frame JSON");
            return;
        }
        heap_allocated = true;
    }

    // Header
    int offset = snprintf(json_str, required_size, "{\"ts\":%" PRId64 ",\"len\":%d,\"data\":\"", esp_timer_get_time() / 1000, len);

    // Fast Hex Conversion
    static const char hex_map[] = "0123456789ABCDEF";
    for (int i = 0; i < len; i++) {
        json_str[offset++] = hex_map[(buffer[i] >> 4) & 0xF];
        json_str[offset++] = hex_map[buffer[i] & 0xF];
    }

    // Footer
    json_str[offset++] = '"';
    json_str[offset++] = '}';
    json_str[offset] = 0;

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)json_str;
    ws_pkt.len = offset;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // Use short timeout to prevent blocking
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10))) {
        // Iterate backwards to allow safe removal
        std::vector<int> failed_clients;

        for (int i = _client_fds.size() - 1; i >= 0; i--) {
            esp_err_t ret = httpd_ws_send_frame_async(_clients[i], _client_fds[i], &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send to client %d: %s", _client_fds[i], esp_err_to_name(ret));

                // Mark for removal if connection is broken
                if (ret == ESP_ERR_INVALID_STATE || ret == ESP_ERR_NOT_FOUND) {
                    failed_clients.push_back(_client_fds[i]);
                }
            }
        }

        // Remove failed clients
        for (int fd : failed_clients) {
            for (size_t i = 0; i < _client_fds.size(); i++) {
                if (_client_fds[i] == fd) {
                    _clients.erase(_clients.begin() + i);
                    _client_fds.erase(_client_fds.begin() + i);
                    ESP_LOGI(TAG, "Removed failed client %d", fd);
                    break;
                }
            }
        }

        // If no clients left, deregister
        if (_client_fds.empty()) {
            _radioModuleConnector->removeFrameHandler(this);
        }

        xSemaphoreGive(_mutex);
    } else {
        ESP_LOGW(TAG, "Could not acquire mutex for WebSocket send, dropping frame");
    }

    if (heap_allocated) {
        free(json_str);
    }
}

void Analyzer::addClient(httpd_handle_t hd, int fd)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        bool found = false;
        for (int socket : _client_fds) {
            if (socket == fd) {
                found = true;
                break;
            }
        }
        if (!found) {
            _clients.push_back(hd);
            _client_fds.push_back(fd);

            // If this is the first client, register with RadioModuleConnector
            if (_client_fds.size() == 1) {
                _radioModuleConnector->addFrameHandler(this);
            }
        }
        xSemaphoreGive(_mutex);
    }
}

void Analyzer::removeClient(httpd_handle_t hd, int fd)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        for (size_t i = 0; i < _client_fds.size(); i++) {
            if (_client_fds[i] == fd) {
                _clients.erase(_clients.begin() + i);
                _client_fds.erase(_client_fds.begin() + i);
                break;
            }
        }

        // If no clients left, deregister
        if (_client_fds.empty()) {
            _radioModuleConnector->removeFrameHandler(this);
        }
        xSemaphoreGive(_mutex);
    }
}

bool Analyzer::hasClients()
{
    bool has = false;
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        has = !_client_fds.empty();
        xSemaphoreGive(_mutex);
    }
    return has;
}

esp_err_t Analyzer::ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // Handshake
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // Set max length
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len: %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
        buf = (uint8_t *)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
    }

    // Triggered when close
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
         ESP_LOGI(TAG, "Websocket close");
         if (_instance) {
             _instance->removeClient(req->handle, httpd_req_to_sockfd(req));
         }
         free(buf);
         return ESP_OK;
    }

    if (_instance) {
        _instance->addClient(req->handle, httpd_req_to_sockfd(req));
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp((char*)ws_pkt.payload, "ping") == 0) {
        // Pong
         httpd_ws_frame_t response;
         memset(&response, 0, sizeof(httpd_ws_frame_t));
         response.payload = (uint8_t*)"pong";
         response.len = 4;
         response.type = HTTPD_WS_TYPE_TEXT;
         httpd_ws_send_frame(req, &response);
    }

    free(buf);
    return ESP_OK;
}
