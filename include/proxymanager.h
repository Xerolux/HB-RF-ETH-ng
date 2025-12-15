#pragma once

#include "settings.h"
#include "radiomoduleconnector.h"
#include "rawuartudplistener.h"
#include "udphelper.h"
#include "hmframe.h"
#include <map>
#include <vector>
#include "freertos/semphr.h"

// Forward declaration
class RawUartUdpListener;

// Structure for Proxy UDP packet (Frame from Slave to Master or Cmd from Master to Slave)
#pragma pack(push, 1)
struct proxy_frame_header_t {
    uint8_t type; // 0 = RX_FRAME (Slave->Master), 1 = TX_CMD (Master->Slave)
    // Common fields
    uint32_t src_id;
    uint32_t dst_id;
    uint16_t seq;
    int8_t rssi;
    uint8_t lqi;
    uint64_t ts_us; // timestamp
    uint16_t len;
    // Payload follows
};
#pragma pack(pop)

class ProxyManager : public FrameHandler {
public:
    ProxyManager(Settings* settings, RadioModuleConnector* radioModule, RawUartUdpListener* rawUartListener);

    void start();
    void stop();

    bool isActive();

    // From FrameHandler (called by RadioModuleConnector)
    void handleFrame(unsigned char *buffer, uint16_t len) override;

    // Called by RawUartUdpListener when CCU sends a packet (TX path)
    // Returns true if handled (sent to proxy slave), false if should be sent locally
    bool handleCCUTX(unsigned char *buffer, uint16_t len);

private:
    Settings* _settings;
    RadioModuleConnector* _radioModule;
    RawUartUdpListener* _rawUartListener;

    TaskHandle_t _tHandle;
    QueueHandle_t _udp_queue;
    udp_pcb* _pcb;

    SemaphoreHandle_t _mutex;

    // Master state
    struct DedupEntry {
        uint64_t first_seen_us;
        uint64_t deadline_us;
        bool sent;
        int8_t best_rssi;
        ip4_addr_t best_source_ip;
        uint32_t src_id; // For routing table update
        std::vector<uint8_t> frame_data;
        uint16_t original_len;
    };
    // Key: Hash of payload (simple approach for unknown protocols)
    std::map<uint32_t, DedupEntry> _dedup_cache;

    struct RoutingEntry {
        ip4_addr_t slave_ip;
        int8_t last_rssi;
        uint64_t last_seen_us;
    };
    // Routing table: dst_id -> RoutingEntry
    std::map<uint32_t, RoutingEntry> _routing_table;

    void broadcastToSlaves(unsigned char *buffer, uint16_t len, uint32_t exclude_src_id);
    void _udpQueueHandler();
    bool _udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port);
    friend void _proxy_udpQueueHandlerTask(void *parameter);
    friend void _proxy_udpReceivePaket(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port);

    void sendToMaster(unsigned char *buffer, uint16_t len, int8_t rssi);

    // Helpers
    bool parseFrame(unsigned char *buffer, uint16_t len, uint32_t &src, uint32_t &dst, uint16_t &seq, bool &is_bidcos);
    uint32_t hashFrame(unsigned char *buffer, uint16_t len);
};
