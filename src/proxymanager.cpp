#include "proxymanager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>
#include <algorithm>

static const char *TAG = "ProxyManager";
#define PROXY_PORT 3009
#define DEDUP_WINDOW_US 40000 // 40ms wait window
#define DEDUP_RETENTION_US 200000 // 200ms total retention

void _proxy_udpQueueHandlerTask(void *parameter)
{
    ((ProxyManager *)parameter)->_udpQueueHandler();
}

void _proxy_udpReceivePaket(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    while (pb != NULL)
    {
        pbuf *this_pb = pb;
        pb = pb->next;
        this_pb->next = NULL;
        if (!((ProxyManager *)arg)->_udpReceivePacket(this_pb, addr, port))
        {
            pbuf_free(this_pb);
        }
    }
}

ProxyManager::ProxyManager(Settings* settings, RadioModuleConnector* radioModule, RawUartUdpListener* rawUartListener)
    : _settings(settings), _radioModule(radioModule), _rawUartListener(rawUartListener), _tHandle(NULL), _pcb(NULL), _mutex(NULL)
{
    _mutex = xSemaphoreCreateMutex();
}

void ProxyManager::start()
{
    if (!_settings->getExperimentalFeaturesEnabled() || _settings->getProxyMode() == PROXY_MODE_STANDALONE) {
        return;
    }

    ESP_LOGI(TAG, "Starting ProxyManager in mode %d", _settings->getProxyMode());

    _udp_queue = xQueueCreate(32, sizeof(udp_event_t *));
    xTaskCreate(_proxy_udpQueueHandlerTask, "ProxyManager_UDP", 4096, this, 15, &_tHandle);

    _pcb = udp_new();
    udp_recv(_pcb, &_proxy_udpReceivePaket, (void *)this);
    _udp_bind(_pcb, IP4_ADDR_ANY, PROXY_PORT);
}

void ProxyManager::stop()
{
    if (_pcb) {
        _udp_disconnect(_pcb);
        udp_recv(_pcb, NULL, NULL);
        _udp_remove(_pcb);
        _pcb = NULL;
    }
    if (_tHandle) {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }
    // We do not delete mutex here as it might be used?
    // Actually safe to keep or delete in destructor.
}

bool ProxyManager::isActive()
{
    return _settings->getExperimentalFeaturesEnabled() && _settings->getProxyMode() != PROXY_MODE_STANDALONE;
}

// Called when local radio receives a frame
void ProxyManager::handleFrame(unsigned char *buffer, uint16_t len)
{
    if (!_settings->getExperimentalFeaturesEnabled()) {
        // Fallback should not happen if caller checks, but safety first
        return;
    }

    proxy_mode_t mode = _settings->getProxyMode();
    if (mode == PROXY_MODE_STANDALONE) {
        return; // Should pass through directly in caller
    }

    // Parse minimal metadata for protocol
    // uint32_t src = 0, dst = 0;
    // uint16_t seq = 0;
    bool is_bidcos = false;

    // Attempt to parse. Even if parsing fails, we might want to forward as raw?
    // For now, assume we need to parse to route/dedup.
    // However, for Slave->Master forwarding, we can just wrap the whole raw frame.
    // The Master does the dedup.

    int8_t rssi = -50; // Placeholder, RSSI reading requires extra info from radio module which might not be in buffer
    // Actually, HB-RF-ETH protocol doesn't explicitly pass RSSI in the frame buffer to the CCU usually?
    // Wait, the HM-MOD-RPI-PCB appends RSSI byte at the end for some frames.
    // Let's assume the buffer contains everything.

    if (mode == PROXY_MODE_SLAVE) {
        // Encapsulate and send to Master
        sendToMaster(buffer, len, rssi);
    } else if (mode == PROXY_MODE_MASTER) {
        // Treat as if received from "Self-Slave"
        // Feed into dedup logic
        // To ensure thread safety, we inject this into the UDP queue loop (same as slave packets)
        // by sending a special loopback packet or by just handling it here with mutex.

        // However, to ensure proper dedup timing, we should put it into the queue.
        // But allocating pbuf for local traffic is overhead.

        // Alternative: Process dedup logic protected by mutex.
        // But dedup requires waiting (deferred processing).
        // We cannot block the radio task.

        // So we MUST send it to the Proxy Queue (managed by `_tHandle` task)
        // which can handle the timing/delay.

        // Create a fake UDP event for local frame
        // Use a special "IP" or flag to indicate local

        pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, sizeof(proxy_frame_header_t) + len, PBUF_RAM);
        if (pb) {
            proxy_frame_header_t* header = (proxy_frame_header_t*)pb->payload;
            header->type = 0; // RX_FRAME
            header->src_id = 0; // Filled by parsing inside queue handler if needed, or here.
            header->dst_id = 0;
            header->seq = 0;
            header->rssi = rssi;
            header->ts_us = esp_timer_get_time();
            header->len = len;

            // Try to pre-parse here to fill src/dst for consistency
            parseFrame(buffer, len, header->src_id, header->dst_id, header->seq, is_bidcos);

            memcpy((uint8_t*)pb->payload + sizeof(proxy_frame_header_t), buffer, len);

            // Send to our own queue
            udp_event_t *e = (udp_event_t *)malloc(sizeof(udp_event_t));
            if (e) {
                e->pb = pb;
                e->addr.addr = IPADDR_ANY; // Indicator for local
                e->port = 0;

                if (xQueueSend(_udp_queue, &e, 0) != pdPASS) {
                    free(e);
                    pbuf_free(pb);
                }
            } else {
                pbuf_free(pb);
            }
        }
    }
}

bool ProxyManager::handleCCUTX(unsigned char *buffer, uint16_t len)
{
    if (!_settings->getExperimentalFeaturesEnabled() || _settings->getProxyMode() != PROXY_MODE_MASTER) {
        return false;
    }

    // CCU wants to send.
    // Parse destination.
    uint32_t src, dst;
    uint16_t seq;
    bool is_bidcos;

    if (!parseFrame(buffer, len, src, dst, seq, is_bidcos)) {
        // Cannot parse, fallback to local send
        return false;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    // Check routing table
    auto it = _routing_table.find(dst);
    bool routed = false;
    ip4_addr_t slave_ip;
    if (it != _routing_table.end()) {
        slave_ip = it->second.slave_ip;
        routed = true;
    }
    xSemaphoreGive(_mutex);

    if (routed) {
        // If route is local (Master itself), return false to trigger local send
        if (slave_ip.addr == 0) {
            return false;
        }

        // Send to slave
        // Construct TX_CMD
        pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, sizeof(proxy_frame_header_t) + len, PBUF_RAM);
        if (pb) {
             proxy_frame_header_t* header = (proxy_frame_header_t*)pb->payload;
             header->type = 1; // TX_CMD
             header->dst_id = dst;
             header->len = len;
             memcpy((uint8_t*)pb->payload + sizeof(proxy_frame_header_t), buffer, len);

             ip_addr_t addr;
             addr.type = IPADDR_TYPE_V4;
             addr.u_addr.ip4 = slave_ip;

             udp_sendto(_pcb, pb, &addr, PROXY_PORT);
             pbuf_free(pb);
             return true; // Handled
        }
    } else {
        // Broadcast to all slaves (and return false so we ALSO send locally? or return true and handle local sending here?)
        // If we broadcast, we should send to all slaves AND local radio.
        // The safest fallback for "unknown destination" is to flood.

        broadcastToSlaves(buffer, len, 0); // 0 = don't exclude anyone

        // Return false to let the caller (RawUartUdpListener) send to local radio module as well.
        return false;
    }

    return false; // Not routed (send locally)
}

void ProxyManager::broadcastToSlaves(unsigned char *buffer, uint16_t len, uint32_t exclude_src_id) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    // Create a list of unique IPs to send to
    std::vector<ip4_addr_t> targets;
    for (auto const& [key, val] : _routing_table) {
        bool exists = false;
        for (const auto& ip : targets) {
            if (ip.addr == val.slave_ip.addr) { exists = true; break; }
        }
        if (!exists) targets.push_back(val.slave_ip);
    }
    xSemaphoreGive(_mutex);

    if (targets.empty()) return;

    pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, sizeof(proxy_frame_header_t) + len, PBUF_RAM);
    if (!pb) return;

    proxy_frame_header_t* header = (proxy_frame_header_t*)pb->payload;
    header->type = 1; // TX_CMD
    header->dst_id = 0; // Broadcast
    header->len = len;
    memcpy((uint8_t*)pb->payload + sizeof(proxy_frame_header_t), buffer, len);

    for (const auto& ip : targets) {
        ip_addr_t addr;
        addr.type = IPADDR_TYPE_V4;
        addr.u_addr.ip4 = ip;
        udp_sendto(_pcb, pb, &addr, PROXY_PORT);
    }
    pbuf_free(pb);
}

void ProxyManager::sendToMaster(unsigned char *buffer, uint16_t len, int8_t rssi)
{
    ip4_addr_t masterIP = _settings->getMasterIP();
    if (masterIP.addr == IPADDR_ANY) return;

    uint32_t src = 0, dst = 0;
    uint16_t seq = 0;
    bool is_bidcos = false;
    parseFrame(buffer, len, src, dst, seq, is_bidcos); // Try to parse to populate header

    pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, sizeof(proxy_frame_header_t) + len, PBUF_RAM);
    if (pb) {
        proxy_frame_header_t* header = (proxy_frame_header_t*)pb->payload;
        header->type = 0; // RX_FRAME
        header->src_id = src;
        header->dst_id = dst;
        header->seq = seq;
        header->rssi = rssi;
        header->ts_us = esp_timer_get_time();
        header->len = len;

        memcpy((uint8_t*)pb->payload + sizeof(proxy_frame_header_t), buffer, len);

        ip_addr_t addr;
        addr.type = IPADDR_TYPE_V4;
        addr.u_addr.ip4 = masterIP;

        udp_sendto(_pcb, pb, &addr, PROXY_PORT);
        pbuf_free(pb);
    }
}

void ProxyManager::_udpQueueHandler()
{
    udp_event_t *event = NULL;
    for (;;)
    {
        // TODO: Use a timeout to handle dedup expiration logic if we buffer
        if (xQueueReceive(_udp_queue, &event, 10 / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (event->pb && event->pb->len >= sizeof(proxy_frame_header_t)) {
                proxy_frame_header_t* header = (proxy_frame_header_t*)event->pb->payload;
                uint8_t* payload = (uint8_t*)event->pb->payload + sizeof(proxy_frame_header_t);

                if (header->type == 0 && _settings->getProxyMode() == PROXY_MODE_MASTER) {
                    // RX Frame from Slave (or Local)

                    // Deduplication Logic with "Best RSSI" buffering
                    uint32_t key = hashFrame(payload, header->len);
                    uint64_t now = esp_timer_get_time();

                    xSemaphoreTake(_mutex, portMAX_DELAY);
                    auto it = _dedup_cache.find(key);

                    if (it == _dedup_cache.end()) {
                        // New Frame
                        DedupEntry entry;
                        entry.first_seen_us = now;
                        entry.deadline_us = now + DEDUP_WINDOW_US;
                        entry.sent = false;
                        entry.best_rssi = header->rssi;
                        entry.best_source_ip = event->addr;
                        entry.src_id = header->src_id; // Store for eventual routing update
                        entry.original_len = header->len;
                        entry.frame_data.assign(payload, payload + header->len);

                        _dedup_cache[key] = entry;
                    } else {
                        // Existing Frame
                        DedupEntry& entry = it->second;
                        if (!entry.sent) {
                            // Still buffering, check if this signal is better
                            if (header->rssi > entry.best_rssi) {
                                entry.best_rssi = header->rssi;
                                entry.best_source_ip = event->addr;
                                // We don't need to update payload as it's identical (by definition of hash)
                                // But technically header might have differences? No, hash includes data.
                            }
                        } else {
                            // Already sent, ignore (duplicate suppression)
                        }
                    }
                    xSemaphoreGive(_mutex);

                } else if (header->type == 1 && _settings->getProxyMode() == PROXY_MODE_SLAVE) {
                    // TX Cmd from Master
                    _radioModule->sendFrame(payload, header->len);
                }
            }

            pbuf_free(event->pb);
            free(event);
        }

        // Process pending frames (Send Best RSSI)
        uint64_t now = esp_timer_get_time();
        xSemaphoreTake(_mutex, portMAX_DELAY);

        for (auto it = _dedup_cache.begin(); it != _dedup_cache.end(); ) {
            DedupEntry& entry = it->second;

            // Send if deadline reached and not sent
            if (!entry.sent && now >= entry.deadline_us) {
                // Send to CCU
                _rawUartListener->sendMessage(7, entry.frame_data.data(), entry.original_len);
                entry.sent = true;

                // Update Routing Table with the WINNER (Best RSSI)
                // Only if src_id is valid (non-zero)
                if (entry.src_id != 0) {
                    RoutingEntry route;
                    route.slave_ip = entry.best_source_ip;
                    route.last_rssi = entry.best_rssi;
                    route.last_seen_us = now;
                    _routing_table[entry.src_id] = route;
                }
            }

            // Cleanup old entries
            if (now > entry.first_seen_us + DEDUP_RETENTION_US) {
                 it = _dedup_cache.erase(it);
            } else {
                ++it;
            }
        }
        xSemaphoreGive(_mutex);
    }
}

bool ProxyManager::_udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    udp_event_t *e = (udp_event_t *)malloc(sizeof(udp_event_t));
    if (!e) return false;

    e->pb = pb;
    // Copy address (assuming IPv4)
    e->addr.addr = addr->u_addr.ip4.addr;
    e->port = port;

    if (xQueueSend(_udp_queue, &e, 0) != pdPASS) {
        free(e);
        return false;
    }
    return true;
}

bool ProxyManager::parseFrame(unsigned char *buffer, uint16_t len, uint32_t &src, uint32_t &dst, uint16_t &seq, bool &is_bidcos)
{
    HMFrame frame;
    if (!HMFrame::TryParse(buffer, len, &frame)) {
        return false;
    }

    // Check destination to distinguish protocols
    // HM_DST_TRX (0x01) usually implies BidCos
    // HM_DST_HMIP (0x02) implies HmIP

    if (frame.destination == HM_DST_TRX && frame.data_len >= 10) {
        // BidCos logic
        seq = frame.data[1];
        src = (frame.data[4] << 16) | (frame.data[5] << 8) | frame.data[6];
        dst = (frame.data[7] << 16) | (frame.data[8] << 8) | frame.data[9];
        is_bidcos = true;
        return true;
    }

    // For HmIP or unknown, we return false so that:
    // 1. Dedup uses Hash (safe)
    // 2. Routing uses Broadcast (safe)

    return false;
}

uint32_t ProxyManager::hashFrame(unsigned char *buffer, uint16_t len)
{
    // Jenkins One-at-a-time hash or simple FNV-1a
    // We hash the *payload* of the HMFrame
    HMFrame frame;
    if (!HMFrame::TryParse(buffer, len, &frame)) {
        // Fallback: hash entire buffer if parse fails
        uint32_t hash = 0x811c9dc5;
        for (int i = 0; i < len; ++i) {
            hash ^= buffer[i];
            hash *= 0x01000193;
        }
        return hash;
    }

    // Hash the radio payload
    uint32_t hash = 0x811c9dc5;

    // Note: For BidCos, the last byte might be RSSI in some modes?
    // But usually RSSI is not part of the frame data we get here unless configured.
    // We hash the whole data block.
    // If multiple receivers get the same frame, the frame data should be identical.

    for (int i = 0; i < frame.data_len; ++i) {
        hash ^= frame.data[i];
        hash *= 0x01000193;
    }
    return hash;
}
