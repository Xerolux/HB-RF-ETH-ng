/*
 *  radiomoduleconnector.h is part of the HB-RF-ETH firmware v2.0
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "led.h"
#include "streamparser.h"
#include <atomic>
#define _Atomic(X) std::atomic<X>

class FrameHandler
{
public:
    virtual void handleFrame(unsigned char *buffer, uint16_t len) = 0;
};

class RadioModuleConnector
{
private:
    LED *_redLED;
    LED *_greenLED;
    LED *_blueLED;
    StreamParser *_streamParser;
    std::atomic<FrameHandler *> _frameHandler = ATOMIC_VAR_INIT(0);
    QueueHandle_t _uart_queue = NULL;
    TaskHandle_t _tHandle = NULL;

    void _handleFrame(unsigned char *buffer, uint16_t len);

public:
    RadioModuleConnector(LED *redLED, LED *greenLed, LED *blueLed);
    ~RadioModuleConnector();
    RadioModuleConnector(const RadioModuleConnector&) = delete;
    RadioModuleConnector& operator=(const RadioModuleConnector&) = delete;

    void start();
    void stop();

    void setLED(bool red, bool green, bool blue);

    void setFrameHandler(FrameHandler *handler, bool decodeEscaped);

    void resetModule();

    void sendFrame(unsigned char *buffer, uint16_t len);

    void _serialQueueHandler();
};

// Snapshot of the radio-module UART instrumentation (#447).
//
// The CCU relay had counters on its UDP half only, and that half reported
// zero drops and zero queue wait on every field unit in #447. That does not
// clear the relay, it clears the measured half: the UART path to the radio
// module was unmeasured, and it contains the one code path that silently
// destroys traffic - an RX overflow flushes the driver buffer, so frames the
// module already delivered vanish before the CCU ever sees them and the CCU
// repeats the transmission. That is how a duty cycle climbs.
//
// Exported through the Prometheus endpoint and the WebUI diagnostics page,
// alongside raw_uart_latency_t, because the people who can supply the
// measurement run neither a scraper nor a broker.
typedef struct {
    uint64_t rx_bytes;       // bytes read from the module
    uint64_t tx_bytes;       // bytes written to the module
    uint64_t rx_frames;      // complete frames decoded from the module
    uint64_t tx_frames;      // frames handed to the module
    uint64_t fifo_ovf;       // hardware RX-FIFO overflows
    uint64_t buffer_full;    // driver RX ring overruns
    uint64_t oversize;       // data events larger than the read scratch buffer
    uint64_t breaks;         // break conditions on the link
    uint64_t parity_err;     // parity errors
    uint64_t frame_err;      // framing errors
    uint64_t read_timeouts;  // bounded reads that returned no data
    uint64_t tx_errors;      // failed or short writes towards the module
    uint64_t flushed_bytes;  // received bytes discarded by an overflow flush
    uint32_t rx_backlog_max; // high-water driver RX ring occupancy, bytes
    uint32_t rx_ring_size;   // driver RX ring capacity, bytes
    uint32_t tx_ring_size;   // driver TX ring capacity, bytes
    uint32_t rx_full_thresh; // RX-FIFO "full" interrupt threshold, bytes
} radio_uart_stats_t;

void radio_uart_get_stats(radio_uart_stats_t *out);

// Clear the backlog high-water mark so an operator can watch a fresh window.
// The event counters are monotonic and deliberately not reset - they are the
// evidence trail the issue needs.
void radio_uart_reset_high_water(void);
