/*
 *  rtcdriver.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "rtcdriver.h"
#include <stdint.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

// Forward declare GPIO pins from pins.h to avoid circular includes
#define HM_SDA_PIN GPIO_NUM_18
#define HM_SCL_PIN GPIO_NUM_5

static const char *TAG = "RTC";

static uint8_t bcd2bin(uint8_t val)
{
    return val - 6 * (val >> 4);
}

static uint8_t bin2bcd(uint8_t val)
{
    return val + 6 * (val / 10);
}

const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static i2c_master_bus_handle_t i2c_bus = NULL;

static void i2c_master_init()
{
    if (i2c_bus != NULL)
        return;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = HM_SDA_PIN,
        .scl_io_num = HM_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true, .allow_pd = false }
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
    }
}

Rtc *Rtc::detect()
{
    RtcDS3231 *ds3231 = new RtcDS3231();
    if (ds3231->begin())
    {
        ESP_LOGI(TAG, "DS3231 RTC found and initialized.");
        return ds3231;
    }
    else
    {
        delete ds3231;
    }

    RtcRX8130 *rx9130 = new RtcRX8130();
    if (rx9130->begin())
    {
        ESP_LOGI(TAG, "RX9130 RTC found and initialized.");
        return rx9130;
    }
    else
    {
        delete rx9130;
    }

    ESP_LOGE(TAG, "No RTC found.");
    return NULL;
}

Rtc::Rtc(uint8_t address, uint8_t reg_start) : _address(address), _reg_start(reg_start)
{
    i2c_master_init();
}

Rtc::~Rtc()
{
}

bool Rtc::begin()
{
    i2c_master_dev_handle_t device_handle;
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _address,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &device_handle);
    if (ret != ESP_OK) {
        return false;
    }

    // Zero-length write to check device presence
    ret = i2c_master_transmit(device_handle, NULL, 0, -1);
    i2c_master_bus_rm_device(device_handle);

    return ret == ESP_OK;
}

struct timeval Rtc::GetTime()
{
    struct timeval res = {};

    uint8_t rawData[7] = {0};

    i2c_master_dev_handle_t device_handle;
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _address,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &device_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device");
        res.tv_sec = 0;
        res.tv_usec = 0;
        return res;
    }

    // Write register address, then read data
    uint8_t reg_addr = _reg_start;
    ret = i2c_master_transmit_receive(device_handle, &reg_addr, 1, rawData, sizeof(rawData), -1);
    i2c_master_bus_rm_device(device_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read time from RTC");
        res.tv_sec = 0;
        res.tv_usec = 0;
        return res;
    }

    res.tv_sec = bcd2bin(rawData[0]);         // seconds
    res.tv_sec += bcd2bin(rawData[1]) * 60;   // minutes
    res.tv_sec += bcd2bin(rawData[2]) * 3600; // hours

    uint16_t days = bcd2bin(rawData[4]);
    uint8_t month = bcd2bin(rawData[5]);
    uint8_t year = bcd2bin(rawData[6]);

    for (uint8_t i = 1; i < month; ++i)
    {
        days += daysInMonth[i - 1];
    }

    if (month > 2 && year % 4 == 0)
        days++;

    days += 365 * year + (year + 3) / 4 - 1;

    res.tv_sec += days * 86400;

    res.tv_sec += 10957 * 86400; // epoch diff 1970 vs. 2000

    return res;
}

void Rtc::SetTime(struct timeval now)
{
    now.tv_sec -= 10957 * 86400; // epoch diff 1970 vs. 2000

    uint8_t seconds = now.tv_sec % 60;
    now.tv_sec /= 60;
    uint8_t minutes = now.tv_sec % 60;
    now.tv_sec /= 60;
    uint8_t hours = now.tv_sec % 24;

    uint16_t days = now.tv_sec / 24;

    uint8_t leap;
    uint8_t year;

    for (year = 0;; year++)
    {
        leap = year % 4 == 0;
        if (days < 365 + leap)
            break;
        days -= 365 + leap;
    }

    uint8_t month;
    for (month = 1;; month++)
    {
        uint8_t daysPerMonth = daysInMonth[month - 1];
        if (leap && month == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    days++;

    uint8_t writeData[8] = {
        _reg_start,
        bin2bcd(seconds),
        bin2bcd(minutes),
        bin2bcd(hours),
        0,
        bin2bcd(days),
        bin2bcd(month),
        bin2bcd(year)
    };

    i2c_master_dev_handle_t device_handle;
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _address,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &device_handle);
    if (ret == ESP_OK) {
        ret = i2c_master_transmit(device_handle, writeData, sizeof(writeData), -1);
        i2c_master_bus_rm_device(device_handle);
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write time to RTC");
    }
}

RtcDS3231::RtcDS3231() : Rtc::Rtc(0x68, 0)
{
}

RtcRX8130::RtcRX8130() : Rtc::Rtc(0x32, 0x10)
{
}

bool RtcRX8130::begin()
{
    if (Rtc::begin())
    {
        uint8_t writeData[2] = {0x1f, 0x31};

        i2c_master_dev_handle_t device_handle;
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = _address,
            .scl_speed_hz = 100000,
        };

        esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &device_handle);
        if (ret == ESP_OK) {
            ret = i2c_master_transmit(device_handle, writeData, sizeof(writeData), -1);
            i2c_master_bus_rm_device(device_handle);
        }

        return ret == ESP_OK;
    }
    else
    {
        return false;
    }
}
