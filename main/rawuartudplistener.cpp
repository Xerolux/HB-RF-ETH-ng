/*
 *  rawuartudplistener.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "rawuartudplistener.h"
#include "hmframe.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <vector>
#include "udphelper.h"
#include "metrics.h"

static const char *TAG = "RawUartUdpListener";

// Process-wide counters exposed via the Prometheus exporter.
//   hbrfeth_udp_rx_frames_total    — frames received from the CCU
//   hbrfeth_udp_tx_frames_total    — frames sent to the CCU
//   hbrfeth_udp_keepalive_total    — keepalive probes sent
//   hbrfeth_udp_drop_total         — received frames dropped (queue / parse)
// Defined once at file scope so registration happens on first use.
static MetricsCounter g_rx_frames("hbrfeth_udp_rx_frames_total",
                                  "Total UDP frames received from CCU");
static MetricsCounter g_tx_frames("hbrfeth_udp_tx_frames_total",
                                  "Total UDP frames sent to CCU");
static MetricsCounter g_keepalives("hbrfeth_udp_keepalive_total",
                                   "Keepalive probes sent");
static MetricsCounter g_rx_drops("hbrfeth_udp_drop_total",
                                 "Received UDP frames dropped (queue full / parse error)");

void _raw_uart_udpQueueHandlerTask(void *parameter)
{
    ((RawUartUdpListener *)parameter)->_udpQueueHandler();
}

void IRAM_ATTR _raw_uart_udpReceivePaket(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    // A pbuf chain is one UDP datagram, not multiple packets. Transfer the
    // complete chain to the worker; splitting it corrupts larger CCU frames.
    if (pb != NULL && !((RawUartUdpListener *)arg)->_udpReceivePacket(pb, addr, port))
    {
        pbuf_free(pb);
    }
}

RawUartUdpListener::RawUartUdpListener(RadioModuleConnector *radioModuleConnector) : _radioModuleConnector(radioModuleConnector), _lastReceivedKeepAlive(0), _pcb(NULL), _udp_queue(NULL), _tHandle(NULL)
{
    atomic_init(&_connectionStarted, false);
    atomic_init(&_remotePort, (ushort)0);
    atomic_init(&_remoteAddress, 0u);
    atomic_init(&_counter, 0);
    atomic_init(&_endpointConnectionIdentifier, 1);
}

void RawUartUdpListener::handlePacket(pbuf *pb, ip4_addr_t addr, uint16_t port)
{
    size_t length = pb->tot_len;
    if (length < 4 || length > 1500)
    {
        ESP_LOGE(TAG, "Received invalid raw-uart packet, length %zu", length);
        g_rx_drops.inc();
        return;
    }

    std::vector<unsigned char> data(length);
    unsigned char response_buffer[3];

    if (pbuf_copy_partial(pb, data.data(), length, 0) != length) {
        ESP_LOGE(TAG, "Could not linearize raw-uart packet, length %zu", length);
        g_rx_drops.inc();
        return;
    }

    if (data[0] != 0 && (addr.addr != atomic_load(&_remoteAddress) || port != atomic_load(&_remotePort)))
    {
        ESP_LOGE(TAG, "Received raw-uart packet from invalid address.");
        g_rx_drops.inc();
        return;
    }

    /* Read the trailing CRC16 with memcpy: the data pointer + length comes from
     * a network pbuf and is not guaranteed to be 2-byte aligned, and casting an
     * unsigned char* to uint16_t* also violates strict aliasing. */
    uint16_t received_crc;
    memcpy(&received_crc, data.data() + length - 2, sizeof(uint16_t));
    if (received_crc != htons(HMFrame::crc(data.data(), length - 2)))
    {
        ESP_LOGE(TAG, "Received raw-uart packet with invalid crc.");
        g_rx_drops.inc();
        return;
    }

    // Valid frame received from the CCU.
    g_rx_frames.inc();

    atomic_store(&_lastReceivedKeepAlive, (int64_t)esp_timer_get_time());

    switch (data[0])
    {
    case 0: // connect
        if (length == 5 && data[2] == 1)
        { // protocol version 1
            atomic_fetch_add(&_endpointConnectionIdentifier, 2);
            atomic_store(&_remotePort, (ushort)0);
            atomic_store(&_connectionStarted, false);
            atomic_store(&_remoteAddress, addr.addr);
            atomic_store(&_remotePort, port);
            _radioModuleConnector->setLED(true, true, false);

            ESP_LOGI(TAG, "CCU 3 connected from %s:%u", ip4addr_ntoa(&addr), port);

            response_buffer[0] = 1;
            response_buffer[1] = data[1];
            sendMessage(0, response_buffer, 2);
        }
        else if (length == 6 && data[2] == 2) {
            int endpointConnectionIdentifier  = atomic_load(&_endpointConnectionIdentifier);

            if (data[3] == 0)
            {
                endpointConnectionIdentifier += 2;
                atomic_store(&_endpointConnectionIdentifier, endpointConnectionIdentifier);
                atomic_store(&_connectionStarted, false);
            }
            else if (data[3] != (endpointConnectionIdentifier & 0xff))
            {
                // Client has a stale identifier (e.g. after device reboot). Accept the reconnect
                // by adopting the client's identifier so the CCU can reconnect without restart.
                // Logged at INFO: this is expected once per boot and semantically safe.
                ESP_LOGI(TAG, "Received raw-uart reconnect packet with unexpected endpoint identifier %d (expected %d) - adopting client identifier", data[3], endpointConnectionIdentifier);
                endpointConnectionIdentifier = data[3];
                atomic_store(&_endpointConnectionIdentifier, endpointConnectionIdentifier);
                atomic_store(&_connectionStarted, false);
            }

            atomic_store(&_remotePort, (ushort)0);
            atomic_store(&_remoteAddress, addr.addr);
            atomic_store(&_remotePort, port);
            _radioModuleConnector->setLED(true, true, false);

            ESP_LOGI(TAG, "CCU 3 reconnected from %s:%u", ip4addr_ntoa(&addr), port);

            response_buffer[0] = 2;
            response_buffer[1] = data[1];
            response_buffer[2] = endpointConnectionIdentifier;
            sendMessage(0, response_buffer, 3);
        }
        else {
            ESP_LOGE(TAG, "Received invalid raw-uart connect packet, length %d", length);
            return;
        }
        break;

    case 1: // disconnect
        ESP_LOGI(TAG, "CCU 3 disconnected");
        atomic_store(&_connectionStarted, false);
        atomic_store(&_remotePort, (ushort)0);
        atomic_store(&_remoteAddress, 0u);
        _radioModuleConnector->setLED(false, false, false);
        break;

    case 2: // keep alive
        sendMessage(2, NULL, 0);
        break;

    case 3: // LED
        if (length != 5)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart LED packet, length %d", length);
            return;
        }

        _radioModuleConnector->setLED(data[2] & 1, data[2] & 2, data[2] & 4);
        break;

    case 4: // Reset
        if (length != 4)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart reset packet, length %d", length);
            return;
        }

        _radioModuleConnector->resetModule();
        break;

    case 5: // Start connection
        if (length != 4)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart startconn packet, length %d", length);
            return;
        }

        atomic_store(&_connectionStarted, true);
        break;

    case 6: // End connection
        if (length != 4)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart endconn packet, length %d", length);
            return;
        }

        atomic_store(&_connectionStarted, false);
        break;

    case 7: // Frame
        if (length < 5)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart frame packet, length %d", length);
            return;
        }

        _radioModuleConnector->sendFrame(&data[2], length - 4);
        break;

    default:
        ESP_LOGE(TAG, "Received invalid raw-uart packet with unknown type %d", data[0]);
        break;
    }
}

ip4_addr_t RawUartUdpListener::getConnectedRemoteAddress()
{
    uint16_t port = atomic_load(&_remotePort);
    uint32_t address = atomic_load(&_remoteAddress);

    if (port)
    {
        ip4_addr_t res{ .addr = address };
        return res;
    }
    else
    {
        return (ip4_addr_t{.addr = IPADDR_ANY});
    }
}

void RawUartUdpListener::sendMessage(unsigned char command, unsigned char *buffer, size_t len)
{
    uint16_t port = atomic_load(&_remotePort);
    uint32_t address = atomic_load(&_remoteAddress);

    if (!port)
        return;

    // Every command type is also a downstream frame to the CCU.
    g_tx_frames.inc();

    pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, len + 4, PBUF_RAM);
    if (!pb) {
        ESP_LOGE(TAG, "Failed to allocate pbuf for sendMessage");
        return;
    }
    unsigned char *sendBuffer = (unsigned char *)pb->payload;

    ip_addr_t addr;
    addr.type = IPADDR_TYPE_V4;
    addr.u_addr.ip4.addr = address;

    sendBuffer[0] = command;
    sendBuffer[1] = (unsigned char)atomic_fetch_add(&_counter, 1);

    if (len)
        memcpy(sendBuffer + 2, buffer, len);

    /* Store the CRC16 with memcpy: sendBuffer + len + 2 is not guaranteed to be
     * 2-byte aligned (len is caller-controlled), and the cast violates strict
     * aliasing. */
    uint16_t crc_net = htons(HMFrame::crc(sendBuffer, len + 2));
    memcpy(sendBuffer + len + 2, &crc_net, sizeof(uint16_t));

    _udp_sendto(_pcb, pb, &addr, port);
    pbuf_free(pb);
}

void RawUartUdpListener::handleFrame(unsigned char *buffer, uint16_t len)
{
    if (!atomic_load(&_connectionStarted))
        return;

    if (len > (1500 - 28 - 4))
    {
        ESP_LOGE(TAG, "Received oversized frame from radio module, length %d", len);
        return;
    }

    sendMessage(7, buffer, len);
}

void RawUartUdpListener::start()
{
    if (_tHandle || _pcb || _udp_queue) return;

    _udp_queue = xQueueCreate(64, sizeof(udp_event_t *));
    if (_udp_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create UDP queue - out of memory");
        return;
    }
    _pcb = _udp_new();
    if (!_pcb || _udp_bind(_pcb, IP4_ADDR_ANY, 3008) != ERR_OK) {
        ESP_LOGE(TAG, "Failed to create/bind UDP listener on port 3008");
        _udp_remove(_pcb);
        _pcb = NULL;
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
        return;
    }
    _udp_recv(_pcb, &_raw_uart_udpReceivePaket, (void *)this);

    if (xTaskCreate(_raw_uart_udpQueueHandlerTask, "RawUartUdpListener_UDP_QueueHandler",
                    4096, this, 15, &_tHandle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP listener task");
        _udp_recv(_pcb, NULL, NULL);
        _udp_remove(_pcb);
        _pcb = NULL;
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
        return;
    }

    _radioModuleConnector->setFrameHandler(this, false);

    ESP_LOGI(TAG, "UDP listener started on port 3008");
}

void RawUartUdpListener::stop()
{
    _radioModuleConnector->setFrameHandler(NULL, false);
    if (_pcb) {
        _udp_recv(_pcb, NULL, NULL);
        _udp_disconnect(_pcb);
        _udp_remove(_pcb);
        _pcb = NULL;
    }
    if (_tHandle) {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }
    if (_udp_queue) {
        udp_event_t *event = NULL;
        while (xQueueReceive(_udp_queue, &event, 0) == pdTRUE) {
            pbuf_free(event->pb);
            free(event);
        }
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
    }
}

void RawUartUdpListener::_udpQueueHandler()
{
    udp_event_t *event = NULL;
    int64_t nextKeepAliveSentOut = esp_timer_get_time();

    for (;;)
    {
        if (xQueueReceive(_udp_queue, &event, (TickType_t)pdMS_TO_TICKS(10)) == pdTRUE)
        {
            handlePacket(event->pb, event->addr, event->port);
            pbuf_free(event->pb);
            free(event);
        }

        if (atomic_load(&_remotePort) != 0)
        {
            int64_t now = esp_timer_get_time();

            if (now > atomic_load(&_lastReceivedKeepAlive) + 10000000)
            { // 10 sec
                ESP_LOGW(TAG, "CCU 3 connection timed out (no keep-alive for 10 seconds)");
                atomic_store(&_connectionStarted, false);
                atomic_store(&_remotePort, (ushort)0);
                atomic_store(&_remoteAddress, 0u);
                _radioModuleConnector->setLED(true, false, false);
            }
            else if (now > nextKeepAliveSentOut)
            {
                nextKeepAliveSentOut = now + 1000000; // 1sec
                g_keepalives.inc();
                sendMessage(2, NULL, 0);
            }
        }
    }
}

bool IRAM_ATTR RawUartUdpListener::_udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    udp_event_t *e = (udp_event_t *)malloc(sizeof(udp_event_t));
    if (!e)
    {
        return false;
    }

    e->pb = pb;

    // Use the source address and port provided directly by LwIP in the callback
    // instead of reading raw pbuf header memory (which is unsafe and fragile).
    e->addr.addr = addr->u_addr.ip4.addr;
    e->port = port;

    if (xQueueSend(_udp_queue, &e, 0) != pdPASS)
    {
        ESP_LOGW(TAG, "UDP queue full, dropping packet");
        g_rx_drops.inc();
        free((void *)(e));
        return false;
    }
    return true;
}

/*
Index 0 - Type: 0-Connect, 1-Disconnect, 2-KeepAlive, 3-LED, 4-StartConn, 5-StopConn, 6-Reset, 7-Frame
Index 1 - Counter
Index 2..n-2 - Payload
Index n-2,n-1 - CRC16

Payload:
  Keepalive: Empty
  Connect: 1 Byte: Protocol version
  LED: 1 Byte: Bit 0 R, Bit 1 G, Bit 2 B
  Reset: Empty
  Frame: Frame-Data
*/
