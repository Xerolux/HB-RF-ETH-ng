/*
 *  hmlgw.h - HomeMatic LAN Gateway Emulation
 *
 *  Part of HB-RF-ETH-ng
 */

#ifndef HMLGW_H
#define HMLGW_H

#include "radiomoduleconnector.h"
#include <lwip/sockets.h>
#include <atomic>
#include <vector>

class Hmlgw : public FrameHandler {
private:
    RadioModuleConnector* _connector;
    std::atomic<bool> _running{false};
    std::atomic<int> _serverSocket{-1};
    std::atomic<int> _clientSocket{-1};
    TaskHandle_t _taskHandle;
    uint16_t _port;
    uint16_t _keepAlivePort;

    // KeepAlive socket
    std::atomic<int> _keepAliveServerSocket{-1};
    std::atomic<int> _keepAliveClientSocket{-1};
    TaskHandle_t _keepAliveTaskHandle;

    void cleanupSockets();
    void handleFatalError(const char* reason);
    void run();
    void runKeepAlive();
    void handleClient();
    void sendDataToClient(const uint8_t* data, size_t len);

public:
    Hmlgw(RadioModuleConnector* connector, uint16_t port, uint16_t keepAlivePort);
    virtual ~Hmlgw();

    void start();
    void stop();

    // FrameHandler interface
    void handleFrame(unsigned char* buffer, uint16_t len) override;
};

#endif
