/*
 *  radiomoduleconnector.cpp is part of the HB-RF-ETH firmware v2.0
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "radiomoduleconnector.h"
#include "hmframe.h"
#include "driver/gpio.h"
#include "pins.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <new>
#include "metrics.h"

// --- Radio-module UART instrumentation (#447) ------------------------------
//
// Every counter the CCU relay had until now sat on the UDP side of the bridge
// (hbrfeth_udp_*). That half has since reported zero drops, zero queue wait
// and zero occupancy on every field unit in #447 - which does not mean the
// relay is healthy, it means the instrumented half was never the suspect.
// The UART side, i.e. the actual path to the radio module, was completely
// unmeasured, and it contains the one code path that silently destroys
// traffic: an RX overflow flushes the driver buffer, so frames the module
// already delivered are discarded without a trace. Lost module->CCU frames
// make the CCU repeat the transmission, which is exactly how a duty cycle
// climbs and a device ends up reported as "gestört".
//
// All of these are a single lock-free add or compare-exchange in task
// context; none of them run in the ISR.

static MetricsCounter g_uart_rx_bytes("hbrfeth_uart_rx_bytes_total",
                                      "Bytes read from the radio module UART");
static MetricsCounter g_uart_tx_bytes("hbrfeth_uart_tx_bytes_total",
                                      "Bytes written to the radio module UART");
static MetricsCounter g_uart_rx_frames("hbrfeth_uart_rx_frames_total",
                                       "Complete frames decoded from the radio module");
static MetricsCounter g_uart_tx_frames("hbrfeth_uart_tx_frames_total",
                                       "Frames handed to the radio module UART");

// The failure counters. These are the numbers #447 actually needs: a nonzero
// value here means radio traffic was destroyed inside this firmware, which no
// UDP-side counter can show.
static MetricsCounter g_uart_fifo_ovf("hbrfeth_uart_fifo_ovf_total",
                                      "Hardware RX-FIFO overflows reported by the UART driver");
static MetricsCounter g_uart_buffer_full("hbrfeth_uart_buffer_full_total",
                                         "Driver RX ring buffer overruns");
static MetricsCounter g_uart_oversize("hbrfeth_uart_oversize_total",
                                      "UART data events larger than the read scratch buffer");
static MetricsCounter g_uart_break("hbrfeth_uart_break_total",
                                   "UART break conditions on the radio module link");
static MetricsCounter g_uart_parity_err("hbrfeth_uart_parity_err_total",
                                        "UART parity errors on the radio module link");
static MetricsCounter g_uart_frame_err("hbrfeth_uart_frame_err_total",
                                       "UART framing errors on the radio module link");
static MetricsCounter g_uart_read_timeout("hbrfeth_uart_read_timeout_total",
                                          "Bounded UART reads that returned no data");
static MetricsCounter g_uart_tx_errors("hbrfeth_uart_tx_errors_total",
                                       "Failed or short writes towards the radio module");

// Bytes thrown away by the flush that follows every overflow. The event count
// alone understates the damage: one overflow can discard a whole frame burst.
static MetricsCounter g_uart_flushed_bytes("hbrfeth_uart_flushed_bytes_total",
                                           "Received bytes discarded by an overflow flush");

// How close the driver RX ring gets to its 2 KiB capacity. A high-water well
// below capacity rules the RX path out; one near capacity says the handler
// task is not being scheduled fast enough, which is the same latency story
// the UDP queue gauges tell for the other half of the bridge.
static MetricsHighWater g_uart_rx_backlog_max("hbrfeth_uart_rx_backlog_max",
                                              "Highest observed driver RX ring occupancy in bytes");

// Ring-buffer capacities. File scope so the diagnostics snapshot can report
// the backlog high-water against the capacity it is approaching - a bare byte
// count is unjudgeable. The rationale for each value is at its use site in
// start().
static const int HM_UART_TX_RING_BUF_SIZE = 2048;
static const int HM_UART_RX_RING_BUF_SIZE = 2048;
static const int HM_UART_RX_FULL_THRESHOLD = 64;

void radio_uart_get_stats(radio_uart_stats_t *out)
{
    if (!out) return;
    out->rx_bytes       = g_uart_rx_bytes.get();
    out->tx_bytes       = g_uart_tx_bytes.get();
    out->rx_frames      = g_uart_rx_frames.get();
    out->tx_frames      = g_uart_tx_frames.get();
    out->fifo_ovf       = g_uart_fifo_ovf.get();
    out->buffer_full    = g_uart_buffer_full.get();
    out->oversize       = g_uart_oversize.get();
    out->breaks         = g_uart_break.get();
    out->parity_err     = g_uart_parity_err.get();
    out->frame_err      = g_uart_frame_err.get();
    out->read_timeouts  = g_uart_read_timeout.get();
    out->tx_errors      = g_uart_tx_errors.get();
    out->flushed_bytes  = g_uart_flushed_bytes.get();
    out->rx_backlog_max = g_uart_rx_backlog_max.get();
    out->rx_ring_size   = (uint32_t)HM_UART_RX_RING_BUF_SIZE;
    out->tx_ring_size   = (uint32_t)HM_UART_TX_RING_BUF_SIZE;
    out->rx_full_thresh = (uint32_t)HM_UART_RX_FULL_THRESHOLD;
}

void radio_uart_reset_high_water(void)
{
    g_uart_rx_backlog_max.reset();
}

void serialQueueHandlerTask(void *parameter)
{
    ((RadioModuleConnector *)parameter)->_serialQueueHandler();
}

RadioModuleConnector::~RadioModuleConnector()
{
    delete _streamParser;
    _streamParser = nullptr;
}

RadioModuleConnector::RadioModuleConnector(LED *redLED, LED *greenLed, LED *blueLed) : _redLED(redLED), _greenLED(greenLed), _blueLED(blueLed)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << HM_RST_PIN;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
            .allow_pd = 0,
            .backup_before_sleep = 0,
        },
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, HM_TX_PIN, HM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    using namespace std::placeholders;
    _streamParser = new (std::nothrow) StreamParser(false, std::bind(&RadioModuleConnector::_handleFrame, this, _1, _2));
    if (_streamParser == NULL)
    {
        ESP_LOGE("RadioModuleConnector", "Failed to allocate frame parser");
    }
}

void RadioModuleConnector::start()
{
    if (_tHandle) return;
    if (_streamParser == NULL) {
        ESP_LOGE("RadioModuleConnector", "Cannot start without frame parser");
        return;
    }
    setLED(false, false, false);

    // A TX ring buffer keeps uart_write_bytes() from busy-waiting on the
    // TX-FIFO-empty handshake at 115200 baud (the zero-size-TX-buffer mode
    // blocks the caller for the full wire time of every frame). The raw-uart
    // relay worker is one of the hottest tasks during a CCU reconnect burst,
    // and draining that burst serially into the module is exactly when the
    // relay must not stall. 2 KiB absorbs a worst-case HmIP frame burst.
    // (capacity constant at file scope: HM_UART_TX_RING_BUF_SIZE)

    // RX ring buffer 2 KiB (was 2 x FIFO = 256 bytes since the original
    // firmware). 256 bytes is ~22 ms of wire time at 115200 baud, so every
    // time this task sat in a blocking network send for longer than that
    // (tcpip_api_call on every module->CCU frame) the driver hit
    // UART_BUFFER_FULL, disabled the RX interrupt, and the hardware FIFO then
    // overflowed. The overflow path is the one place in the UART driver that
    // spins in an ISR critical section on the RX-FIFO counter the ESP32
    // errata (UART-3.17) declares unreliable - the prime suspect for the
    // interrupt-watchdog resets of issue #362, which only affect busy
    // installations (lots of RF traffic) and never the idle bench unit.
    // 2 KiB covers ~180 ms of continuous inbound traffic; the heap cost is
    // 1.75 KiB. Keeping the overflow path out of reach is cheaper than
    // surviving it.
    // (capacity constant at file scope: HM_UART_RX_RING_BUF_SIZE)

    // RX-FIFO "full" threshold 64 of 128 bytes (driver default: 120). The
    // default leaves the ISR only 8 bytes = 0.7 ms at 115200 baud between
    // "interrupt raised" and "FIFO overflows"; any critical section, flash
    // window or higher-priority ISR on that core longer than that overflows
    // the FIFO. 64 gives 5.5 ms of latency margin at the cost of one
    // interrupt per 64 instead of 120 bytes - irrelevant at this data rate.
    // (capacity constant at file scope: HM_UART_RX_FULL_THRESHOLD)

    esp_err_t err = uart_driver_install(UART_NUM_1, HM_UART_RX_RING_BUF_SIZE,
                                        HM_UART_TX_RING_BUF_SIZE, 20, &_uart_queue, 0);
    if (err != ESP_OK) {
        ESP_LOGE("RadioModuleConnector", "Failed to install UART driver: %s",
                 esp_err_to_name(err));
        _uart_queue = NULL;
        return;
    }

    err = uart_set_rx_full_threshold(UART_NUM_1, HM_UART_RX_FULL_THRESHOLD);
    if (err != ESP_OK) {
        // Not fatal: the driver default still works, just with less margin.
        ESP_LOGW("RadioModuleConnector", "Could not set RX-FIFO threshold: %s",
                 esp_err_to_name(err));
    }

    if (xTaskCreate(serialQueueHandlerTask, "RadioModuleConnector_UART_QueueHandler",
                    4096, this, 15, &_tHandle) != pdPASS) {
        ESP_LOGE("RadioModuleConnector", "Failed to create UART handler task");
        uart_driver_delete(UART_NUM_1);
        _uart_queue = NULL;
        return;
    }
    resetModule();
}

// LATENT HAZARD (issue #362 audits): deleting the UART task while it may be
// blocked inside tcpip_api_call() leaves a stack-resident call descriptor
// that the tcpip thread later signals - memory corruption. There are no
// runtime callers today; if this ever becomes reachable, it must use a
// cooperative rendezvous like RawUartUdpListener::stop() instead.
void RadioModuleConnector::stop()
{
    if (_tHandle) {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }
    uart_driver_delete(UART_NUM_1);
    _uart_queue = NULL;
    resetModule();
}

void RadioModuleConnector::setFrameHandler(FrameHandler *frameHandler, bool decodeEscaped)
{
    atomic_store(&_frameHandler, frameHandler);
    _streamParser->setDecodeEscaped(decodeEscaped);
}

void RadioModuleConnector::setLED(bool red, bool green, bool blue)
{
    _redLED->setState(red ? LED_STATE_ON : LED_STATE_OFF);
    _greenLED->setState(green ? LED_STATE_ON : LED_STATE_OFF);
    _blueLED->setState(blue ? LED_STATE_ON : LED_STATE_OFF);
}

void RadioModuleConnector::resetModule()
{
    // With the TX ring buffer, queued bytes may not have hit the wire yet;
    // the module reset would cut them off mid-frame. Drain first (bounded —
    // the full 2 KiB ring needs ~180 ms at 115200). Harmless no-op when the
    // driver is not installed (returns an error, checked by the driver).
    uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(250));

    gpio_set_level(HM_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(HM_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void RadioModuleConnector::sendFrame(unsigned char *buffer, uint16_t len)
{
    // The return value was discarded until now. A short or failed write means
    // a CCU command never reached the module, which the CCU then repeats - the
    // same duty-cycle mechanism as a dropped receive, and equally invisible on
    // the UDP-side counters (#447).
    const int written = uart_write_bytes(UART_NUM_1, (const char *)buffer, len);
    if (written < 0 || (uint16_t)written != len) {
        g_uart_tx_errors.inc();
        ESP_LOGW("RadioModuleConnector", "Short UART write: %d of %u bytes",
                 written, (unsigned)len);
        if (written > 0) g_uart_tx_bytes.inc((uint32_t)written);
    } else {
        g_uart_tx_bytes.inc((uint32_t)written);
    }
    g_uart_tx_frames.inc();
}

// Bytes the driver is holding right now, folded into the "discarded by a
// flush" total. Called immediately before uart_flush_input() so the number
// reflects what that flush is about to throw away. A failed query contributes
// nothing rather than guessing.
static uint32_t countFlushedBytes()
{
    size_t buffered = 0;
    if (uart_get_buffered_data_len(UART_NUM_1, &buffered) != ESP_OK || buffered == 0)
        return 0;
    g_uart_flushed_bytes.inc((uint32_t)buffered);
    return (uint32_t)buffered;
}

void RadioModuleConnector::_serialQueueHandler()
{
    uart_event_t event;
    /* One UART_DATA event carries at most one ISR read, i.e. at most the
     * hardware FIFO (128 bytes); the driver RX ring buffer is larger than
     * this scratch buffer only so that many such events can queue up while
     * this task is blocked. Twice the FIFO keeps a safety margin, and the
     * size check below turns any larger event into a flush instead of an
     * overflow of this stack buffer. */
    const size_t bufSize = UART_HW_FIFO_LEN(UART_NUM_1) * 2;
    uint8_t buffer[UART_HW_FIFO_LEN(UART_NUM_1) * 2];

    uart_flush_input(UART_NUM_1);

    for (;;)
    {
        if (xQueueReceive(_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                // Record how much the driver is holding before we drain it.
                // Sampling here rather than in the ISR keeps this out of the
                // interrupt path; what we miss between two events is bounded
                // by one FIFO.
                {
                    size_t buffered = 0;
                    if (uart_get_buffered_data_len(UART_NUM_1, &buffered) == ESP_OK)
                        g_uart_rx_backlog_max.record((uint32_t)buffered);
                }
                if (event.size > bufSize) {
                    g_uart_oversize.inc();
                    countFlushedBytes();
                    ESP_LOGE("RadioModuleConnector", "UART event exceeds RX buffer: %u",
                             (unsigned)event.size);
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(_uart_queue);
                    _streamParser->flush();
                    break;
                }
                {
                    // Bounded wait: a portMAX_DELAY here can never be
                    // aborted, and a driver flush racing this read would
                    // park the relay task forever on bytes that never come.
                    // 100 ms is orders above the 115200 drain time of the
                    // largest event; a zero return simply waits for the
                    // next UART_DATA event.
                    int read = uart_read_bytes(UART_NUM_1, buffer, event.size, pdMS_TO_TICKS(100));
                    if (read > 0) {
                        g_uart_rx_bytes.inc((uint32_t)read);
                        _streamParser->append(buffer, (uint16_t)read);
                    } else {
                        g_uart_read_timeout.inc();
                    }
                }
                break;
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
                // This is the path that destroys radio traffic without leaving
                // a trace on the UDP-side counters: whatever the module already
                // delivered is flushed, the CCU never sees it and repeats the
                // transmission. Count the events and the bytes lost, and say so
                // in the log - a silent flush here is indistinguishable from a
                // healthy link on every diagnostic we ship (#447).
                if (event.type == UART_FIFO_OVF) g_uart_fifo_ovf.inc();
                else                             g_uart_buffer_full.inc();
                {
                    const uint32_t lost = countFlushedBytes();
                    // Rate-limited: this runs on the UART task itself, and a
                    // blocking console write per overflow would lengthen the
                    // very stall that caused it. The counters carry the rest.
                    static uint32_t s_last_ovf_log_ms = 0;
                    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
                    if (now_ms - s_last_ovf_log_ms >= 1000) {
                        s_last_ovf_log_ms = now_ms;
                        ESP_LOGW("RadioModuleConnector",
                                 "UART RX overflow (%s), discarding %u buffered bytes",
                                 event.type == UART_FIFO_OVF ? "FIFO" : "ring buffer",
                                 (unsigned)lost);
                    }
                }
                uart_flush_input(UART_NUM_1);
                xQueueReset(_uart_queue);
                _streamParser->flush();
                break;
            case UART_BREAK:
            case UART_PARITY_ERR:
            case UART_FRAME_ERR:
                // A line-level error discards the partially assembled frame,
                // so it costs the CCU a repeat just like an overflow does.
                if (event.type == UART_BREAK)           g_uart_break.inc();
                else if (event.type == UART_PARITY_ERR) g_uart_parity_err.inc();
                else                                    g_uart_frame_err.inc();
                _streamParser->flush();
                break;
            default:
                break;
            }
        }
    }
}

void RadioModuleConnector::_handleFrame(unsigned char *buffer, uint16_t len)
{
    // Denominator for the failure counters: "3 overflows" is unreadable
    // without knowing whether 300 or 3 million frames came through.
    g_uart_rx_frames.inc();

    FrameHandler *frameHandler = (FrameHandler *)atomic_load(&_frameHandler);

    if (frameHandler)
    {
        frameHandler->handleFrame(buffer, len);
    }
}
