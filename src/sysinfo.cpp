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
#include "pins.h"

static const char *TAG = "SysInfo";

static volatile float _cpuUsage = 0.0f;
static volatile float _memoryUsage = 0.0f;
static volatile float _temperature = -127.0f;
static volatile uint32_t _lastSysInfoRequestTime = 0;
static char _serial[13];
static const char *_currentVersion;
static const char *_firmwareVariant;
static board_type_t _board;
static uint64_t _bootTime;
static temperature_sensor_handle_t _temp_sensor = NULL;

void updateCPUUsageTask(void *arg)
{
    TaskStatus_t *taskStatus = (TaskStatus_t *)malloc(25 * sizeof(TaskStatus_t));
    if (!taskStatus) {
        ESP_LOGE(TAG, "Failed to allocate memory for task status");
        vTaskDelete(NULL);
        return;
    }

    TaskHandle_t idle0Task = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1Task = xTaskGetIdleTaskHandleForCore(1);

    uint32_t totalRunTime = 0, idleRunTime = 0, lastTotalRunTime = 0, lastIdleRunTime = 0;

    for (;;)
    {
        // Optimize performance: Only calculate CPU usage if SysInfo was recently requested
        // If no one is looking at the dashboard, we don't need to burn cycles calculating CPU usage
        bool active = (xTaskGetTickCount() * portTICK_PERIOD_MS) - _lastSysInfoRequestTime < 5000;

        vTaskDelay((active ? 1000 : 2000) / portTICK_PERIOD_MS);

        if (active) {
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

            // CPU Usage
            _cpuUsage = 100.0f - ((idleRunTime - lastIdleRunTime) * 100.0f / ((totalRunTime - lastTotalRunTime) * 2));

            lastIdleRunTime = idleRunTime;
            lastTotalRunTime = totalRunTime;
        }

        // Memory Usage
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
        _memoryUsage = 100.0f - (info.total_free_bytes * 100.0f / (info.total_free_bytes + info.total_allocated_bytes));

        // Temperature
#if defined(SOC_TEMP_SENSOR_SUPPORTED) && SOC_TEMP_SENSOR_SUPPORTED
        if (_temp_sensor) {
            float temp_celsius = 0.0f;
            if (temperature_sensor_get_celsius(_temp_sensor, &temp_celsius) == ESP_OK) {
                _temperature = temp_celsius;
            } else {
                _temperature = -127.0f;
            }
        }
#endif
    }

    free(taskStatus);
    vTaskDelete(NULL);
}

// Board detection removed - ADC voltage measurement not functional
// All boards will report as UNKNOWN type
board_type_t detectBoard()
{
    ESP_LOGI(TAG, "Board detection disabled (ADC not functional)");
    return BOARD_TYPE_UNKNOWN;
}

SysInfo::SysInfo()
{
    xTaskCreate(updateCPUUsageTask, "UpdateCPUUsage", 4096, NULL, 3, NULL);

    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(_serial, sizeof(_serial), "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    const esp_app_desc_t* app_desc = esp_app_get_description();
    _currentVersion = app_desc->version;

    // Set firmware variant from compile-time define
#ifndef FIRMWARE_VARIANT
    _firmwareVariant = "unknown";
#else
    _firmwareVariant = FIRMWARE_VARIANT;
#endif

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

double SysInfo::getCpuUsage() const
{
    return (double)_cpuUsage;
}

double SysInfo::getMemoryUsage() const
{
    return (double)_memoryUsage;
}

const char *SysInfo::getSerialNumber() const
{
    return _serial;
}

board_type_t SysInfo::getBoardType() const
{
    return _board;
}

const char *SysInfo::getCurrentVersion() const
{
    return _currentVersion;
}

const char *SysInfo::getFirmwareVariant() const
{
    return _firmwareVariant;
}

double SysInfo::getSupplyVoltage() const
{
    // ADC voltage measurement disabled (not functional)
    return 0.0;
}

const char* SysInfo::getBoardRevisionString() const
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

double SysInfo::getTemperature() const
{
    // Return cached value updated by background task
    return (double)_temperature;
}

uint64_t SysInfo::getUptimeSeconds() const
{
    // Get current time in seconds since boot
    uint64_t uptime = (esp_timer_get_time() / 1000000);
    return uptime;
}

const char* SysInfo::getResetReason() const
{
    esp_reset_reason_t reason = esp_reset_reason();

    switch (reason)
    {
    case ESP_RST_POWERON:
        return "sysinfo.reset.poweron";
    case ESP_RST_SW:
        return "sysinfo.reset.sw";
    case ESP_RST_PANIC:
        return "sysinfo.reset.panic";
    case ESP_RST_INT_WDT:
        return "sysinfo.reset.int_wdt";
    case ESP_RST_TASK_WDT:
        return "sysinfo.reset.task_wdt";
    case ESP_RST_WDT:
        return "sysinfo.reset.wdt";
    case ESP_RST_DEEPSLEEP:
        return "sysinfo.reset.deepsleep";
    case ESP_RST_BROWNOUT:
        return "sysinfo.reset.brownout";
    case ESP_RST_SDIO:
        return "sysinfo.reset.sdio";
    case ESP_RST_EXT:
        return "sysinfo.reset.ext";
    default:
        return "sysinfo.reset.unknown";
    }
}

void SysInfo::markSysInfoRequested()
{
    _lastSysInfoRequestTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
}
