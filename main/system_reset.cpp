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
#include "monitoring.h"
#include "supporter_crl.h"
#include <atomic>

static const char *TAG = "SystemReset";

// Restart sync is a fixed safety feature for every device. Legacy settings and
// old backups may still contain flashPause=false, but they can no longer disable
// the 35-second Ethernet link-down window.
static bool g_flashPauseEnabled = true;
static std::atomic<bool> g_restart_in_progress{false};

// Optional callback registered by the Ethernet driver. When set, it cleanly
// stops the MAC (esp_eth_stop) BEFORE we toggle the PHY reset pin. This
// guarantees the link drops at both layers — GPIO-only PHY reset alone was
// unreliable on some board revisions and the CCU watchdog never saw the
// disconnect.
static restart_eth_pause_fn_t g_eth_pause_cb = NULL;
static restart_network_stop_fn_t g_network_stop_cb = NULL;

void set_flash_pause_enabled(bool enabled) {
    (void)enabled;
    g_flashPauseEnabled = true;
}

void register_restart_eth_pause_callback(restart_eth_pause_fn_t cb) {
    g_eth_pause_cb = cb;
}

void register_restart_network_stop_callback(restart_network_stop_fn_t cb) {
    g_network_stop_cb = cb;
}

static void full_system_restart_impl(bool operation_reserved) {
    bool wait_logged = false;
    for (;;) {
        bool expected = false;
        if (g_restart_in_progress.compare_exchange_weak(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            break;
        }
        if (!wait_logged) {
            ESP_LOGW(TAG, "Restart already in progress; waiting for ownership");
            wait_logged = true;
        }
        // A genuine restart never returns. If the incumbent instead rejects
        // its operation reservation and clears the flag, a reserved caller
        // must be able to take over rather than park forever while still
        // owning the global OTA/config-operation gate.
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Initiating full system restart...");

    // Reserve the same operation gate used by manual firmware uploads and
    // config updates.
    // A normal restart waits for a short in-flight config transaction so it
    // does not unnecessarily trigger the NVS marker's safe-default recovery.
    // If a firmware upload owns the gate, abort the competing restart instead
    // of interrupting active flash writes.
    if (operation_reserved) {
        if (!ota_operation_active()) {
            ESP_LOGE(TAG, "Reserved restart called without operation ownership");
            g_restart_in_progress.store(false, std::memory_order_release);
            return;
        }
    } else {
        if (!ota_operation_try_begin()) {
            if (monitoring_config_update_active()) {
                ESP_LOGW(TAG, "Config update active; waiting before restart");
                for (int attempt = 0;
                     attempt < 200 && monitoring_config_update_active();
                     ++attempt) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                if (monitoring_config_update_active() ||
                    !ota_operation_try_begin()) {
                    // The fail-safe NVS marker prevents a partial monitoring
                    // generation from becoming active on the next boot.
                    ESP_LOGE(TAG,
                             "Config update did not quiesce; restarting directly");
                    if (g_eth_pause_cb) {
                        g_eth_pause_cb();
                    }
                    esp_restart();
                    return;
                }
            } else {
                ESP_LOGE(TAG, "Restart rejected while another operation is active");
                g_restart_in_progress.store(false, std::memory_order_release);
                return;
            }
        }
    }

    // Every caller—not only a post-upload restart—must stop lwIP/TLS users before the
    // Ethernet MAC and PHY are held down for 35 seconds. Otherwise MQTT,
    // notifications, Syslog, CRL, CheckMK or Prometheus can touch sockets
    // after esp_eth_stop(), which was one path to watchdog/panic resets.
    //
    // Always call monitoring_pause_for_ota even when the flag appears set.
    // monitoring_update_config can restart workers via rollback while
    // g_monitoring_ota_paused is true, then return before the restart sees
    // the flag and skips this call — leaving workers alive when Ethernet
    // is torn down.
    esp_err_t prepare_result = ESP_OK;
    {
        uint32_t ignored_pause_mask = 0;
        prepare_result = monitoring_pause_for_ota(&ignored_pause_mask);
        if (prepare_result == ESP_ERR_INVALID_STATE) {
            // Flag was already set by another caller (config-update rollback,
            // concurrent restart). Workers are already stopped; proceed.
            prepare_result = ESP_OK;
        }
    }
    if (prepare_result == ESP_OK) {
        prepare_result = supporter_crl_stop_refresh_task();
    }
    if (prepare_result == ESP_OK && g_network_stop_cb != NULL) {
        // Raw-UART owns a UDP PCB/queue outside the monitoring subsystem.
        // Stop it before esp_eth_stop() so queued radio frames cannot call
        // udp_sendto() throughout the link-down window.
        prepare_result = g_network_stop_cb();
    }
    if (prepare_result != ESP_OK) {
        // A worker still owns network/library state. Stop Ethernet at the MAC
        // level before rebooting so the link partner sees carrier loss and we
        // avoid another corrupted shutdown through active sockets.
        ESP_LOGE(TAG, "Worker quiesce failed (%s); stopping Ethernet and rebooting",
                 esp_err_to_name(prepare_result));
        if (g_eth_pause_cb) {
            g_eth_pause_cb();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        return;
    }

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

void full_system_restart() {
    full_system_restart_impl(false);
}

void full_system_restart_with_reserved_operation() {
    full_system_restart_impl(true);
}

void emergency_network_stop_before_reboot()
{
    if (g_eth_pause_cb) {
        g_eth_pause_cb();
    }
}
