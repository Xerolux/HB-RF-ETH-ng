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

// Fixed limits for security
#define MAX_FRAME_SIZE 8000  // Maximum frame size to prevent buffer overflow
#define RX_BUFFER_SIZE 1024
#define TEMP_FRAME_BUFFER_SIZE 8192

/**
 * Ring Buffer for efficient HMLGW data handling
 *
 * OPTIMIZED for real-time signal processing:
 * - Uses memcpy for contiguous writes (2-5x faster than byte-by-byte)
 * - Eliminates heap fragmentation from std::vector reallocations
 * - Reduces CPU overhead from copy operations
 * - Provides predictable memory usage (fixed 8KB)
 */
class RingBuffer {
private:
    static const size_t BUFFER_SIZE = 8192;  // Fixed 8KB buffer
    uint8_t _data[BUFFER_SIZE];
    size_t _readPos;
    size_t _writePos;
    size_t _count;  // Number of bytes currently in buffer

public:
    RingBuffer() : _readPos(0), _writePos(0), _count(0) {
        memset(_data, 0, sizeof(_data));
    }

    size_t size() const { return _count; }
    size_t capacity() const { return BUFFER_SIZE; }
    size_t available() const { return BUFFER_SIZE - _count; }

    // OPTIMIZED: Write data using memcpy for contiguous regions
    size_t write(const uint8_t* data, size_t len) {
        size_t toWrite = (len > available()) ? available() : len;
        if (toWrite == 0) return 0;

        // Calculate how much can be written before wrap-around
        size_t firstChunk = BUFFER_SIZE - _writePos;
        if (firstChunk > toWrite) firstChunk = toWrite;

        // First contiguous write
        memcpy(&_data[_writePos], data, firstChunk);

        // Handle wrap-around if needed
        if (toWrite > firstChunk) {
            memcpy(&_data[0], data + firstChunk, toWrite - firstChunk);
        }

        _writePos = (_writePos + toWrite) % BUFFER_SIZE;
        _count += toWrite;
        return toWrite;
    }

    // Peek at data without removing it
    uint8_t operator[](size_t index) const {
        if (index >= _count) return 0;
        return _data[(_readPos + index) % BUFFER_SIZE];
    }

    // Remove processed bytes from buffer
    void consume(size_t bytes) {
        if (bytes > _count) bytes = _count;
        _readPos = (_readPos + bytes) % BUFFER_SIZE;
        _count -= bytes;
    }

    // Clear buffer
    void clear() {
        _readPos = _writePos = _count = 0;
    }

    // OPTIMIZED: Copy data using memcpy for contiguous regions
    void copyData(uint8_t* dest, size_t offset, size_t len) const {
        if (len == 0 || offset >= _count) return;
        if (offset + len > _count) len = _count - offset;

        size_t srcPos = (_readPos + offset) % BUFFER_SIZE;
        size_t firstChunk = BUFFER_SIZE - srcPos;
        if (firstChunk > len) firstChunk = len;

        memcpy(dest, &_data[srcPos], firstChunk);

        if (len > firstChunk) {
            memcpy(dest + firstChunk, &_data[0], len - firstChunk);
        }
    }
};

Hmlgw::Hmlgw(RadioModuleConnector* connector, uint16_t port, uint16_t keepAlivePort)
    : _connector(connector),
      _running(false),
      _serverSocket(-1),
      _clientSocket(-1),
      _taskHandle(NULL),
      _port(port),
      _keepAlivePort(keepAlivePort),
      _keepAliveServerSocket(-1),
      _keepAliveClientSocket(-1),
      _keepAliveTaskHandle(NULL) {
}

Hmlgw::~Hmlgw() {
    stop();
}

void Hmlgw::cleanupSockets() {
    int clientSock = _clientSocket.exchange(-1);
    if (clientSock >= 0) {
        close(clientSock);
    }
    int serverSock = _serverSocket.exchange(-1);
    if (serverSock >= 0) {
        close(serverSock);
    }
    int kaClientSock = _keepAliveClientSocket.exchange(-1);
    if (kaClientSock >= 0) {
        close(kaClientSock);
    }
    int kaServerSock = _keepAliveServerSocket.exchange(-1);
    if (kaServerSock >= 0) {
        close(kaServerSock);
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
    // OPTIMIZED: Increased priority from 15 to 17 for faster CCU communication
    // Increased stack size from 4KB to 6KB for stability
    BaseType_t taskCreated = xTaskCreate([](void* arg) {
        static_cast<Hmlgw*>(arg)->run();
    }, "hmlgw_task", 6144, this, 17, &_taskHandle);

    if (taskCreated != pdPASS) {
        _taskHandle = NULL;
        handleFatalError("Failed to start HMLGW task");
        return;
    }

    // Start KeepAlive Task
    // OPTIMIZED: Increased priority from 15 to 16 for reliable keepalive
    BaseType_t keepAliveCreated = xTaskCreate([](void* arg) {
        static_cast<Hmlgw*>(arg)->runKeepAlive();
    }, "hmlgw_ka_task", 2048, this, 16, &_keepAliveTaskHandle);

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

    // Signal tasks to stop first
    _running = false;
    _connector->removeFrameHandler(this);

    // Close sockets to unblock any blocking accept()/recv() calls
    cleanupSockets();

    // Wait for tasks to notice _running==false and exit their loops.
    // The accept() calls have a 1-second timeout, so 2 seconds is sufficient.
    auto gracefully_delete_task = [](TaskHandle_t& task_handle, const char* task_name) {
        if (task_handle) {
            // Wait up to 2 seconds for the task to exit its loop
            for (int i = 0; i < 20; ++i) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            // Force delete - the task should have exited by now
            // (accept timeout is 1s, so 2s wait is sufficient)
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

    int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    _serverSocket.store(serverSock);
    if (serverSock < 0) {
        handleFatalError("Unable to create socket");
        return;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(serverSock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        handleFatalError("Socket unable to bind");
        return;
    }

    if (listen(serverSock, 1) != 0) {
        handleFatalError("Error occurred during listen");
        return;
    }

    // Set accept timeout to allow clean shutdown
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(serverSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ESP_LOGI(TAG, "HMLGW Server listening on port %d", _port);

    while (_running) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(serverSock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            // Timeout is normal when no client connects
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        ESP_LOGI(TAG, "Socket accepted");
        _clientSocket.store(sock);

        handleClient();

        ESP_LOGI(TAG, "Socket disconnected");
        int clientSock = _clientSocket.exchange(-1);
        if (clientSock >= 0) {
            close(clientSock);
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

    int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    _keepAliveServerSocket.store(serverSock);
    if (serverSock < 0) {
        handleFatalError("Unable to create KA socket");
        return;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(serverSock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        handleFatalError("KA Socket unable to bind");
        return;
    }

    if (listen(serverSock, 1) != 0) {
        handleFatalError("KA Error occurred during listen");
        return;
    }

    // Set accept timeout to allow clean shutdown
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(serverSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ESP_LOGI(TAG, "HMLGW KeepAlive listening on port %d", _keepAlivePort);

    while (_running) {
         struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(serverSock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
             // Timeout is normal when no client connects
             if (errno == EAGAIN || errno == EWOULDBLOCK) {
                 continue;
             }
             ESP_LOGW(TAG, "KeepAlive accept error: errno %d", errno);
             continue;
        }

        // SAFETY: Use local variable to avoid race conditions with _keepAliveClientSocket
        int clientSock = sock;
        _keepAliveClientSocket.store(sock);

        // Set recv timeout on client socket
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // KeepAlive protocol: accept connection and hold it open
        // Read and discard any data (protocol doesn't require echo)
        char rx_buffer[128];
        while (_running) {
            int len = recv(clientSock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                // Timeout or error - check if we should continue
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;  // Timeout, keep connection alive
                }
                break;  // Real error, close connection
            } else if (len == 0) {
                break;  // Client closed connection
            }
            // Data received but ignored (KeepAlive doesn't echo)
        }
        close(clientSock);
        // Only update if still the same socket (avoid race with cleanup)
        int expected = clientSock;
        if (_keepAliveClientSocket.compare_exchange_strong(expected, -1)) {
            // Successfully cleared
        }
    }
    cleanupSockets();
    ESP_LOGI(TAG, "HMLGW KeepAlive task exiting");
    // Don't delete task here - stop() will handle it
}

void Hmlgw::handleClient() {
    RingBuffer buffer;  // Ring buffer eliminates heap fragmentation
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    uint8_t temp_frame[TEMP_FRAME_BUFFER_SIZE];  // Temporary buffer for frame extraction

    // Get client socket atomically for this session
    int clientSock = _clientSocket.load();
    if (clientSock < 0) {
        return;
    }

    // Set receive timeout to allow periodic checking of _running flag
    // Socket remains BLOCKING - recv() will wait up to 1 second for data
    // This eliminates busy-wait polling (previously polled every 10ms)
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (_running) {
        // Check if client socket is still valid before recv
        if (_clientSocket.load() < 0) {
            ESP_LOGW(TAG, "Client socket invalidated");
            break;
        }

        // Blocking recv with 1-second timeout - eliminates CPU waste during idle
        int len = recv(clientSock, rx_buffer, sizeof(rx_buffer), 0);
        if (len < 0) {
            // Check if it's just a timeout (expected when no data)
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout occurred - this is normal, just continue waiting
                continue;
            }
            ESP_LOGE(TAG, "Error occurred during recv: errno %d", errno);
            break;
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
            break;
        }

        // Write new data to ring buffer
        size_t written = buffer.write(rx_buffer, len);
        if (written < (size_t)len) {
            ESP_LOGW(TAG, "Ring buffer full, dropped %d bytes", len - written);
        }

        // Ring buffer overflow protection (should never happen with 8KB buffer)
        if (buffer.size() >= buffer.capacity()) {
            ESP_LOGE(TAG, "Ring buffer full - clearing");
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

                     // SECURITY: Strict bounds check to prevent buffer overflow
                     if (dataLen > MAX_FRAME_SIZE) {
                         ESP_LOGE(TAG, "Invalid data length %d (max %d), skipping", dataLen, MAX_FRAME_SIZE);
                         processed++;
                         continue;
                     }

                     if (processed + 3 + dataLen <= buffer.size()) {
                         // SAFETY: Verify temp_frame is large enough
                         if (dataLen > TEMP_FRAME_BUFFER_SIZE) {
                             ESP_LOGE(TAG, "Frame too large for temp buffer: %d", dataLen);
                             processed++;
                             continue;
                         }
                         // We have the full frame - extract to temp buffer
                         buffer.copyData(temp_frame, processed + 3, dataLen);
                         _connector->sendFrame(temp_frame, dataLen);
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

        // Remove processed data from ring buffer (zero-copy operation)
        if (processed > 0) {
            buffer.consume(processed);
        }

        // No taskYIELD needed - blocking recv() already yields during timeout
        // This eliminates unnecessary context switches for better real-time performance
    }
}

void Hmlgw::sendDataToClient(const uint8_t* data, size_t len) {
    // SAFETY: Make a local copy of socket to avoid TOCTOU race condition
    int sock = _clientSocket.load();
    if (sock < 0) {
        return;
    }

    // Use MSG_DONTWAIT to avoid blocking on dead connections
    ssize_t sent = send(sock, data, len, MSG_DONTWAIT);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE) {
            // Connection broken or would block - close it
            ESP_LOGW(TAG, "Send failed (errno %d), closing client", errno);
        } else {
            ESP_LOGW(TAG, "Send failed, closing client: errno %d", errno);
        }
        // Only close if still the same socket (avoid closing a new connection)
        int expected = sock;
        if (_clientSocket.compare_exchange_strong(expected, -1)) {
            close(sock);
        }
    }
}

void Hmlgw::handleFrame(unsigned char* buffer, uint16_t len) {
    if (!_running) return;

    // SAFETY: Make a local copy of socket to avoid TOCTOU race condition
    int sock = _clientSocket.load();
    if (sock < 0) return;

    // FIX: Send header and data in a single send() call to prevent interleaving
    // with concurrent calls. HM radio frames are typically < 256 bytes.
    // Use a fixed stack buffer that covers all realistic frame sizes.
    // Max HM radio frame is ~512 bytes, so 1024+3 covers all valid frames.
    if (len > 1024) {
        ESP_LOGW(TAG, "Frame too large: %d bytes", len);
        return;
    }

    uint8_t frame[1024 + 3];
    frame[0] = CMD_SERIAL;
    frame[1] = (len >> 8) & 0xFF;
    frame[2] = len & 0xFF;
    memcpy(frame + 3, buffer, len);

    ssize_t sent = send(sock, frame, 3 + len, MSG_DONTWAIT);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE) {
            ESP_LOGW(TAG, "Send frame failed (errno %d), closing client", errno);
        } else {
            ESP_LOGW(TAG, "Send frame failed: errno %d", errno);
        }
        // Only close if still the same socket (avoid closing a new connection)
        int expected = sock;
        if (_clientSocket.compare_exchange_strong(expected, -1)) {
            close(sock);
        }
    }
}
