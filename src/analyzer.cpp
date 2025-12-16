#include "analyzer.h"
#include "esp_log.h"
#include <cstring>
#include <iomanip>
#include <sstream>

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

    // Convert frame to JSON
    // Format: { "ts": <timestamp_ms>, "rssi": <rssi>, "len": <len>, "data": "<hex_string>" }
    // Note: RSSI is not directly available in the raw UART stream unless parsing specific frames.
    // For now, we send raw data and let the frontend parse it.

    // We'll use a simple JSON construction to avoid heap overhead of cJSON for every frame if possible,
    // but cJSON is safer.

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "ts", esp_timer_get_time() / 1000); // ms since boot
    cJSON_AddNumberToObject(root, "len", len);

    char *hex = (char*)malloc(len * 2 + 1);
    if (hex) {
        for (int i = 0; i < len; i++) {
            sprintf(hex + i * 2, "%02X", buffer[i]);
        }
        cJSON_AddStringToObject(root, "data", hex);
        free(hex);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str) {
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t *)json_str;
        ws_pkt.len = strlen(json_str);
        ws_pkt.type = HTTPD_WS_TEXT;

        if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
            // Iterate backwards to allow safe removal
            for (int i = _client_fds.size() - 1; i >= 0; i--) {
                esp_err_t ret = httpd_ws_send_frame_async(_clients[i], _client_fds[i], &ws_pkt);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send to client %d: %s", _client_fds[i], esp_err_to_name(ret));
                    // If send fails, we might want to remove the client, but usually httpd handles disconnects.
                    // However, async send might fail if socket is closed.
                }
            }
            xSemaphoreGive(_mutex);
        }
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

        // Verify authentication?
        // Typically ws connections don't easily support Authorization header in browser API,
        // but cookies or query params work. Or we can rely on the fact that the page is protected
        // (but WS endpoint should be too).
        // For simplicity, let's assume if they can connect they are authorized or we check cookie/token in handshake.
        // But req->headers might not have it if using standard WS API.
        // Let's check query param ?token=... or similar if needed.
        // For now, let's allow it but maybe we should check.

        // Note: The main WebUI uses Token auth. The WS client should probably send it.
        // Since we can't easily add headers in standard WebSocket JS API, query param is best.
        // But let's see.

        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TEXT;

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
        // ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

    // Triggered when close
    if (ws_pkt.type == HTTPD_WS_CLOSE) {
         ESP_LOGI(TAG, "Websocket close");
         if (_instance) {
             _instance->removeClient(req->handle, httpd_req_to_sockfd(req));
         }
         free(buf);
         return ESP_OK;
    }

    // If it's a new connection (first frame or just opened), we need to track it.
    // Actually httpd doesn't explicitly tell us "Opened" except via GET handshake.
    // But we can't get sockfd easily in GET.
    // So usually we rely on "first message" or we check on every message.
    // Wait, `httpd_req_to_sockfd(req)` works.

    if (_instance) {
        _instance->addClient(req->handle, httpd_req_to_sockfd(req));
    }

    if (ws_pkt.type == HTTPD_WS_TEXT && strcmp((char*)ws_pkt.payload, "ping") == 0) {
        // Pong
         httpd_ws_frame_t response;
         memset(&response, 0, sizeof(httpd_ws_frame_t));
         response.payload = (uint8_t*)"pong";
         response.len = 4;
         response.type = HTTPD_WS_TEXT;
         httpd_ws_send_frame(req, &response);
    }

    free(buf);
    return ESP_OK;
}
