/*
 *  rawuartudplistener.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#pragma once

#include "lwip/opt.h"
#include "lwip/inet.h"
#include "lwip/udp.h"
#include "lwip/priv/tcpip_priv.h"
#include "esp_err.h"
#include "freertos/semphr.h"
#include <atomic>
#define _Atomic(X) std::atomic<X>
#include "radiomoduleconnector.h"

class RawUartUdpListener : FrameHandler
{
private:
    RadioModuleConnector *_radioModuleConnector;
    std::atomic<uint> _remoteAddress;
    std::atomic<ushort> _remotePort;
    std::atomic<bool> _connectionStarted;
    std::atomic<int> _counter;
    std::atomic<int> _endpointConnectionIdentifier;
    std::atomic<udp_pcb *> _pcb{NULL};
    std::atomic<QueueHandle_t> _udp_queue{NULL};
    std::atomic<TaskHandle_t> _tHandle{NULL};
    std::atomic<bool> _stopRequested{true};
    // Closes the race between a radio-frame callback which already entered
    // sendMessage() and worker-owned PCB teardown during cooperative stop.
    std::atomic<uint32_t> _activeSenders{0};
    StaticSemaphore_t _lifecycleMutexStorage = {};
    SemaphoreHandle_t _lifecycleMutex = NULL;

    bool handlePacket(pbuf *pb, ip4_addr_t addr, uint16_t port);
    void sendMessage(unsigned char command, unsigned char *buffer, size_t len);

public:
    RawUartUdpListener(RadioModuleConnector *radioModuleConnector);

    void handleFrame(unsigned char *buffer, uint16_t len);
    void handleEvent();

    ip4_addr_t getConnectedRemoteAddress();

    // True while a CCU raw-uart session is bound (remote port nonzero).
    // Backs raw_uart_session_active(); see the liveness sentinel notes.
    bool sessionActive() const { return _remotePort.load(std::memory_order_relaxed) != 0; }

    void start();
    esp_err_t stop();

    void _udpQueueHandler();
    bool _udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port);
};

// Depth of the inbound datagram queue. Exported so the diagnostics view can
// show occupancy against capacity rather than a bare number nobody can judge.
#define RAW_UART_UDP_QUEUE_DEPTH 64

// Snapshot of the CCU relay instrumentation. Exported through the Prometheus
// endpoint, the MQTT status batch and the WebUI diagnostics page - the last of
// these because the people reporting delayed switching commands and disturbed
// device communication (#411 / #362 / #447) run neither a scraper nor a broker,
// and were therefore the only ones who could not see the numbers.
//
// The frame totals matter as much as the failure counts: "17 drops" means
// nothing without knowing whether 17 or 17 million frames went through.
typedef struct {
    uint32_t queue_wait_max_us; // high-water since boot or last reset
    uint32_t queue_depth_max;   // high-water queue occupancy
    uint64_t wait_over_10ms;    // datagrams delayed 10 ms .. 100 ms
    uint64_t wait_over_100ms;   // datagrams delayed 100 ms .. 1 s
    uint64_t wait_over_1s;      // datagrams delayed more than 1 s
    uint64_t drops;             // datagrams dropped (queue full / invalid)
    uint64_t rx_frames;         // datagrams accepted from the CCU
    uint64_t tx_frames;         // datagrams sent to the CCU
    uint64_t keepalives;        // keepalive probes sent
    uint64_t tx_alloc_fail;     // frames to the CCU lost: no pbuf available
    uint64_t tx_send_err;       // frames to the CCU rejected by lwIP/Ethernet
} raw_uart_latency_t;

void raw_uart_get_latency(raw_uart_latency_t *out);

// tcpip liveness sentinel (#362): uptime ms of the most recent datagram that
// reached the lwIP receive callback. Written from tcpip_thread context on
// every accepted datagram; readers never touch the network. A CCU session
// that is still active (session_active() below) while this timestamp goes
// stale means the worker itself is frozen inside a blocking send and the
// tcpip thread is no longer delivering — the initiator state this firmware
// must detect and self-heal from.
uint32_t raw_uart_last_tcpip_rx_ms(void);

// True while a CCU raw-uart session is bound (remote port nonzero). The
// liveness check is gated on this: without a session, a stale timestamp is
// expected, not a fault.
bool raw_uart_session_active(void);

// Clear the high-water marks so an operator can observe a fresh window after
// changing something, instead of reading a mark set days earlier. The
// bucket counters are monotonic and deliberately not reset.
void raw_uart_reset_latency_high_water(void);
