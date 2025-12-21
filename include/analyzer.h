#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "radiomoduleconnector.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <vector>
#include <string>

// Maximum frame size for analyzer
#define ANALYZER_MAX_FRAME_SIZE 1024

// Structure for queued frames
struct AnalyzerFrame {
    unsigned char data[ANALYZER_MAX_FRAME_SIZE];
    uint16_t len;
    int64_t timestamp_ms;
};

class Analyzer : public FrameHandler
{
private:
    std::vector<httpd_handle_t> _clients;
    std::vector<int> _client_fds;
    SemaphoreHandle_t _mutex;
    RadioModuleConnector *_radioModuleConnector;

    // Async processing
    QueueHandle_t _frameQueue;
    TaskHandle_t _taskHandle;
    bool _running;
    bool _initialized;

    void _processingTask();
    void _processFrame(const AnalyzerFrame &frame);

public:
    Analyzer(RadioModuleConnector *radioModuleConnector);
    virtual ~Analyzer();

    void handleFrame(unsigned char *buffer, uint16_t len) override;

    void addClient(httpd_handle_t hd, int fd);
    void removeClient(httpd_handle_t hd, int fd);
    bool hasClients();
    bool isReady() const { return _initialized; }

    // Static handler for WebSocket events
    static esp_err_t ws_handler(httpd_req_t *req);

    // Friend function for task
    friend void analyzerProcessingTask(void *parameter);
};
