/*
 *  hmlgw.cpp - HomeMatic LAN Gateway Emulation
 *
 *  This class implements the LAN Gateway protocol to be compatible with
 *  software that expects a HomeMatic LAN Gateway (HM-LGW-O-TW-W-EU) or
 *  compatible daemon (hmlangw).
 *
 *  The protocol implementation is based on public information about the
 *  HomeMatic LAN Gateway protocol and is compatible with the hmlangw project.
 *
 *  Protocol overview:
 *  - Port 2000 (Data): Handles data stream.
 *    - 'H': Hello/Identification.
 *    - 'K': KeepAlive.
 *    - 'S': Serial data encapsulation.
 *  - Port 2001 (KeepAlive): Optional KeepAlive channel (echo).
 *
 *  This code was written for HB-RF-ETH-ng and is not a copy of other projects.
 */

#include "hmlgw.h"
#include "esp_log.h"
#include <string.h>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <vector>

static const char* TAG = "HMLGW";

// Protocol constants
const char CMD_HELLO = 'H';
const char CMD_KEEPALIVE = 'K';
const char CMD_SERIAL = 'S';
static const char* IDENTIFIER = "HM-USB-IF";

Hmlgw::Hmlgw(RadioModuleConnector* connector, uint16_t port, uint16_t keepAlivePort)
    : _connector(connector), _running(false), _serverSocket(-1), _clientSocket(-1),
      _taskHandle(NULL), _port(port), _keepAlivePort(keepAlivePort),
      _keepAliveServerSocket(-1), _keepAliveClientSocket(-1), _keepAliveTaskHandle(NULL) {
}

Hmlgw::~Hmlgw() {
    stop();
}

void Hmlgw::start() {
    if (_running) return;
    _running = true;

    // Start Main Task
    xTaskCreate([](void* arg) {
        static_cast<Hmlgw*>(arg)->run();
    }, "hmlgw_task", 4096, this, 5, &_taskHandle);

    // Start KeepAlive Task
    xTaskCreate([](void* arg) {
        static_cast<Hmlgw*>(arg)->runKeepAlive();
    }, "hmlgw_ka_task", 2048, this, 5, &_keepAliveTaskHandle);

    _connector->addFrameHandler(this);
    // Hmlgw protocol expects raw bytes usually, but wrapped?
    // If we use 'S' + Len + Data, we need raw data.
    // RadioModuleConnector by default parses frames.
    // If we want raw stream, we might need to setDecodeEscaped(false).
    _connector->setDecodeEscaped(false);
}

void Hmlgw::stop() {
    _running = false;
    _connector->removeFrameHandler(this);

    if (_clientSocket != -1) {
        close(_clientSocket);
        _clientSocket = -1;
    }
    if (_serverSocket != -1) {
        close(_serverSocket);
        _serverSocket = -1;
    }
    if (_keepAliveClientSocket != -1) {
        close(_keepAliveClientSocket);
        _keepAliveClientSocket = -1;
    }
    if (_keepAliveServerSocket != -1) {
        close(_keepAliveServerSocket);
        _keepAliveServerSocket = -1;
    }

    if (_taskHandle) {
        vTaskDelete(_taskHandle);
        _taskHandle = NULL;
    }
    if (_keepAliveTaskHandle) {
        vTaskDelete(_keepAliveTaskHandle);
        _keepAliveTaskHandle = NULL;
    }
}

void Hmlgw::run() {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(_port);

    _serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (_serverSocket < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    int opt = 1;
    setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(_serverSocket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(_serverSocket);
        return;
    }

    if (listen(_serverSocket, 1) != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(_serverSocket);
        return;
    }

    ESP_LOGI(TAG, "HMLGW Server listening on port %d", _port);

    while (_running) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(_serverSocket, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        ESP_LOGI(TAG, "Socket accepted");
        _clientSocket = sock;

        handleClient();

        ESP_LOGI(TAG, "Socket disconnected");
        if (_clientSocket != -1) {
            close(_clientSocket);
            _clientSocket = -1;
        }
    }
    vTaskDelete(NULL);
}

void Hmlgw::runKeepAlive() {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(_keepAlivePort);

    _keepAliveServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (_keepAliveServerSocket < 0) {
        ESP_LOGE(TAG, "Unable to create KA socket: errno %d", errno);
        return;
    }

    int opt = 1;
    setsockopt(_keepAliveServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(_keepAliveServerSocket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "KA Socket unable to bind: errno %d", errno);
        close(_keepAliveServerSocket);
        return;
    }

    if (listen(_keepAliveServerSocket, 1) != 0) {
        ESP_LOGE(TAG, "KA Error occurred during listen: errno %d", errno);
        close(_keepAliveServerSocket);
        return;
    }

    ESP_LOGI(TAG, "HMLGW KeepAlive listening on port %d", _keepAlivePort);

    while (_running) {
         struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(_keepAliveServerSocket, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
             vTaskDelay(100 / portTICK_PERIOD_MS);
             continue;
        }

        _keepAliveClientSocket = sock;
        // Keepalive usually just accepts connection and maybe echoes?
        // Or just holds it open.
        // We will just read and discard, or echo.
        char rx_buffer[128];
        while (_running) {
            int len = recv(_keepAliveClientSocket, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                break;
            } else if (len == 0) {
                break;
            }
            // Echo back? Or just ignore?
            // "K" is sent on Data port.
        }
        close(_keepAliveClientSocket);
        _keepAliveClientSocket = -1;
    }
    vTaskDelete(NULL);
}

void Hmlgw::handleClient() {
    std::vector<uint8_t> buffer;
    uint8_t rx_buffer[1024];

    while (_running) {
        int len = recv(_clientSocket, rx_buffer, sizeof(rx_buffer), 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGE(TAG, "Error occurred during recv: errno %d", errno);
            break;
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
            break;
        }

        // Append new data to buffer
        buffer.insert(buffer.end(), rx_buffer, rx_buffer + len);

        // Process buffer
        size_t processed = 0;
        while(processed < buffer.size()) {
            char cmd = buffer[processed];
            if (cmd == CMD_HELLO) { // 'H'
                 char resp[32];
                 int rlen = snprintf(resp, sizeof(resp), "H%s", IDENTIFIER);
                 sendDataToClient((uint8_t*)resp, rlen);
                 processed++;
            } else if (cmd == CMD_KEEPALIVE) { // 'K'
                 char resp = 'K';
                 sendDataToClient((uint8_t*)&resp, 1);
                 processed++;
            } else if (cmd == CMD_SERIAL) { // 'S'
                 // Need at least 3 bytes for header (S + LenHigh + LenLow)
                 if (processed + 3 <= buffer.size()) {
                     uint16_t dataLen = (buffer[processed+1] << 8) | buffer[processed+2];
                     if (processed + 3 + dataLen <= buffer.size()) {
                         // We have the full frame
                         _connector->sendFrame(&buffer[processed+3], dataLen);
                         processed += 3 + dataLen;
                     } else {
                         // Partial frame... wait for more data
                         break;
                     }
                 } else {
                     // Partial header... wait for more data
                     break;
                 }
            } else {
                // Unknown command or sync lost?
                // Just skip byte
                processed++;
            }
        }

        // Remove processed data from buffer
        if (processed > 0) {
            buffer.erase(buffer.begin(), buffer.begin() + processed);
        }
    }
}

void Hmlgw::sendDataToClient(const uint8_t* data, size_t len) {
    if (_clientSocket != -1) {
        send(_clientSocket, data, len, 0);
    }
}

void Hmlgw::handleFrame(unsigned char* buffer, uint16_t len) {
    if (_clientSocket == -1) return;

    // Encapsulate in 'S' frame
    // 'S' + HighLen + LowLen + Data
    uint8_t header[3];
    header[0] = CMD_SERIAL;
    header[1] = (len >> 8) & 0xFF;
    header[2] = len & 0xFF;

    sendDataToClient(header, 3);
    sendDataToClient(buffer, len);
}
