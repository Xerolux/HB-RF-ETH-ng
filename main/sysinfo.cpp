/*
 *  sysinfo.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "sysinfo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "reset_info.h"
#include "pins.h"
#include <atomic>
#include <stdio.h>
#include <inttypes.h>

#define DEFAULT_VREF 1100

static const char *TAG = "SysInfo";

// Store hundredths of a percent so reads are atomic on the 32-bit ESP32.
static std::atomic<uint32_t> _cpuUsageHundredths{0};
static char _serial[13];
static const char *_currentVersion;
static board_type_t _board;
static uint64_t _bootTime;

static const UBaseType_t MAX_TASKS = 32;

void updateCPUUsageTask(void *arg)
{
    static TaskStatus_t taskStatus[MAX_TASKS];

    TaskHandle_t idle0Task = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1Task = xTaskGetIdleTaskHandleForCore(1);

    uint32_t totalRunTime = 0, idleRunTime = 0, lastTotalRunTime = 0, lastIdleRunTime = 0;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

        UBaseType_t taskCount = uxTaskGetSystemState(taskStatus, MAX_TASKS, &totalRunTime);

        idleRunTime = 0;

        if (totalRunTime > 0)
        {
            for (int i = 0; i < taskCount; i++)
            {
                TaskStatus_t ts = taskStatus[i];

                if (ts.xHandle == idle0Task || ts.xHandle == idle1Task)
                {
                    idleRunTime += ts.ulRunTimeCounter;
                }
            }
        }

        uint32_t totalDelta = totalRunTime - lastTotalRunTime;
        if (totalDelta > 0) {
            double usage = 100.0 - ((idleRunTime - lastIdleRunTime) * 100.0 / (totalDelta * 2));
            if (usage < 0.0) usage = 0.0;
            if (usage > 100.0) usage = 100.0;
            _cpuUsageHundredths.store((uint32_t)(usage * 100.0 + 0.5));
        }

        lastIdleRunTime = idleRunTime;
        lastTotalRunTime = totalRunTime;
    }
}

uint32_t get_voltage(adc_unit_t adc_unit, adc_channel_t adc_channel, adc_atten_t adc_atten)
{
    // ESP-IDF 5.1 ADC oneshot API
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = adc_unit,
        .clk_src = (adc_oneshot_clk_src_t)0,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return 0;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = adc_atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(adc_handle, adc_channel, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_handle);
        return 0;
    }

    // Calibration
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = adc_unit,
        .atten = adc_atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .default_vref = DEFAULT_VREF,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values: %s", esp_err_to_name(ret));
    }

    // Read ADC with multiple samples for stability
    int adc_raw = 0;
    const int num_samples = 10;
    int successful_samples = 0;
    for (int i = 0; i < num_samples; i++) {
        int sample;
        ret = adc_oneshot_read(adc_handle, adc_channel, &sample);
        if (ret == ESP_OK) {
            adc_raw += sample;
            successful_samples++;
        } else {
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between samples
    }
    if (successful_samples == 0) {
        ESP_LOGE(TAG, "All ADC samples failed");
        if (cali_handle != NULL) {
            adc_cali_delete_scheme_line_fitting(cali_handle);
        }
        adc_oneshot_del_unit(adc_handle);
        return 0;
    }
    adc_raw /= successful_samples;

    // Convert to voltage
    int voltage_mv = 0;
    if (cali_handle != NULL) {
        ret = adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to convert ADC to voltage: %s", esp_err_to_name(ret));
            // Fallback: use raw value with approximation
            voltage_mv = (adc_raw * 3300) / 4095; // 12-bit ADC, 3.3V ref
        }
    } else {
        // No calibration, use raw value approximation
        voltage_mv = (adc_raw * 3300) / 4095;
    }

    // Cleanup
    if (cali_handle != NULL) {
        adc_cali_delete_scheme_line_fitting(cali_handle);
    }
    adc_oneshot_del_unit(adc_handle);

    return voltage_mv;
}

// Last voltage reading from the board-revision sense pin, kept for diagnostic
// output. detectBoard() updates it on every call; getBoardSenseVoltage() and
// getBoardRevisionString() surface it to the WebUI / MQTT / log-share endpoints
// so a user running into "Unknown" can see what the ADC actually measured
// (instead of having to attach a serial console).
static uint32_t _lastBoardSenseVoltage = 0;

board_type_t detectBoard()
{
    uint32_t voltage = get_voltage(BOARD_REV_SENSE_UNIT, BOARD_REV_SENSE_CHANNEL, ADC_ATTEN_DB_12);
    _lastBoardSenseVoltage = voltage;

    // Voltage divider values per board revision:
    //   Rev 1.10/1.11 PUB  → 10K/2K   ≈ 550 mV   (3V3 × 2/(10+2))
    //   Rev 1.8 SK         → 10K/10K  ≈ 1650 mV  (3V3 × 10/20)
    //   Rev 1.8 PUB        → 2K/10K   ≈ 2750 mV  (3V3 × 10/12)
    //   Rev 1.10/1.11 SK   → 1K/12K   ≈ 3050 mV  (3V3 × 12/13)
    //
    // ESP32 ADC1 with ADC_ATTEN_DB_12 has a usable range of roughly 150 mV to
    // ~3100 mV (depending on the calibration eFuse / Vref). The ranges below
    // intentionally leave wide gaps between the nominal divider voltages so
    // that 5-10% ADC error — common with uncalibrated chips — does not flip a
    // board into the wrong bucket, and so that the uppermost band (Rev 1.10/1.11
    // SK) keeps working even when the ADC saturates near its ceiling.

    if (voltage < 250) {
        ESP_LOGW(TAG, "Board-revision ADC reading very low (%" PRIu32 " mV) — assuming no divider / open pin", voltage);
        return BOARD_TYPE_UNKNOWN;
    }
    if (voltage <= 950) {
        return BOARD_TYPE_REV_1_10_PUB;        // 10K/2K  ~550 mV
    }
    if (voltage >= 1200 && voltage <= 2050) {
        return BOARD_TYPE_REV_1_8_SK;          // 10K/10K ~1650 mV
    }
    if (voltage >= 2350 && voltage <= 2850) {
        return BOARD_TYPE_REV_1_8_PUB;         // 2K/10K  ~2750 mV
    }
    if (voltage >= 2851) {
        // Upper bound intentionally open: ADC_ATTEN_DB_12 saturates around
        // 3100–3300 mV, and Rev 1.10/1.11 SK sits at ~3050 mV — any reading
        // 2851+ is far enough from the Rev 1.8 PUB band (2350–2850) to be SK.
        return BOARD_TYPE_REV_1_10_SK;         // 1K/12K  ~3050 mV
    }

    ESP_LOGW(TAG, "Could not determine board, voltage: %" PRIu32 " mV (falls into a gap between known divider values)", voltage);
    return BOARD_TYPE_UNKNOWN;
}

SysInfo::SysInfo()
{
    if (xTaskCreate(updateCPUUsageTask, "UpdateCPUUsage", 4096,
                    NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CPU usage task");
    }

    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(_serial, sizeof(_serial), "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    const esp_app_desc_t* app_desc = esp_app_get_description();
    _currentVersion = app_desc->version;

    _board = detectBoard();

    // Store boot time for uptime calculation
    _bootTime = esp_timer_get_time() / 1000000; // Convert to seconds

}

double SysInfo::getCpuUsage()
{
    return _cpuUsageHundredths.load() / 100.0;
}

double SysInfo::getMemoryUsage()
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);

    return 100.0 - (info.total_free_bytes * 100.0 / (info.total_free_bytes + info.total_allocated_bytes));
}

const char *SysInfo::getSerialNumber()
{
    return _serial;
}

board_type_t SysInfo::getBoardType()
{
    return _board;
}

const char *SysInfo::getCurrentVersion()
{
    return _currentVersion;
}

const char* SysInfo::getBoardRevisionString()
{
    switch (_board)
    {
    case BOARD_TYPE_REV_1_8_PUB:
        return "REV 1.8 (PUB)";
    case BOARD_TYPE_REV_1_8_SK:
        return "REV 1.8 (SK)";
    case BOARD_TYPE_REV_1_10_PUB:
        return "REV 1.10/1.11 (PUB)";
    case BOARD_TYPE_REV_1_10_SK:
        return "REV 1.10/1.11 (SK)";
    default:
        // Include the raw ADC reading so a user looking at the WebUI / log
        // share can immediately see what the divider produced — usually one
        // of: open pin, unsupported board rev, or ADC calibration drift.
        static char unknown_buf[40];
        snprintf(unknown_buf, sizeof(unknown_buf), "Unknown (%" PRIu32 " mV)", _lastBoardSenseVoltage);
        return unknown_buf;
    }
}

uint32_t SysInfo::getBoardSenseVoltage()
{
    return _lastBoardSenseVoltage;
}

uint64_t SysInfo::getUptimeSeconds()
{
    // Get current time in seconds since boot
    uint64_t uptime = (esp_timer_get_time() / 1000000);
    return uptime;
}

const char* SysInfo::getResetReason()
{
    // Use ResetInfo for detailed reset reasons
    return ResetInfo::getResetDetails();
}
