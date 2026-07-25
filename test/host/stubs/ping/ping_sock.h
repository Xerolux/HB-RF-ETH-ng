#pragma once

#include <cstdint>

#include "esp_err.h"
#include "lwip/inet.h"

using esp_ping_handle_t = void *;

enum esp_ping_profile_t {
    ESP_PING_PROF_DURATION = 8,
};

struct esp_ping_config_t {
    ip_addr_t target_addr{};
    std::uint32_t count = 5;
    std::uint32_t interval_ms = 1000;
    std::uint32_t timeout_ms = 1000;
};

#define ESP_PING_DEFAULT_CONFIG() esp_ping_config_t{}

struct esp_ping_callbacks_t {
    void *cb_args;
    void (*on_ping_success)(esp_ping_handle_t, void *);
    void (*on_ping_timeout)(esp_ping_handle_t, void *);
    void (*on_ping_end)(esp_ping_handle_t, void *);
};

esp_err_t esp_ping_new_session(const esp_ping_config_t *config,
                               const esp_ping_callbacks_t *callbacks,
                               esp_ping_handle_t *handle);
esp_err_t esp_ping_start(esp_ping_handle_t handle);
esp_err_t esp_ping_stop(esp_ping_handle_t handle);
esp_err_t esp_ping_delete_session(esp_ping_handle_t handle);
esp_err_t esp_ping_get_profile(esp_ping_handle_t handle,
                               esp_ping_profile_t profile,
                               void *data,
                               std::uint32_t size);
