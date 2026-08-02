#pragma once

#include "esp_err.h"

typedef void (*restart_eth_pause_fn_t)(void);
typedef esp_err_t (*restart_network_stop_fn_t)(void);

void full_system_restart();
// Use only when the caller already owns ota_operation_try_begin(). This is
// required by manual firmware upload and other exclusive NVS mutations so a
// competing restart cannot mistake another task's reservation for its own.
void full_system_restart_with_reserved_operation();
void set_flash_pause_enabled(bool enabled);
void register_restart_eth_pause_callback(restart_eth_pause_fn_t cb);
void register_restart_network_stop_callback(restart_network_stop_fn_t cb);
