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
#include <stdlib.h>
#include <string.h>
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

static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "Raw-UART sender lifetime guard must be native 32-bit");

void _raw_uart_udpQueueHandlerTask(void *parameter)
{
    ((RawUartUdpListener *)parameter)->_udpQueueHandler();
}

// Deliberately NOT IRAM_ATTR: this lwIP UDP recv callback runs in the tcpip
// thread (task context, not ISR), so there is no cache-off execution
// requirement. Keeping the wrapper in IRAM while the callee chain
// (_udpReceivePacket -> ESP_LOGW) stays in flash recreates the exact
// cache-disabled-access hazard that caused interrupt-watchdog hangs during
// flash/NVS windows on the other core (see .planning interrupt-watchdog
// analysis). lwIP's recv callbacks always run with cache enabled.
void _raw_uart_udpReceivePaket(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    // A pbuf chain is one UDP datagram, not multiple packets. Transfer the
    // complete chain to the worker; splitting it corrupts larger CCU frames.
    if (pb != NULL && !((RawUartUdpListener *)arg)->_udpReceivePacket(pb, addr, port))
    {
        pbuf_free(pb);
    }
}

RawUartUdpListener::RawUartUdpListener(RadioModuleConnector *radioModuleConnector)
    : _radioModuleConnector(radioModuleConnector),
      _lifecycleMutex(
          xSemaphoreCreateMutexStatic(&_lifecycleMutexStorage))
{
    atomic_init(&_connectionStarted, false);
    atomic_init(&_remotePort, (ushort)0);
    atomic_init(&_remoteAddress, 0u);
    atomic_init(&_counter, 0);
    atomic_init(&_endpointConnectionIdentifier, 1);
}

bool RawUartUdpListener::handlePacket(pbuf *pb, ip4_addr_t addr, uint16_t port)
{
    size_t length = pb->tot_len;
    if (length < 4 || length > 1500)
    {
        ESP_LOGE(TAG, "Received invalid raw-uart packet, length %zu", length);
        g_rx_drops.inc();
        return false;
    }

    struct HeapBufferGuard {
        unsigned char *value = NULL;
        ~HeapBufferGuard()
        {
            free(value);
        }
    };

    // Raw-UART control packets and normal radio frames fit into this buffer,
    // avoiding a heap allocation for every received UDP datagram. Oversized
    // (but still valid) frames retain the existing support through a checked
    // heap fallback.
    unsigned char small_data[256];
    HeapBufferGuard heap_data;
    unsigned char *data = small_data;
    if (length > sizeof(small_data))
    {
        heap_data.value = (unsigned char *)malloc(length);
        if (!heap_data.value)
        {
            ESP_LOGE(TAG, "Could not allocate raw-uart packet buffer, length %zu", length);
            g_rx_drops.inc();
            return false;
        }
        data = heap_data.value;
    }

    unsigned char response_buffer[3];

    if (pbuf_copy_partial(pb, data, length, 0) != length) {
        ESP_LOGE(TAG, "Could not linearize raw-uart packet, length %zu", length);
        g_rx_drops.inc();
        return false;
    }

    if (data[0] != 0 && (addr.addr != atomic_load(&_remoteAddress) || port != atomic_load(&_remotePort)))
    {
        ESP_LOGE(TAG, "Received raw-uart packet from invalid address.");
        g_rx_drops.inc();
        return false;
    }

    /* Read the trailing CRC16 with memcpy: the data pointer + length comes from
     * a network pbuf and is not guaranteed to be 2-byte aligned, and casting an
     * unsigned char* to uint16_t* also violates strict aliasing. */
    uint16_t received_crc;
    memcpy(&received_crc, data + length - 2, sizeof(uint16_t));
    if (received_crc != htons(HMFrame::crc(data, length - 2)))
    {
        ESP_LOGE(TAG, "Received raw-uart packet with invalid crc.");
        g_rx_drops.inc();
        return false;
    }

    // Valid frame received from the CCU.
    g_rx_frames.inc();

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
            return true;
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
            return true;
        }

        _radioModuleConnector->setLED(data[2] & 1, data[2] & 2, data[2] & 4);
        break;

    case 4: // Reset
        if (length != 4)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart reset packet, length %d", length);
            return true;
        }

        _radioModuleConnector->resetModule();
        break;

    case 5: // Start connection
        if (length != 4)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart startconn packet, length %d", length);
            return true;
        }

        atomic_store(&_connectionStarted, true);
        break;

    case 6: // End connection
        if (length != 4)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart endconn packet, length %d", length);
            return true;
        }

        atomic_store(&_connectionStarted, false);
        break;

    case 7: // Frame
        if (length < 5)
        {
            ESP_LOGE(TAG, "Received invalid raw-uart frame packet, length %d", length);
            return true;
        }

        _radioModuleConnector->sendFrame(&data[2], length - 4);
        break;

    default:
        ESP_LOGE(TAG, "Received invalid raw-uart packet with unknown type %d", data[0]);
        break;
    }
    return true;
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
    // Stop closes the send gate before the worker removes the PCB. Register as
    // an active sender and then re-check the gate so teardown can safely wait
    // for callbacks which entered immediately before that transition.
    if (_stopRequested.load(std::memory_order_acquire)) return;
    _activeSenders.fetch_add(1, std::memory_order_seq_cst);
    struct ActiveSenderGuard {
        std::atomic<uint32_t> *counter;
        ~ActiveSenderGuard()
        {
            counter->fetch_sub(1, std::memory_order_seq_cst);
        }
    } active_sender{&_activeSenders};

    if (_stopRequested.load(std::memory_order_seq_cst)) return;

    uint16_t port = atomic_load(&_remotePort);
    uint32_t address = atomic_load(&_remoteAddress);
    udp_pcb *pcb = _pcb.load(std::memory_order_acquire);

    if (!port || !pcb)
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

    _udp_sendto(pcb, pb, &addr, port);
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
    if (!_lifecycleMutex) {
        ESP_LOGE(TAG, "Cannot start UDP listener without lifecycle mutex");
        return;
    }
    xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);

    if (_tHandle.load(std::memory_order_acquire) ||
        _pcb.load(std::memory_order_acquire) ||
        _udp_queue.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "UDP listener already running or still stopping");
        xSemaphoreGive(_lifecycleMutex);
        return;
    }

    _stopRequested.store(false, std::memory_order_release);

    // Store the small event descriptor directly in the FreeRTOS queue. This
    // removes one malloc/free pair per UDP datagram from the LwIP callback.
    // 32 slots is plenty for a single CCU-3 session; 64 reserved ~1 KB of
    // queue storage for no observed benefit. Drop depth halves that reserve.
    QueueHandle_t queue = xQueueCreate(32, sizeof(udp_event_t));
    if (queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create UDP queue - out of memory");
        _stopRequested.store(true, std::memory_order_release);
        xSemaphoreGive(_lifecycleMutex);
        return;
    }
    _udp_queue.store(queue, std::memory_order_release);

    udp_pcb *pcb = _udp_new();
    _pcb.store(pcb, std::memory_order_release);
    if (!pcb || _udp_bind(pcb, IP4_ADDR_ANY, 3008) != ERR_OK) {
        ESP_LOGE(TAG, "Failed to create/bind UDP listener on port 3008");
        _udp_remove(pcb);
        _pcb.store(NULL, std::memory_order_release);
        vQueueDelete(queue);
        _udp_queue.store(NULL, std::memory_order_release);
        _stopRequested.store(true, std::memory_order_release);
        xSemaphoreGive(_lifecycleMutex);
        return;
    }
    _udp_recv(pcb, &_raw_uart_udpReceivePaket, (void *)this);

    // Priority 12 (was 15): the listener still runs well above user tasks
    // (events: 3, log_stream: 4, mqtt_publish: 4) so the CCU-3 session stays
    // latency-bound to the radio module, but yields a few levels of headroom
    // below the ESP-IDF system / Wi-Fi / timer tasks. Under a burst of UDP
    // frames this gives high-priority system tasks more room to interleave and
    // avoids unnecessary pressure around the per-CPU tick ISR that feeds the
    // interrupt watchdog.
    TaskHandle_t task = NULL;
    if (xTaskCreate(_raw_uart_udpQueueHandlerTask, "RawUartUdpListener_UDP_QueueHandler",
                    4096, this, 12, &task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP listener task");
        // Reception was enabled before task creation so the tcpip thread may
        // already have transferred pbuf ownership into the queue. Close
        // admission first, synchronously unregister the callback, then return
        // every queued pbuf before deleting the queue. This OOM path must not
        // leak the very packet buffers needed for a later recovery attempt.
        _stopRequested.store(true, std::memory_order_release);
        _udp_recv(pcb, NULL, NULL);
        _udp_remove(pcb);
        _pcb.store(NULL, std::memory_order_release);
        udp_event_t pending = {};
        while (xQueueReceive(queue, &pending, 0) == pdTRUE) {
            if (pending.pb) pbuf_free(pending.pb);
        }
        vQueueDelete(queue);
        _udp_queue.store(NULL, std::memory_order_release);
        xSemaphoreGive(_lifecycleMutex);
        return;
    }
    _tHandle.store(task, std::memory_order_release);

    _radioModuleConnector->setFrameHandler(this, false);
    xSemaphoreGive(_lifecycleMutex);

    ESP_LOGI(TAG, "UDP listener started on port 3008");
}

esp_err_t RawUartUdpListener::stop()
{
    static constexpr uint32_t RAW_UART_STOP_TIMEOUT_MS = 2000;

    if (!_lifecycleMutex) return ESP_ERR_NO_MEM;
    xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);

    // Clear the handler inside the same lifecycle transition used by start().
    // Otherwise a concurrent start could register `this` after an early clear
    // and leave the stopped listener installed. A callback which already
    // loaded the old handler is covered by _activeSenders.
    _radioModuleConnector->setFrameHandler(NULL, false);
    // Sequential consistency closes the two-atomic admission race with
    // sendMessage(): either teardown observes the registered sender, or that
    // sender observes this closed gate before it can load/use the PCB.
    _stopRequested.store(true, std::memory_order_seq_cst);

    TaskHandle_t task = _tHandle.load(std::memory_order_acquire);
    if (!task) {
        xSemaphoreGive(_lifecycleMutex);
        return ESP_OK;
    }

    if (task == xTaskGetCurrentTaskHandle()) {
        // The worker will observe the closed gate on return and clean itself
        // up. Waiting for our own handle here would deadlock.
        xSemaphoreGive(_lifecycleMutex);
        return ESP_ERR_INVALID_STATE;
    }

    // Do not use the generic FreeRTOS delay-abort API here. The same worker can be blocked inside
    // tcpip_api_call() while sending a keep-alive; aborting that semaphore wait
    // would let the stack-backed lwIP call descriptor disappear while the
    // tcpip thread still references it. The queue receive below is deliberately
    // bounded to 10 ms, so setting the stop flag is already a targeted and
    // sufficiently prompt wake-up mechanism.
    xSemaphoreGive(_lifecycleMutex);

    const TickType_t started = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(RAW_UART_STOP_TIMEOUT_MS);
    while (_tHandle.load(std::memory_order_acquire) != NULL &&
           (TickType_t)(xTaskGetTickCount() - started) < timeout) {
        vTaskDelay(1);
    }

    if (_tHandle.load(std::memory_order_acquire) != NULL) {
        ESP_LOGE(TAG, "UDP listener did not stop within %u ms",
                 (unsigned)RAW_UART_STOP_TIMEOUT_MS);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void RawUartUdpListener::_udpQueueHandler()
{
    udp_event_t event = {};
    // All keep-alive timekeeping is confined to this task. Tick subtraction
    // is unsigned and therefore remains correct across TickType_t rollover.
    const TickType_t keep_alive_interval = pdMS_TO_TICKS(1000);
    const TickType_t connection_timeout = pdMS_TO_TICKS(10000);
    TickType_t last_received_keep_alive = xTaskGetTickCount();
    TickType_t last_keep_alive_sent = last_received_keep_alive;

    for (;;)
    {
        if (_stopRequested.load(std::memory_order_acquire)) break;

        QueueHandle_t queue = _udp_queue.load(std::memory_order_acquire);
        if (!queue) break;

        if (xQueueReceive(queue, &event, (TickType_t)pdMS_TO_TICKS(10)) == pdTRUE)
        {
            if (event.pb) {
                const TickType_t received_at = xTaskGetTickCount();
                if (!_stopRequested.load(std::memory_order_acquire) &&
                    handlePacket(event.pb, event.addr, event.port)) {
                    last_received_keep_alive = received_at;
                }
                pbuf_free(event.pb);
            }
        }

        if (_stopRequested.load(std::memory_order_acquire)) break;

        if (atomic_load(&_remotePort) != 0)
        {
            const TickType_t now = xTaskGetTickCount();

            if ((TickType_t)(now - last_received_keep_alive) >=
                connection_timeout)
            { // 10 sec
                ESP_LOGW(TAG, "CCU 3 connection timed out (no keep-alive for 10 seconds)");
                atomic_store(&_connectionStarted, false);
                atomic_store(&_remotePort, (ushort)0);
                atomic_store(&_remoteAddress, 0u);
                _radioModuleConnector->setLED(true, false, false);
            }
            else if ((TickType_t)(now - last_keep_alive_sent) >=
                     keep_alive_interval)
            {
                last_keep_alive_sent = now;
                g_keepalives.inc();
                sendMessage(2, NULL, 0);
            }
        }
    }

    // Stop LwIP delivery first. tcpip_api_call() serialises with the receive
    // callback, so when this returns no callback can still be using the queue.
    udp_pcb *pcb = _pcb.load(std::memory_order_acquire);
    if (pcb) _udp_recv(pcb, NULL, NULL);

    // A radio callback may already be inside the synchronous tcpip send call.
    // Keep this low-priority task cooperative until it leaves; stop() remains
    // bounded and reports ESP_ERR_TIMEOUT instead of force-deleting either
    // task and stranding shared counter state.
    while (_activeSenders.load(std::memory_order_seq_cst) != 0) {
        vTaskDelay(1);
    }

    pcb = _pcb.exchange(NULL, std::memory_order_acq_rel);
    if (pcb) {
        _udp_disconnect(pcb);
        _udp_remove(pcb);
    }

    QueueHandle_t queue = _udp_queue.exchange(NULL, std::memory_order_acq_rel);
    if (queue) {
        while (xQueueReceive(queue, &event, 0) == pdTRUE) {
            if (event.pb) pbuf_free(event.pb);
        }
        vQueueDelete(queue);
    }

    atomic_store(&_connectionStarted, false);
    atomic_store(&_remotePort, (ushort)0);
    atomic_store(&_remoteAddress, 0u);
    _radioModuleConnector->setLED(false, false, false);

    ESP_LOGI(TAG, "UDP listener stopped");
    xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
    _tHandle.store(NULL, std::memory_order_release);
    xSemaphoreGive(_lifecycleMutex);
    vTaskDelete(NULL);
}

bool RawUartUdpListener::_udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    if (!pb || !addr || _stopRequested.load(std::memory_order_acquire)) {
        return false;
    }

    QueueHandle_t queue = _udp_queue.load(std::memory_order_acquire);
    if (!queue) return false;

    udp_event_t event = {};
    event.pb = pb;

    // Use the source address and port provided directly by LwIP in the callback
    // instead of reading raw pbuf header memory (which is unsafe and fragile).
    event.addr.addr = addr->u_addr.ip4.addr;
    event.port = port;

    if (xQueueSend(queue, &event, 0) != pdPASS)
    {
        ESP_LOGW(TAG, "UDP queue full, dropping packet");
        g_rx_drops.inc();
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
