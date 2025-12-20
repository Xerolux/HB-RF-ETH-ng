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

static const char *TAG = "HB-RF-ETH";

extern "C"
{
    void app_main(void);
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
    }
    else
    {
        ESP_LOGW(TAG, "Radio module could not be detected.");
    }

    radioModuleConnector.resetModule();

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

    esp_ota_mark_app_valid_cancel_rollback();

    vTaskSuspend(NULL);
}
