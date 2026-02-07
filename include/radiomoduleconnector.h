/*
 *  radiomoduleconnector.h is part of the HB-RF-ETH firmware v2.0
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
    QueueHandle_t _uart_queue;
    TaskHandle_t _tHandle = NULL;

    void _handleFrame(unsigned char *buffer, uint16_t len);

public:
    RadioModuleConnector(LED *redLED, LED *greenLed, LED *blueLed);
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
