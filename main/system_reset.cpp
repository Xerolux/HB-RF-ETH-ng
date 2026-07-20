/*
 *  system_reset.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "system_reset.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"

static const char *TAG = "SystemReset";

// Restart sync is a fixed safety feature for every device. Legacy settings and
// old backups may still contain flashPause=false, but they can no longer disable
// the 35-second Ethernet link-down window.
static bool g_flashPauseEnabled = true;

// Optional callback registered by the Ethernet driver. When set, it cleanly
// stops the MAC (esp_eth_stop) BEFORE we toggle the PHY reset pin. This
// guarantees the link drops at both layers — GPIO-only PHY reset alone was
// unreliable on some board revisions and the CCU watchdog never saw the
// disconnect.
static restart_eth_pause_fn_t g_eth_pause_cb = NULL;

void set_flash_pause_enabled(bool enabled) {
    (void)enabled;
    g_flashPauseEnabled = true;
}

void register_restart_eth_pause_callback(restart_eth_pause_fn_t cb) {
    g_eth_pause_cb = cb;
}

void full_system_restart() {
    ESP_LOGI(TAG, "Initiating full system restart...");

    // Cleanly stop the Ethernet MAC first so the link partner (switch / CCU)
    // immediately sees carrier loss. The PHY pin toggle below keeps the PHY
    // in reset during the pause window; together both layers are down.
    if (g_eth_pause_cb) {
        ESP_LOGI(TAG, "Stopping Ethernet MAC for link-down pause...");
        g_eth_pause_cb();
    }

    // Configure pins as output
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << ETH_POWER_PIN) | (1ULL << HM_RST_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Resetting peripherals (ETH: GPIO %d, Radio: GPIO %d)...", ETH_POWER_PIN, HM_RST_PIN);

    // Assert Reset (Active Low)
    gpio_set_level(ETH_POWER_PIN, 0);
    gpio_set_level(HM_RST_PIN, 0);

    // Hold Ethernet PHY in reset for 35 s so the CCU watchdog (30 s timeout)
    // detects the link loss and triggers a clean CCU restart. The PHY nRST pin
    // is driven, not a power rail — PoE is unaffected. pdMS_TO_TICKS can
    // overflow for large values, so wait in 5-second chunks.
    ESP_LOGI(TAG, "Link-down pause active: Ethernet off for 35 s (CCU watchdog trigger)...");
    for (int i = 0; i < 7; i++) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    ESP_LOGI(TAG, "Releasing peripheral reset pins before ESP32 restart...");
    gpio_set_level(ETH_POWER_PIN, 1);
    gpio_set_level(HM_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "Peripherals reset complete. Restarting ESP32...");
    esp_restart();
}
