/*
 *  main.cpp is part of the HB-RF-ETH firmware v2.0
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
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"

#include "pins.h"
#include "led.h"
#include "settings.h"
#include "rtcdriver.h"
#include "systemclock.h"
#include "dcf.h"
#include "ntpclient.h"
#include "gps.h"
#include "ethernet.h"
#include "pushbuttonhandler.h"
#include "radiomoduleconnector.h"
#include "radiomoduledetector.h"
#include "rawuartudplistener.h"
#include "webui.h"
#include "ntpserver.h"
#include "esp_ota_ops.h"
#include "updatecheck.h"
#include "monitoring.h"
#include "log_manager.h"
#include "supporter_crl.h"
#include "metrics.h"
#include "events.h"
#include "reset_info.h"
#include "system_reset.h"

static const char *TAG = "HB-RF-ETH";

extern "C"
{
    void app_main(void);
}

// Keep a freshly installed OTA image in PENDING_VERIFY just long enough to
// confirm the critical boot sequence (Ethernet, radio module, WebUI) survived.
// The previous 60-second window overlapped with the first UpdateCheck (30s),
// CRL refresh (60s) and deferred log-retry (10s) TLS fetches — on a device
// already starved of sockets/heap those fetches could destabilise the system
// inside the self-test window and trigger an unwanted bootloader rollback.
// 15 seconds is enough for all services to start and for the first httpd
// request to be served, while staying well clear of the TLS-heavy window.
static void validate_running_firmware_task(void *parameter)
{
    (void)parameter;
    vTaskDelay(pdMS_TO_TICKS(15000));

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA self-test window passed; firmware marked valid");
    } else {
        ESP_LOGE(TAG, "Could not mark firmware valid after self-test: %s",
                 esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}

static void schedule_firmware_validation(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (!running || esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }

    ESP_LOGI(TAG, "OTA image pending verification; starting 15-second self-test window");
    if (xTaskCreate(validate_running_firmware_task, "ota_selftest", 2304,
                    NULL, 2, NULL) != pdPASS) {
        // Could not create the task — mark valid immediately rather than
        // leaving the image pending forever (which would roll back on the
        // next reboot even though the firmware booted fine).
        ESP_LOGE(TAG, "Could not create OTA self-test task; marking valid now");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

void app_main()
{
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
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // All long-lived service objects are function-local statics instead of
    // stack locals: they must outlive app_main's active phase (the task is
    // suspended at the end, never returns) and keeping them in the frame
    // pushed the main task close to its stack limit, which caused a silent
    // boot hang via stack overflow into adjacent heap allocations.
    static Settings settings;

    // Install the log capture hook unconditionally at boot. The hook itself
    // is a no-op for the ring buffer when it is not allocated, but it allows
    // subscribers (syslog forwarder, WebSocket log stream) to receive lines
    // without each having to install their own hook.
    LogManager::init();
    supporter_crl_init();
    // Only spin up the CRL refresh task when a supporter key is actually
    // configured. Without a key there is nothing to revoke, but the task
    // still costs 8 KB of task stack and a 30-50 KB TLS heap spike every
    // refresh cycle — costs most key-less devices never benefit from.
    {
        const char *sk = settings.getSupporterKey();
        if (sk && sk[0] != '\0')
        {
            supporter_crl_start_refresh_task();
        }
    }

    // Initialise the metrics registry so counters can be registered by any
    // subsystem from this point on. The Prometheus exporter (Phase A) reads
    // them on demand.
    metrics_init();

    // Default: disabled to save 8 KB heap. If the user enabled system log
    // capture in the WebUI, persist that choice and resume capture early on
    // boot so startup diagnostics are included.
    if (settings.getSystemLogEnabled())
    {
        LogManager::begin();
        if (LogManager::instance().isEnabled())
        {
            ESP_LOGI(TAG, "System log capture restored from settings");
        }
        else
        {
            ESP_LOGE(TAG, "System log capture is enabled in settings but the buffer could not be allocated");
        }
    }

    set_flash_pause_enabled(settings.getFlashPause());

    static LED powerLED(LED_PWR_PIN);
    static LED statusLED(LED_STATUS_PIN);

    static LED redLED(HM_RED_PIN);
    static LED greenLED(HM_GREEN_PIN);
    static LED blueLED(HM_BLUE_PIN);

    LED::start(&settings);

    powerLED.setState(LED_STATE_BLINK);
    statusLED.setState(LED_STATE_BLINK_INV);

    redLED.setState(LED_STATE_OFF);
    greenLED.setState(LED_STATE_OFF);
    blueLED.setState(LED_STATE_OFF);

    static SysInfo sysInfo;

    static PushButtonHandler pushButton;
    pushButton.handleStartupFactoryReset(&powerLED, &statusLED, &settings);

    // Watch the rest of the boot sequence with the task WDT: a silent hang
    // (e.g. blocked forever on a semaphore inside a driver init) then
    // produces a visible watchdog report with backtrace instead of a dead
    // board. Subscribed only after the factory-reset button handling, which
    // legitimately blocks while the button is held.
    bool bootWdt = (esp_task_wdt_add(NULL) == ESP_OK);

    static RadioModuleConnector radioModuleConnector(&redLED, &greenLED, &blueLED);
    radioModuleConnector.start();

    static Ethernet ethernet(&settings);
    ethernet.start();

    if (bootWdt)
        esp_task_wdt_reset();

    setenv("TZ", "UTC0", 1);
    tzset();

    Rtc *rtc = NULL;
    rtc = Rtc::detect();

    static SystemClock clk(rtc);
    clk.start();

    static DCF dcf(&settings, &clk);
    static NtpClient ntpClient(&settings, &clk);
    static GPS gps(&settings, &clk);

    switch (settings.getTimesource())
    {
    case TIMESOURCE_NTP:
        ntpClient.start();
        break;
    case TIMESOURCE_GPS:
        gps.start();
        break;
    case TIMESOURCE_DCF:
        dcf.start();
        break;
    }

    static NtpServer ntpServer(&clk);
    ntpServer.start();

    // Radio module detection blocks for up to ~20 s waiting on module
    // responses - longer than the task-WDT period, so unsubscribe for the
    // duration of this single well-understood blocking step.
    if (bootWdt)
        esp_task_wdt_delete(NULL);

    static RadioModuleDetector radioModuleDetector;
    radioModuleDetector.detectRadioModule(&radioModuleConnector);

    if (bootWdt)
        bootWdt = (esp_task_wdt_add(NULL) == ESP_OK);

    radio_module_type_t radioModuleType = radioModuleDetector.getRadioModuleType();
    if (radioModuleType != RADIO_MODULE_NONE)
    {
        switch (radioModuleType)
        {
        case RADIO_MODULE_HM_MOD_RPI_PCB:
            ESP_LOGI(TAG, "Detected HM-MOD-RPI-PCB:");
            break;
        case RADIO_MODULE_RPI_RF_MOD:
            ESP_LOGI(TAG, "Detected RPI-RF-MOD:");
            break;
        default:
            ESP_LOGI(TAG, "Detected unknown radio module:");
            break;
        }

        ESP_LOGI(TAG, "  Serial: %s", radioModuleDetector.getSerial());
        ESP_LOGI(TAG, "  SGTIN: %s", radioModuleDetector.getSGTIN());
        ESP_LOGI(TAG, "  BidCos Radio MAC: 0x%06" PRIX32, radioModuleDetector.getBidCosRadioMAC());
        ESP_LOGI(TAG, "  HmIP Radio MAC: 0x%06" PRIX32, radioModuleDetector.getHmIPRadioMAC());

        const uint8_t *firmwareVersion = radioModuleDetector.getFirmwareVersion();
        ESP_LOGI(TAG, "  Firmware Version: %d.%d.%d", *firmwareVersion, *(firmwareVersion + 1), *(firmwareVersion + 2));
    }
    else
    {
        ESP_LOGW(TAG, "Radio module could not be detected.");
    }

    radioModuleConnector.resetModule();

    // Add a short delay before starting UDP listener to ensure network is fully stable
    // This helps improve CCU 3 reconnection after firmware updates
    ESP_LOGI(TAG, "Waiting 2 seconds for network stabilization before starting UDP listener...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (bootWdt)
        esp_task_wdt_reset();

    static RawUartUdpListener rawUartUdpLister(&radioModuleConnector);
    rawUartUdpLister.start();

    // Initialize reset info system
    ResetInfo::init();

    // Surface the reset reason at boot. getResetDetails() is the canonical
    // one-shot entry point: it auto-classifies unexplained reboots (panic,
    // watchdog, brownout) when no subsystem had a chance to tag them, reads
    // the persisted diagnostic string, and clears NVS so the next normal
    // reboot does not show stale data. Calling it here makes the boot log
    // the first consumer — MQTT (status/last_reset_reason) and /sysinfo.json
    // later read the cached reset_text_buffer and see the same value. Without
    // this block a user inspecting the serial log after an unexplained
    // overnight reboot sees "no error" because nothing else logs the reason.
    {
        const char *details = sysInfo.getResetReason();
        const char *esp_reason = ResetInfo::getEspResetReason();
        const char *diag = ResetInfo::getLastDiag();
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
        ESP_LOGI(TAG, "Boot reset reason: %s (esp: %s)%s%s",
                 details ? details : "(unknown)",
                 esp_reason ? esp_reason : "(unknown)",
                 (diag && diag[0]) ? " - " : "",
                 (diag && diag[0]) ? diag : "");
        ESP_LOGI(TAG, "Boot heap: %u bytes free, %u bytes largest block",
                 (unsigned)free_heap, (unsigned)largest);
    }

    static UpdateCheck updateCheck(&settings, &sysInfo, &statusLED);
    // NOTE: updateCheck.start() is intentionally deferred until AFTER
    // monitoring_init(). monitoring_init() creates g_net_fetch_mutex, the
    // serialization guard for all outbound TLS handshakes. The UpdateCheck task
    // delays its first fetch by 30 s, but on a slow boot (radio-module
    // detection alone can take ~20 s) that window can close before the mutex
    // exists, letting the first manifest fetch race a CRL/MQTT/WebUI fetch and
    // exhaust the WROOM-32 TLS heap → panic. Starting the task after the mutex
    // exists removes that boot-race for good. The task itself only sleeps until
    // then, so this costs nothing.
    // Register data providers for MQTT status topics (Ethernet link/IP,
    // radio module info, system clock / NTP sync state). Must happen before
    // monitoring_init() so the very first status publish cycle sees them.
    monitoring_set_providers(&ethernet, &radioModuleDetector, &clk);
    monitoring_set_settings(&settings);

    // Initialize monitoring (CheckMK, MQTT)
    esp_err_t monitoringResult = monitoring_init(NULL, &sysInfo, &updateCheck);
    if (monitoringResult != ESP_OK)
    {
        ESP_LOGE(TAG, "Monitoring initialization failed: %s", esp_err_to_name(monitoringResult));
    }

    // g_net_fetch_mutex now exists — safe to start the background update-check
    // task. See the NOTE above for why this ordering matters.
    updateCheck.start();

    // Emit a one-shot "radio module detected" event now that the notification
    // worker is running. Boots without a module stay silent.
    if (radioModuleType != RADIO_MODULE_NONE) {
        events_emit(EVENT_RF_MODULE_DETECTED,
                    radioModuleType == RADIO_MODULE_RPI_RF_MOD ? "RPI-RF-MOD" : "HM-MOD-RPI-PCB");
    }

    // Open HTTP endpoints only after the shared HTTPS mutex exists. This
    // closes the boot-time window in which proxy/OTA requests could overlap
    // the first background update check and exhaust TLS heap.
    static WebUI webUI(&settings, &statusLED, &sysInfo, &updateCheck, &ethernet, &rawUartUdpLister, &radioModuleConnector, &radioModuleDetector);
    webUI.start();

    // If system logging was enabled in settings but the ring buffer could not
    // be allocated during early boot (heap was too tight — typically because
    // the first background UpdateCheck / TLS fetch was running), retry once
    // after all subsystems have finished initialising. By then free heap has
    // usually recovered enough for at least the reduced buffer to fit, so the
    // "not enough memory" state after a restart resolves itself.
    if (settings.getSystemLogEnabled() && !LogManager::instance().isEnabled())
    {
        xTaskCreate([](void *) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            LogManager::begin();
            if (LogManager::instance().isEnabled())
                ESP_LOGI(TAG, "System log capture restored in deferred retry");
            else
                ESP_LOGE(TAG, "Deferred log-buffer retry also failed — heap still too low");
            vTaskDelete(NULL);
        }, "log_retry", 3072, NULL, 2, NULL);
    }

    powerLED.setState(LED_STATE_ON);
    statusLED.setState(LED_STATE_OFF);

    schedule_firmware_validation();

    ESP_LOGI(TAG, "All services started.");

    // Boot finished - stop watching the main task, it stays suspended from
    // here on and would otherwise trip the WDT.
    if (bootWdt)
        esp_task_wdt_delete(NULL);

    vTaskSuspend(NULL);
}
