#include "system_reset.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"

static const char *TAG = "SystemReset";

void full_system_restart() {
    ESP_LOGI(TAG, "Initiating full system restart...");

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

    // Hold reset for 500ms to ensure peripherals are fully reset
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Peripherals reset complete. Restarting ESP32...");
    esp_restart();
}
