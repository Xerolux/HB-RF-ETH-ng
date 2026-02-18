/*
 *  led.h is part of the HB-RF-ETH firmware v2.0
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

#include <stdio.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "settings.h"

#define MAX_LED_COUNT 5

typedef enum
{
    LED_STATE_OFF = 0,
    LED_STATE_ON = 1,
    LED_STATE_BLINK = 2,
    LED_STATE_BLINK_INV = 3,
    LED_STATE_BLINK_FAST = 4,
    LED_STATE_BLINK_SLOW = 5,
    LED_STATE_BLINK_2X = 6,        // 2x blinken, dann Pause
    LED_STATE_BLINK_3X = 7,        // 3x blinken, dann Pause
    LED_STATE_BREATHING = 8,       // Sanftes pulsieren
    LED_STATE_HEARTBEAT = 9,       // Herzschlag-Muster
    LED_STATE_STROBE = 10,         // Strobe-Effekt
} led_state_t;

// LED Programme für verschiedene Systemzustände
typedef enum
{
    LED_PROG_IDLE = 0,             // Im Normalbetrieb
    LED_PROG_CCU_DISCONNECTED = 1, // CCU nicht verbunden
    LED_PROG_CCU_CONNECTED = 2,    // CCU verbunden
    LED_PROG_UPDATE_AVAILABLE = 3, // Update verfügbar
    LED_PROG_ERROR = 4,            // Fehlerzustand
    LED_PROG_BOOTING = 5,          // System bootet
    LED_PROG_UPDATE_IN_PROGRESS = 6 // Update wird eingespielt
} led_program_t;

class LED
{
private:
    uint8_t _state;
    ledc_channel_config_t _channel_conf;
    void _setPinState(bool enabled);

    // LED Programme Konfiguration
    static led_state_t _programs[7]; // Programme für verschiedene Zustände

public:
    static void start(Settings *settings);
    static void stop();

    // LED Programme Management
    static void setProgram(led_program_t program, led_state_t state);
    static led_state_t getProgram(led_program_t program);
    static void setProgramState(led_program_t program);

    LED(gpio_num_t pin);
    void setState(led_state_t state);
    void updatePinState();
};