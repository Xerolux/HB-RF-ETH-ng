#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "radiomoduleconnector.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <vector>
#include <string>

class Analyzer : public FrameHandler
{
private:
    std::vector<httpd_handle_t> _clients;
    std::vector<int> _client_fds;
    SemaphoreHandle_t _mutex;
    RadioModuleConnector *_radioModuleConnector;

public:
    Analyzer(RadioModuleConnector *radioModuleConnector);
    virtual ~Analyzer();

    void handleFrame(unsigned char *buffer, uint16_t len) override;

    void addClient(httpd_handle_t hd, int fd);
    void removeClient(httpd_handle_t hd, int fd);
    bool hasClients();

    // Static handler for WebSocket events
    static esp_err_t ws_handler(httpd_req_t *req);
};
