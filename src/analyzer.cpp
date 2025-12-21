#include "analyzer.h"
#include "esp_log.h"
#include <cstring>
#include <stdio.h>
#include <inttypes.h>
#include <cstdlib>

static const char *TAG = "Analyzer";
static Analyzer *_instance = NULL;

struct AnalyzerWsSendJob {
    httpd_handle_t handle;
    int fd;
    char *payload;
    size_t len;
    Analyzer *analyzer;
};

static void analyzer_ws_send(void *arg)
{
    AnalyzerWsSendJob *job = static_cast<AnalyzerWsSendJob *>(arg);
    if (!job || !job->payload) {
        return;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = reinterpret_cast<uint8_t *>(job->payload);
    ws_pkt.len = job->len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.final = true;

    // Executed on the HTTP server thread (queued via httpd_queue_work), so use the synchronous API
    esp_err_t ret = httpd_ws_send_frame(job->handle, job->fd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send to client %d: %s", job->fd, esp_err_to_name(ret));
        if (job->analyzer) {
            job->analyzer->removeClient(job->handle, job->fd);
        }
    }

    free(job->payload);
    free(job);
}

// Friend function for FreeRTOS task
void analyzerProcessingTask(void *parameter)
{
    ((Analyzer *)parameter)->_processingTask();
}

Analyzer::Analyzer(RadioModuleConnector *radioModuleConnector) : _radioModuleConnector(radioModuleConnector), _running(false), _initialized(false)
{
    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        ESP_LOGE(TAG, "Failed to create analyzer mutex");
        return;
    }

    // Create queue for async frame processing (holds up to 20 frames)
    // If queue is full, oldest frames will be dropped to prevent blocking
    _frameQueue = xQueueCreate(20, sizeof(AnalyzerFrame));
    if (!_frameQueue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        vSemaphoreDelete(_mutex);
        _mutex = NULL;
        return;
    }

    _instance = this;

    // Create processing task with lower priority (5) to not interfere with UART
    _running = true;
    BaseType_t ret = xTaskCreate(analyzerProcessingTask, "Analyzer_Processing", 8192, this, 5, &_taskHandle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create processing task");
        _taskHandle = NULL;
        _running = false;
        vQueueDelete(_frameQueue);
        _frameQueue = NULL;
        vSemaphoreDelete(_mutex);
        _mutex = NULL;
        _instance = NULL;
    } else {
        ESP_LOGI(TAG, "Analyzer processing task created successfully");
        _initialized = true;
    }
}

Analyzer::~Analyzer()
{
    _initialized = false;
    ESP_LOGI(TAG, "Analyzer destructor called");

    // First, deregister from radio module to stop receiving new frames
    if (_radioModuleConnector) {
        _radioModuleConnector->removeFrameHandler(this);
    }

    // Close any active websocket sessions to avoid dangling clients
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(200))) {
        for (size_t i = 0; i < _clients.size(); i++) {
            httpd_sess_trigger_close(_clients[i], _client_fds[i]);
        }
        _clients.clear();
        _client_fds.clear();
        xSemaphoreGive(_mutex);
    }

    // Signal task to stop
    _running = false;

    // Wait for task to finish gracefully (up to 500ms)
    if (_taskHandle) {
        int retries = 5;
        while (retries-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            // Check if task is still running by verifying queue operations
            if (uxQueueMessagesWaiting(_frameQueue) == 0) {
                break;
            }
        }
        // Now safe to delete the task
        vTaskDelete(_taskHandle);
        _taskHandle = NULL;
        ESP_LOGI(TAG, "Processing task deleted");
    }

    // Drain queue and free any allocated memory
    if (_frameQueue) {
        AnalyzerFrame frame;
        while (xQueueReceive(_frameQueue, &frame, 0) == pdTRUE) {
            if (frame.longData) {
                free(frame.longData);
            }
        }
        vQueueDelete(_frameQueue);
        _frameQueue = NULL;
    }

    // Clean up mutex
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = NULL;
    }

    _instance = NULL;
    ESP_LOGI(TAG, "Analyzer destructor completed");
}

void Analyzer::handleFrame(unsigned char *buffer, uint16_t len)
{
    // Safety checks: ensure we're still running and resources are valid
    if (!_initialized || !_running || !_frameQueue || !buffer) {
        return;
    }

    if (!hasClients()) {
        return;
    }

    // Sanity check on frame length
    if (len > ANALYZER_MAX_FRAME_SIZE) {
        ESP_LOGW(TAG, "Frame too large for analyzer: %d bytes (max %d)", len, ANALYZER_MAX_FRAME_SIZE);
        return;
    }

    // Create frame structure
    AnalyzerFrame frame;
    frame.len = len;
    frame.timestamp_ms = esp_timer_get_time() / 1000;
    frame.longData = nullptr;

    if (len <= ANALYZER_SHORT_BUFFER_SIZE) {
        memcpy(frame.shortData, buffer, len);
    } else {
        frame.longData = (uint8_t*)malloc(len);
        if (frame.longData) {
            memcpy(frame.longData, buffer, len);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for frame");
            return;
        }
    }

    // Try to send to queue without blocking (important: don't block UART task!)
    if (xQueueSend(_frameQueue, &frame, 0) != pdTRUE) {
        // Queue is full, drop oldest frame and try again
        AnalyzerFrame dummy;
        if (xQueueReceive(_frameQueue, &dummy, 0) == pdTRUE) {
            // Free memory if the dropped frame had dynamic allocation
            if (dummy.longData) {
                free(dummy.longData);
            }
        }

        if (xQueueSend(_frameQueue, &frame, 0) != pdTRUE) {
            // Still can't send, drop this frame
            if (frame.longData) {
                free(frame.longData);
            }
            ESP_LOGW(TAG, "Frame queue full, dropping frame");
        }
    }
}

void Analyzer::_processingTask()
{
    AnalyzerFrame frame;
    ESP_LOGI(TAG, "Analyzer processing task started");

    while (_running) {
        // Wait for frame with timeout to allow clean shutdown
        if (xQueueReceive(_frameQueue, &frame, pdMS_TO_TICKS(100)) == pdTRUE) {
            _processFrame(frame);
            // Free memory if frame used dynamic allocation
            if (frame.longData) {
                free(frame.longData);
            }
        }
    }

    ESP_LOGI(TAG, "Analyzer processing task stopped");
    // DO NOT delete task here - destructor will handle it
    // This prevents double-deletion crashes
}

void Analyzer::_processFrame(const AnalyzerFrame &frame)
{
    // Safety check: ensure we're still running
    if (!_running || !_mutex) {
        return;
    }

    const uint8_t* dataPtr = frame.longData ? frame.longData : frame.shortData;

    // Convert frame to JSON
    // Format: { "ts": <timestamp_ms>, "len": <len>, "data": "<hex_string>" }
    // Optimization: Use manual string formatting instead of cJSON to reduce heap allocations

    char stack_buffer[2048];
    char* json_str = stack_buffer;
    bool heap_allocated = false;
    size_t required_size = 64 + frame.len * 2;

    if (required_size > sizeof(stack_buffer)) {
        json_str = (char*)malloc(required_size);
        if (!json_str) {
            ESP_LOGE(TAG, "Failed to allocate memory for frame JSON");
            return;
        }
        heap_allocated = true;
    }

    // Header
    int offset = snprintf(json_str, required_size, "{\"ts\":%" PRId64 ",\"len\":%d,\"data\":\"", frame.timestamp_ms, frame.len);

    // Fast Hex Conversion
    static const char hex_map[] = "0123456789ABCDEF";
    for (int i = 0; i < frame.len; i++) {
        json_str[offset++] = hex_map[(dataPtr[i] >> 4) & 0xF];
        json_str[offset++] = hex_map[dataPtr[i] & 0xF];
    }

    // Footer
    json_str[offset++] = '"';
    json_str[offset++] = '}';
    json_str[offset] = 0;
    std::vector<httpd_handle_t> clients;
    std::vector<int> client_fds;

    // Use short timeout to prevent blocking (50ms instead of 10ms for more stability)
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50))) {
        if (!_client_fds.empty()) {
            clients = _clients;
            client_fds = _client_fds;
        } else {
            _radioModuleConnector->removeFrameHandler(this);
        }

        xSemaphoreGive(_mutex);
    } else {
        ESP_LOGW(TAG, "Could not acquire mutex for WebSocket send, dropping frame");
    }

    if (clients.empty()) {
        if (heap_allocated) {
            free(json_str);
        }
        return;
    }

    for (size_t i = 0; i < client_fds.size(); i++) {
        AnalyzerWsSendJob *job = static_cast<AnalyzerWsSendJob *>(malloc(sizeof(AnalyzerWsSendJob)));
        if (!job) {
            ESP_LOGE(TAG, "Failed to allocate ws send job");
            continue;
        }

        job->payload = static_cast<char *>(malloc(offset));
        if (!job->payload) {
            ESP_LOGE(TAG, "Failed to allocate ws payload");
            free(job);
            continue;
        }

        memcpy(job->payload, json_str, offset);
        job->len = offset;
        job->handle = clients[i];
        job->fd = client_fds[i];
        job->analyzer = this;

        esp_err_t work_ret = httpd_queue_work(job->handle, analyzer_ws_send, job);
        if (work_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to queue ws send for client %d: %s", job->fd, esp_err_to_name(work_ret));
            free(job->payload);
            free(job);
            removeClient(job->handle, job->fd);
        }
    }

    if (heap_allocated) {
        free(json_str);
    }
}

void Analyzer::addClient(httpd_handle_t hd, int fd)
{
    if (!_initialized) {
        ESP_LOGW(TAG, "Analyzer not initialized, rejecting client %d", fd);
        return;
    }
    // Use timeout instead of portMAX_DELAY to prevent potential deadlocks
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000))) {
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
            ESP_LOGI(TAG, "Client %d added, total clients: %d", fd, _client_fds.size());

            // If this is the first client, register with RadioModuleConnector
            if (_client_fds.size() == 1) {
                _radioModuleConnector->addFrameHandler(this);
            }
        }
        xSemaphoreGive(_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire mutex in addClient for fd %d", fd);
    }
}

void Analyzer::removeClient(httpd_handle_t hd, int fd)
{
    if (!_initialized) {
        return;
    }
    // Use timeout instead of portMAX_DELAY to prevent potential deadlocks
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000))) {
        for (size_t i = 0; i < _client_fds.size(); i++) {
            if (_client_fds[i] == fd) {
                _clients.erase(_clients.begin() + i);
                _client_fds.erase(_client_fds.begin() + i);
                ESP_LOGI(TAG, "Client %d removed, remaining clients: %d", fd, _client_fds.size());
                break;
            }
        }

        // If no clients left, deregister
        if (_client_fds.empty()) {
            _radioModuleConnector->removeFrameHandler(this);
        }
        xSemaphoreGive(_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire mutex in removeClient for fd %d", fd);
    }
}

bool Analyzer::hasClients()
{
    bool has = false;
    if (!_initialized) {
        return false;
    }
    // Use timeout instead of portMAX_DELAY to prevent potential deadlocks
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100))) {
        has = !_client_fds.empty();
        xSemaphoreGive(_mutex);
    }
    return has;
}

esp_err_t Analyzer::ws_handler(httpd_req_t *req)
{
    if (_instance == nullptr || !_instance->isReady()) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "Analyzer is not ready");
        return ESP_OK;
    }

    if (req->method == HTTP_GET) {
        // Handshake
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        if (_instance) {
            _instance->addClient(req->handle, httpd_req_to_sockfd(req));
        }
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
