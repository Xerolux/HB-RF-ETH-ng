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
#include <fcntl.h>
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

void Hmlgw::cleanupSockets() {
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
}

void Hmlgw::handleFatalError(const char* reason) {
    ESP_LOGE(TAG, "HMLGW stopped due to error: %s", reason);
    _running = false;
    cleanupSockets();
    _connector->removeFrameHandler(this);
}

void Hmlgw::start() {
    if (_running) return;
    _running = true;

    // Start Main Task
    BaseType_t taskCreated = xTaskCreate([](void* arg) {
        static_cast<Hmlgw*>(arg)->run();
    }, "hmlgw_task", 4096, this, 5, &_taskHandle);

    if (taskCreated != pdPASS) {
        _taskHandle = NULL;
        handleFatalError("Failed to start HMLGW task");
        return;
    }

    // Start KeepAlive Task
    BaseType_t keepAliveCreated = xTaskCreate([](void* arg) {
        static_cast<Hmlgw*>(arg)->runKeepAlive();
    }, "hmlgw_ka_task", 2048, this, 5, &_keepAliveTaskHandle);

    if (keepAliveCreated != pdPASS) {
        _keepAliveTaskHandle = NULL;
        handleFatalError("Failed to start HMLGW keep-alive task");
        if (_taskHandle) {
            vTaskDelete(_taskHandle);
            _taskHandle = NULL;
        }
        return;
    }

    _connector->addFrameHandler(this);
    // Hmlgw protocol expects raw bytes usually, but wrapped?
    // If we use 'S' + Len + Data, we need raw data.
    // RadioModuleConnector by default parses frames.
    // If we want raw stream, we might need to setDecodeEscaped(false).
    _connector->setDecodeEscaped(false);
}

void Hmlgw::stop() {
    if (!_running) return;

    _running = false;
    _connector->removeFrameHandler(this);

    cleanupSockets();

    auto gracefully_delete_task = [&](TaskHandle_t& task_handle, const char* task_name) {
        if (task_handle) {
            // Wait for the task to exit its loop for up to 500ms
            for (int i = 0; i < 5; ++i) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            vTaskDelete(task_handle);
            task_handle = NULL;
            ESP_LOGI(TAG, "%s task deleted", task_name);
        }
    };

    gracefully_delete_task(_taskHandle, "Main");
    gracefully_delete_task(_keepAliveTaskHandle, "KeepAlive");
}

void Hmlgw::run() {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(_port);

    _serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (_serverSocket < 0) {
        handleFatalError("Unable to create socket");
        return;
    }

    int opt = 1;
    setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(_serverSocket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        handleFatalError("Socket unable to bind");
        return;
    }

    if (listen(_serverSocket, 1) != 0) {
        handleFatalError("Error occurred during listen");
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

    cleanupSockets();
    ESP_LOGI(TAG, "HMLGW task exiting");
    // Don't delete task here - stop() will handle it
}

void Hmlgw::runKeepAlive() {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(_keepAlivePort);

    _keepAliveServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (_keepAliveServerSocket < 0) {
        handleFatalError("Unable to create KA socket");
        return;
    }

    int opt = 1;
    setsockopt(_keepAliveServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(_keepAliveServerSocket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        handleFatalError("KA Socket unable to bind");
        return;
    }

    if (listen(_keepAliveServerSocket, 1) != 0) {
        handleFatalError("KA Error occurred during listen");
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
    cleanupSockets();
    ESP_LOGI(TAG, "HMLGW KeepAlive task exiting");
    // Don't delete task here - stop() will handle it
}

void Hmlgw::handleClient() {
    std::vector<uint8_t> buffer;
    uint8_t rx_buffer[1024];
    uint32_t loop_count = 0;

    // Set socket to non-blocking mode
    int flags = fcntl(_clientSocket, F_GETFL, 0);
    fcntl(_clientSocket, F_SETFL, flags | O_NONBLOCK);

    // Set receive timeout to prevent indefinite blocking
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(_clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (_running) {
        int len = recv(_clientSocket, rx_buffer, sizeof(rx_buffer), 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(10 / portTICK_PERIOD_MS);

                // Yield periodically to prevent watchdog timeout
                loop_count++;
                if (loop_count % 10 == 0) {
                    taskYIELD();
                }
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

        // Limit buffer size to prevent memory exhaustion
        if (buffer.size() > 65536) {
            ESP_LOGE(TAG, "Buffer overflow protection - clearing buffer");
            buffer.clear();
            continue;
        }

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

                     // Sanity check on data length
                     if (dataLen > 8192) {
                         ESP_LOGE(TAG, "Invalid data length %d, skipping", dataLen);
                         processed++;
                         continue;
                     }

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

        // Yield to prevent watchdog timeout
        taskYIELD();
    }
}

void Hmlgw::sendDataToClient(const uint8_t* data, size_t len) {
    if (_clientSocket != -1) {
        ssize_t sent = send(_clientSocket, data, len, 0);
        if (sent < 0) {
            ESP_LOGW(TAG, "Send failed, closing client: errno %d", errno);
            close(_clientSocket);
            _clientSocket = -1;
        }
    }
}

void Hmlgw::handleFrame(unsigned char* buffer, uint16_t len) {
    if (!_running || _clientSocket == -1) return;

    // Encapsulate in 'S' frame
    // 'S' + HighLen + LowLen + Data
    uint8_t header[3];
    header[0] = CMD_SERIAL;
    header[1] = (len >> 8) & 0xFF;
    header[2] = len & 0xFF;

    sendDataToClient(header, 3);
    sendDataToClient(buffer, len);
}
