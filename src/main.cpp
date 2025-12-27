/*
 *  main.cpp is part of the HB-RF-ETH firmware v2.0
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
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
#include "esp_wifi.h"

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
#include "hmlgw.h"
#include "webui.h"
#include "mdnsserver.h"
#include "ntpserver.h"
#include "esp_ota_ops.h"
#include "updatecheck.h"
#include "monitoring.h"
#include "dtls_encryption.h"
#include "log_manager.h"

static const char *TAG = "HB-RF-ETH";

// Heap monitoring task
static void heap_monitor_task(void *pvParameters)
{
    LED *statusLED = (LED *)pvParameters;
    uint32_t low_water_mark = 0;
    const uint32_t critical_threshold = 20480;  // 20KB critical threshold
    UBaseType_t stack_watermark_min = 2560;  // Start with allocated size

    for (;;)
    {
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_free_heap = esp_get_minimum_free_heap_size();

        if (min_free_heap < low_water_mark || low_water_mark == 0) {
            low_water_mark = min_free_heap;
            ESP_LOGI(TAG, "Heap low water mark: %" PRIu32 " bytes", low_water_mark);
        }

        if (free_heap < critical_threshold) {
            ESP_LOGW(TAG, "Low heap memory: %" PRIu32 " bytes free (min: %" PRIu32 ")",
                     free_heap, min_free_heap);
        }

        // Monitor stack watermark for this task
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        if (watermark < stack_watermark_min) {
            stack_watermark_min = watermark;
            ESP_LOGI(TAG, "heap_monitor stack watermark: %u bytes free (allocated: 2560)",
                     watermark * sizeof(StackType_t));
        }

        // Log heap stats every 5 minutes
        static uint32_t count = 0;
        count++;
        if (count % 60 == 0) {
            ESP_LOGI(TAG, "Heap status - Free: %" PRIu32 " bytes, Min free: %" PRIu32 " bytes",
                     free_heap, min_free_heap);
            ESP_LOGI(TAG, "heap_monitor stack - Min watermark: %u bytes (%.1f%% used)",
                     stack_watermark_min * sizeof(StackType_t),
                     100.0 * (2560 - stack_watermark_min * sizeof(StackType_t)) / 2560.0);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);  // Check every 5 seconds
    }
}

extern "C"
{
    void app_main(void);
}

void app_main()
{
    // Initialize logging immediately to capture startup events
    LogManager::instance().start();

    // CRITICAL: Mark OTA update as valid immediately after boot
    // This must be done BEFORE any complex initialization that could cause a panic
    // Otherwise, ESP-IDF will automatically rollback to the previous firmware
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "OTA firmware validated successfully");

    // CRITICAL: Disable all power management features for maximum performance
    // Radio signals require immediate processing without delays
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,  // Maximum CPU frequency
        .min_freq_mhz = 240,  // No CPU frequency scaling
        .light_sleep_enable = false  // Disable light sleep
    };
    esp_err_t pm_err = esp_pm_configure(&pm_config);
    if (pm_err == ESP_OK) {
        ESP_LOGI(TAG, "Power management disabled - running at full performance");
    } else if (pm_err == ESP_ERR_NOT_SUPPORTED) {
        // Power management not compiled in - system runs at full performance by default
        ESP_LOGI(TAG, "Power management not available - running at full performance (default)");
    } else {
        ESP_LOGW(TAG, "Failed to configure power management: %s", esp_err_to_name(pm_err));
    }

    // Disable WiFi power saving (even though we use Ethernet)
    // This ensures no interference from WiFi subsystem
    esp_wifi_set_ps(WIFI_PS_NONE);

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
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    Settings settings;

    LED powerLED(LED_PWR_PIN);
    LED statusLED(LED_STATUS_PIN);

    LED redLED(HM_RED_PIN);
    LED greenLED(HM_GREEN_PIN);
    LED blueLED(HM_BLUE_PIN);

    LED::start(&settings);

    powerLED.setState(LED_STATE_BLINK);
    statusLED.setState(LED_STATE_BLINK_INV);

    redLED.setState(LED_STATE_OFF);
    greenLED.setState(LED_STATE_OFF);
    blueLED.setState(LED_STATE_OFF);

    SysInfo sysInfo;

    PushButtonHandler pushButton;
    pushButton.handleStartupFactoryReset(&powerLED, &statusLED, &settings);

    RadioModuleConnector radioModuleConnector(&redLED, &greenLED, &blueLED);
    radioModuleConnector.start();

    Ethernet ethernet(&settings);
    ethernet.start();

    setenv("TZ", "UTC0", 1);
    tzset();

    Rtc *rtc = NULL;
    rtc = Rtc::detect();

    SystemClock clk(rtc);
    clk.start();

    DCF dcf(&settings, &clk);
    NtpClient ntpClient(&settings, &clk);
    GPS gps(&settings, &clk);

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

    MDns mdns;
    mdns.start(&settings);

    NtpServer ntpServer(&clk);
    ntpServer.start();

    RadioModuleDetector radioModuleDetector;
    radioModuleDetector.detectRadioModule(&radioModuleConnector);

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

        // Only reset module if one was detected
        radioModuleConnector.resetModule();
    }
    else
    {
        ESP_LOGW(TAG, "Radio module could not be detected. System will continue without radio functionality.");
    }

    // Initialize DTLS encryption (works only in Raw UART mode)
    DTLSEncryption dtlsEncryption;
    dtls_mode_t dtls_mode = (dtls_mode_t)settings.getDTLSMode();
    dtls_cipher_suite_t dtls_cipher = (dtls_cipher_suite_t)settings.getDTLSCipherSuite();

    RawUartUdpListener* rawUartUdpLister = NULL;
    Hmlgw* hmlgw = NULL;

    // INTENTIONAL MEMORY ALLOCATION PATTERN:
    // The following objects (hmlgw, rawUartUdpLister) are allocated with 'new'
    // and intentionally NOT deleted. They are singleton services that persist
    // for the entire application lifetime (until device reset).
    // This is by design - not a memory leak.

    if (settings.getHmlgwEnabled()) {
        ESP_LOGI(TAG, "Starting HMLGW mode (DTLS not supported in HM-LGW)");
        hmlgw = new Hmlgw(&radioModuleConnector, settings.getHmlgwPort(), settings.getHmlgwKeepAlivePort());
        hmlgw->start();
    } else {
        ESP_LOGI(TAG, "Starting Raw UART UDP mode");

        // DTLS only works in Raw UART mode (not compatible with HM-LGW or Analyzer)
        if (settings.getAnalyzerEnabled() && dtls_mode != DTLS_MODE_DISABLED)
        {
            ESP_LOGW(TAG, "DTLS disabled: Analyzer requires unencrypted data");
            dtls_mode = DTLS_MODE_DISABLED;
        }

        if (dtls_mode != DTLS_MODE_DISABLED)
        {
            if (dtlsEncryption.init(dtls_mode, dtls_cipher))
            {
                dtlsEncryption.setSessionResumption(settings.getDTLSSessionResumption());
                dtlsEncryption.setRequireClientCert(settings.getDTLSRequireClientCert());
                ESP_LOGI(TAG, "DTLS encryption initialized successfully");
            }
            else
            {
                ESP_LOGW(TAG, "DTLS initialization failed, continuing without encryption");
            }
        }

        rawUartUdpLister = new RawUartUdpListener(&radioModuleConnector, &settings, &dtlsEncryption);
        rawUartUdpLister->start();
    }

    UpdateCheck updateCheck(&settings, &sysInfo, &statusLED);
    updateCheck.start();

    WebUI webUI(&settings, &statusLED, &sysInfo, &updateCheck, &ethernet, rawUartUdpLister, &radioModuleConnector, &radioModuleDetector, &dtlsEncryption);
    webUI.start();

    // Initialize monitoring (SNMP, CheckMK, MQTT)
    monitoring_init(NULL, &sysInfo, &updateCheck);

    powerLED.setState(LED_STATE_ON);
    statusLED.setState(LED_STATE_OFF);

    // Log initial heap status
    ESP_LOGI(TAG, "System initialized. Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Largest free block: %lu bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // Start heap monitoring task
    // Increased to 2560 bytes to accommodate ESP_LOG* macro stack usage
    xTaskCreate(heap_monitor_task, "heap_monitor", 2560, &statusLED, 1, NULL);

    vTaskSuspend(NULL);
}
