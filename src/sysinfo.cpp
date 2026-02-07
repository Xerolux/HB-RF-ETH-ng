/*
 *  sysinfo.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "sysinfo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/temperature_sensor.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "reset_info.h"
#include "pins.h"

#define DEFAULT_VREF 1100

static const char *TAG = "SysInfo";

static double _cpuUsage;
static char _serial[13];
static const char *_currentVersion;
static board_type_t _board;
static uint64_t _bootTime;
static temperature_sensor_handle_t _temp_sensor = NULL;

void updateCPUUsageTask(void *arg)
{
    TaskStatus_t *taskStatus = (TaskStatus_t *)malloc(25 * sizeof(TaskStatus_t));

    TaskHandle_t idle0Task = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1Task = xTaskGetIdleTaskHandleForCore(1);

    uint32_t totalRunTime = 0, idleRunTime = 0, lastTotalRunTime = 0, lastIdleRunTime = 0;

    for (;;)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        UBaseType_t taskCount = uxTaskGetSystemState(taskStatus, 25, &totalRunTime);

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

        _cpuUsage = 100.0 - ((idleRunTime - lastIdleRunTime) * 100.0 / ((totalRunTime - lastTotalRunTime) * 2));

        lastIdleRunTime = idleRunTime;
        lastTotalRunTime = totalRunTime;
    }

    free(taskStatus);
    vTaskDelete(NULL);
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
    for (int i = 0; i < num_samples; i++) {
        int sample;
        ret = adc_oneshot_read(adc_handle, adc_channel, &sample);
        if (ret == ESP_OK) {
            adc_raw += sample;
        } else {
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between samples
    }
    adc_raw /= num_samples;

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

board_type_t detectBoard()
{
    uint32_t voltage = get_voltage(BOARD_REV_SENSE_UNIT, BOARD_REV_SENSE_CHANNEL, ADC_ATTEN_DB_12);

    switch (voltage) // R31/R32
    {
    case 400 ... 700: // 10K/2K
        return BOARD_TYPE_REV_1_10_PUB;

    case 1500 ... 1800: // 10K/10K
        return BOARD_TYPE_REV_1_8_SK;

    case 2600 ... 2900: // 2K/10K
        return BOARD_TYPE_REV_1_8_PUB;

    case 2901 ... 3200: // 1K/12K
        return BOARD_TYPE_REV_1_10_SK;

    default:
        ESP_LOGW(TAG, "Could not determine board, voltage: %" PRIu32, voltage);
        return BOARD_TYPE_UNKNOWN;
    }
}

SysInfo::SysInfo()
{
    xTaskCreate(updateCPUUsageTask, "UpdateCPUUsage", 4096, NULL, 3, NULL);

    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(_serial, sizeof(_serial), "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    const esp_app_desc_t* app_desc = esp_app_get_description();
    _currentVersion = app_desc->version;

    _board = detectBoard();

    // Store boot time for uptime calculation
    _bootTime = esp_timer_get_time() / 1000000; // Convert to seconds

    // Initialize temperature sensor
    // Note: ESP32 (classic) does not have an internal temperature sensor
    // Only ESP32-S2, ESP32-S3, ESP32-C3, etc. have internal temperature sensors
#if defined(SOC_TEMP_SENSOR_SUPPORTED) && SOC_TEMP_SENSOR_SUPPORTED
    temperature_sensor_config_t temp_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_install(&temp_config, &_temp_sensor));
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_enable(_temp_sensor));
    ESP_LOGI(TAG, "Internal temperature sensor initialized");
#else
    ESP_LOGI(TAG, "ESP32 classic has no internal temperature sensor - external sensor required");
    _temp_sensor = NULL;
#endif
}

double SysInfo::getCpuUsage()
{
    return _cpuUsage;
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

double SysInfo::getSupplyVoltage()
{
    // Measure supply voltage with 2:1 voltage divider
    // GPIO37 (ADC1_CH1) measures half of the actual supply voltage
    uint32_t voltage_mv = get_voltage(SUPPLY_VOLTAGE_SENSE_UNIT, SUPPLY_VOLTAGE_SENSE_CHANNEL, ADC_ATTEN_DB_12);

    if (voltage_mv == 0) {
        ESP_LOGW(TAG, "Failed to read supply voltage, returning 0.0V");
        return 0.0;
    }

    // Apply 2:1 voltage divider correction
    double actual_voltage = (voltage_mv * 2.0) / 1000.0; // Convert to volts

    ESP_LOGD(TAG, "Supply voltage: %.2fV (ADC: %u mV)", actual_voltage, voltage_mv);
    return actual_voltage;
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
        return "REV 1.10 (PUB)";
    case BOARD_TYPE_REV_1_10_SK:
        return "REV 1.10 (SK)";
    default:
        return "Unknown";
    }
}

double SysInfo::getTemperature()
{
#if defined(SOC_TEMP_SENSOR_SUPPORTED) && SOC_TEMP_SENSOR_SUPPORTED
    if (_temp_sensor == NULL) {
        ESP_LOGD(TAG, "Temperature sensor not initialized");
        return -127.0; // Return special value to indicate "not available"
    }

    float temp_celsius = 0.0;
    esp_err_t err = temperature_sensor_get_celsius(_temp_sensor, &temp_celsius);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read temperature: %s", esp_err_to_name(err));
        return -127.0;
    }

    return (double)temp_celsius;
#else
    // ESP32 classic has no internal temperature sensor
    // Return special value -127.0 to indicate "not available"
    // This value is commonly used in temperature sensors to indicate invalid/unavailable reading
    return -127.0;
#endif
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
