/*
 *  rawuartudplistener.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 5.1 and modern toolchains
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
#include <esp_timer.h>
#include <string.h>
#include "udphelper.h"

static const char *TAG = "RawUartUdpListener";

void _raw_uart_udpQueueHandlerTask(void *parameter)
{
    ((RawUartUdpListener *)parameter)->_udpQueueHandler();
}

void _raw_uart_udpReceivePaket(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    while (pb != NULL)
    {
        pbuf *this_pb = pb;
        pb = pb->next;
        this_pb->next = NULL;
        if (!((RawUartUdpListener *)arg)->_udpReceivePacket(this_pb, addr, port))
        {
            pbuf_free(this_pb);
        }
    }
}

RawUartUdpListener::RawUartUdpListener(RadioModuleConnector *radioModuleConnector, Settings *settings, DTLSEncryption *dtls)
    : _radioModuleConnector(radioModuleConnector), _settings(settings), _dtls(dtls),
      _lastReceivedKeepAlive(0), _pcb(NULL), _udp_queue(NULL), _tHandle(NULL)
{
    atomic_init(&_connectionStarted, false);
    atomic_init(&_remotePort, (ushort)0);
    atomic_init(&_remoteAddress, 0u);
    atomic_init(&_counter, 0);
    atomic_init(&_endpointConnectionIdentifier, 1);
    atomic_init(&_dtlsEnabled, false);

    // Check if DTLS is enabled
    if (_settings && _dtls && _dtls->isEnabled())
    {
        atomic_store(&_dtlsEnabled, true);
        ESP_LOGI(TAG, "DTLS encryption enabled for Raw UART UDP");
    }
}

void RawUartUdpListener::handlePacket(pbuf *pb, ip4_addr_t addr, uint16_t port)
{
    size_t length = pb->len;
    unsigned char *data = (unsigned char *)(pb->payload);
    unsigned char response_buffer[3];

    if (length < 4)
    {
        ESP_LOGE(TAG, "Received invalid raw-uart packet, length %d", length);
        return;
    }

    if (data[0] != 0 && (addr.addr != atomic_load(&_remoteAddress) || port != atomic_load(&_remotePort)))
    {
        ESP_LOGE(TAG, "Received raw-uart packet from invalid address.");
        return;
    }

    // SAFETY: Use byte-by-byte comparison to avoid unaligned access
    // CRC is stored as big-endian at the end of the packet
    uint16_t received_crc = ((uint16_t)data[length - 2] << 8) | data[length - 1];
    if (received_crc != HMFrame::crc(data, length - 2))
    {
        ESP_LOGE(TAG, "Received raw-uart packet with invalid crc (expected %04x, got %04x).",
                 HMFrame::crc(data, length - 2), received_crc);
        return;
    }

    _lastReceivedKeepAlive = esp_timer_get_time();

    switch (data[0])
    {
    case 0: // connect
        if (length == 5 && data[2] == 1)
        { // protocol version 1
            atomic_fetch_add(&_endpointConnectionIdentifier, 2);
            atomic_store(&_connectionStarted, false);
            // FIX: Update address before port to prevent sendMessage from sending
            // to old address with new port. Port acts as the "connection valid" flag.
            atomic_store(&_remoteAddress, addr.addr);
            atomic_store(&_remotePort, port);
            _radioModuleConnector->setLED(true, true, false);
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
                ESP_LOGW(TAG, "Received raw-uart reconnect packet with invalid endpoint identifier %d, should be %d. Sending response to force sync.", data[3], endpointConnectionIdentifier);
            }

            // FIX: Update address before port to prevent sendMessage from using
            // stale address with new port.
            atomic_store(&_remoteAddress, addr.addr);
            atomic_store(&_remotePort, port);
            _radioModuleConnector->setLED(true, true, false);
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
        // FIX: Clear port first (checked by sendMessage), then state, then address.
        // This ensures sendMessage won't send to a partially-cleared connection.
        atomic_store(&_remotePort, (ushort)0);
        atomic_store(&_connectionStarted, false);
        atomic_store(&_remoteAddress, 0u);
        _radioModuleConnector->setLED(false, false, false);
        break;

    case 2: // keep alive
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
    // SAFETY: Load both atomics before checking to get consistent snapshot
    // Note: There's still a small race window, but it's acceptable for this use case
    uint32_t address = _remoteAddress.load(std::memory_order_relaxed);
    uint16_t port = _remotePort.load(std::memory_order_relaxed);

    if (port)
    {
        ip4_addr_t res{ .addr = address };
        return res;
    }
    else
    {
        return IP4_ADDR_ANY->u_addr.ip4;
    }
}

void RawUartUdpListener::sendMessage(unsigned char command, unsigned char *buffer, size_t len)
{
    // SAFETY: Load atomics in a consistent order
    uint32_t address = _remoteAddress.load(std::memory_order_relaxed);
    uint16_t port = _remotePort.load(std::memory_order_relaxed);

    if (!port)
        return;

    pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, len + 4, PBUF_RAM);
    if (pb == NULL) {
        ESP_LOGE(TAG, "Failed to allocate pbuf for sendMessage");
        return;
    }

    unsigned char *sendBuffer = (unsigned char *)pb->payload;

    ip_addr_t addr;
    addr.type = IPADDR_TYPE_V4;
    addr.u_addr.ip4.addr = address;

    sendBuffer[0] = command;
    sendBuffer[1] = (unsigned char)_counter.fetch_add(1, std::memory_order_relaxed);

    if (len)
        memcpy(sendBuffer + 2, buffer, len);

    *((uint16_t *)(sendBuffer + len + 2)) = htons(HMFrame::crc(sendBuffer, len + 2));

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
    // Optimization: Store struct directly in queue to avoid malloc/free per packet
    // Use larger queue for DTLS to handle potential processing delays
    int queueSize = atomic_load(&_dtlsEnabled) ? 64 : 32;

    _udp_queue = xQueueCreate(queueSize, sizeof(udp_event_t));
    if (_udp_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create UDP queue");
        return;
    }

    // CRITICAL: Highest priority (19) for CCU communication - just below RadioModuleConnector (20)
    // CCU messages must be processed with minimal latency for real-time signal relay
    // Increased stack size to 6KB for stability during peak load
    xTaskCreate(_raw_uart_udpQueueHandlerTask, "RawUartUdpListener_UDP_QueueHandler", 6144, this, 19, &_tHandle);

    _pcb = udp_new();
    if (_pcb == NULL) {
        ESP_LOGE(TAG, "Failed to create UDP queue");
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
        return;
    }
    udp_recv(_pcb, &_raw_uart_udpReceivePaket, (void *)this);

    _udp_bind(_pcb, IP4_ADDR_ANY, 3008);

    _radioModuleConnector->addFrameHandler(this);
    _radioModuleConnector->setDecodeEscaped(false);
}

void RawUartUdpListener::stop()
{
    _udp_disconnect(_pcb);
    udp_recv(_pcb, NULL, NULL);
    _udp_remove(_pcb);
    _pcb = NULL;

    _radioModuleConnector->removeFrameHandler(this);
    vTaskDelete(_tHandle);

    // Clean up queue
    if (_udp_queue) {
        // Drain any pending events to prevent memory leaks (pbufs need freeing)
        udp_event_t event;
        while (xQueueReceive(_udp_queue, &event, 0) == pdTRUE) {
            if (event.pb) {
                pbuf_free(event.pb);
            }
        }
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
    }
}

void RawUartUdpListener::_udpQueueHandler()
{
    udp_event_t event;
    int64_t nextKeepAliveSentOut = esp_timer_get_time();

    for (;;)
    {
        // 10ms timeout balances responsiveness with allowing IDLE task to run.
        // UDP packets arriving in the queue trigger immediate wakeup regardless.
        if (xQueueReceive(_udp_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            handlePacket(event.pb, event.addr, event.port);
            pbuf_free(event.pb);
            // No yield here - process packets as fast as possible
            continue;
        }

        // Only check timeouts when no packets are pending
        if (atomic_load(&_remotePort) != 0)
        {
            int64_t now = esp_timer_get_time();

            // OPTIMIZED: 2.5s timeout for faster disconnect detection
            if (now > _lastReceivedKeepAlive + 2500000)
            {
                // FIX: Clear port first (checked by sendMessage as connection flag)
                atomic_store(&_remotePort, (ushort)0);
                atomic_store(&_connectionStarted, false);
                atomic_store(&_remoteAddress, 0u);
                _radioModuleConnector->setLED(true, false, false);
                ESP_LOGE(TAG, "Connection timed out");
            }

            if (now > nextKeepAliveSentOut)
            {
                nextKeepAliveSentOut = now + 1000000; // 1sec
                sendMessage(2, NULL, 0);
            }
        }

        // Yield when queue is empty to let other tasks run
        taskYIELD();
    }

    vTaskDelete(NULL);
}

bool RawUartUdpListener::_udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    udp_event_t e;

    e.pb = pb;
    e.addr.addr = ip_addr_get_ip4_u32(addr);
    e.port = port;

    // Do not block if queue is full to prevent stalling the lwIP thread (DoS risk)
    if (xQueueSend(_udp_queue, &e, 0) != pdPASS)
    {
        ESP_LOGW(TAG, "UDP queue full, dropping Raw UART packet");
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
