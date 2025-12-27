/*
 *  radiomoduleconnector.cpp is part of the HB-RF-ETH firmware v2.0
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "radiomoduleconnector.h"
#include "hmframe.h"
#include "driver/gpio.h"
#include "pins.h"
#include "esp_log.h"

void serialQueueHandlerTask(void *parameter)
{
    ((RadioModuleConnector *)parameter)->_serialQueueHandler();
}

RadioModuleConnector::RadioModuleConnector(LED *redLED, LED *greenLed, LED *blueLed) : _redLED(redLED), _greenLED(greenLed), _blueLED(blueLed)
{
    _handlersMutex = xSemaphoreCreateMutex();

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
        .flags = {},
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, HM_TX_PIN, HM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    using namespace std::placeholders;
    _streamParser = new StreamParser(false, std::bind(&RadioModuleConnector::_handleFrame, this, _1, _2));
}

RadioModuleConnector::~RadioModuleConnector()
{
    if (_streamParser) {
        delete _streamParser;
        _streamParser = nullptr;
    }
    if (_handlersMutex) {
        vSemaphoreDelete(_handlersMutex);
        _handlersMutex = nullptr;
    }
}

void RadioModuleConnector::start()
{
    setLED(false, false, false);

    // Increased buffer sizes to prevent overflow under heavy load
    // RX buffer: 4KB (was ~256 bytes) to handle burst traffic
    // Queue: 40 events (was 20) for better buffering
    uart_driver_install(UART_NUM_1, 4096, 0, 40, &_uart_queue, 0);

    // CRITICAL: Highest priority (20) for radio signal reception
    // Radio signals must be processed immediately to prevent loss
    xTaskCreate(serialQueueHandlerTask, "RadioModuleConnector_UART_QueueHandler", 6144, this, 20, &_tHandle);
    resetModule();
}

void RadioModuleConnector::stop()
{
    resetModule();
    uart_driver_delete(UART_NUM_1);
    vTaskDelete(_tHandle);
}

void RadioModuleConnector::addFrameHandler(FrameHandler *handler)
{
    if (xSemaphoreTake(_handlersMutex, portMAX_DELAY)) {
        if (std::find(_frameHandlers.begin(), _frameHandlers.end(), handler) == _frameHandlers.end()) {
            _frameHandlers.push_back(handler);
        }
        xSemaphoreGive(_handlersMutex);
    }
}

void RadioModuleConnector::removeFrameHandler(FrameHandler *handler)
{
    if (xSemaphoreTake(_handlersMutex, portMAX_DELAY)) {
        auto it = std::find(_frameHandlers.begin(), _frameHandlers.end(), handler);
        if (it != _frameHandlers.end()) {
            _frameHandlers.erase(it);
        }
        xSemaphoreGive(_handlersMutex);
    }
}

void RadioModuleConnector::setDecodeEscaped(bool decodeEscaped)
{
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
    gpio_set_level(HM_RST_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(HM_RST_PIN, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void RadioModuleConnector::sendFrame(unsigned char *buffer, uint16_t len)
{
    uart_write_bytes(UART_NUM_1, (const char *)buffer, len);
}

void RadioModuleConnector::_serialQueueHandler()
{
    uart_event_t event;
    // Increased buffer to match new UART buffer size
    uint8_t *buffer = (uint8_t *)malloc(4096);

    if (!buffer) {
        ESP_LOGE("RadioModuleConnector", "Failed to allocate UART buffer");
        vTaskDelete(NULL);
        return;
    }

    uart_flush_input(UART_NUM_1);

    for (;;)
    {
        if (xQueueReceive(_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                if (event.size <= 4096) {
                    uart_read_bytes(UART_NUM_1, buffer, event.size, portMAX_DELAY);
                    _streamParser->append(buffer, event.size);
                } else {
                    ESP_LOGE("RadioModuleConnector", "UART data size %d exceeds buffer", event.size);
                    uart_flush_input(UART_NUM_1);
                    _streamParser->flush();
                }
                break;
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
                ESP_LOGW("RadioModuleConnector", "UART buffer overflow detected");
                uart_flush_input(UART_NUM_1);
                xQueueReset(_uart_queue);
                _streamParser->flush();
                break;
            case UART_BREAK:
            case UART_PARITY_ERR:
            case UART_FRAME_ERR:
                // UART errors (break, parity, frame) can occur during normal operation
                // Rate-limit logging to avoid log spam - only log every 10th error
                {
                    static uint32_t uart_error_count = 0;
                    uart_error_count++;
                    if (uart_error_count % 10 == 1) {
                        ESP_LOGD("RadioModuleConnector", "UART error detected (count: %lu, type: %d)",
                                uart_error_count, event.type);
                    }
                }
                _streamParser->flush();
                break;
            default:
                break;
            }

            // Yield only on errors to maintain high-priority processing
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                taskYIELD();
            }
        }
    }

    free(buffer);
    buffer = NULL;
    vTaskDelete(NULL);
}

void RadioModuleConnector::_handleFrame(unsigned char *buffer, uint16_t len)
{
    if (xSemaphoreTake(_handlersMutex, portMAX_DELAY)) {
        std::vector<FrameHandler *> handlers = _frameHandlers;
        xSemaphoreGive(_handlersMutex);

        for (auto handler : handlers) {
            handler->handleFrame(buffer, len);
        }
    }
}
