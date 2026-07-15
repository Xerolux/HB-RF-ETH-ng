# CMakeLists.txt for HB-RF-ETH-ng firmware
# Updated for ESP-IDF 6.0 - explicit component dependencies required

FILE(GLOB_RECURSE app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "../include"
                       REQUIRES
                           driver
                           esp_driver_gpio
                           esp_driver_uart
                           esp_driver_ledc
                           esp_driver_i2c
                           esp_driver_spi
                           esp_driver_tsens
                           esp_eth
                           esp_netif
                           esp_event
                           esp_http_server
                           esp_http_client
                           esp_https_ota
                           esp_timer
                           nvs_flash
                           lwip
                           mbedtls
                           app_update
                           esp_adc
                           spi_flash
                           )

target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.css.br" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.js.br" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/index.html.br" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/favicon.ico.gz" BINARY)
# PWA assets — make the WebUI installable ("Add to Home screen" on iOS,
# "Install app" on Android). Single icon serves any/maskable/apple-touch.
# Gzipped by rename_webui_files.py.
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/manifest.webmanifest.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/icon-256.png.gz" BINARY)
# Embedded firmware release archive. Regenerate with scripts/update_archive.py
# whenever archive.json changes (release process). Served from flash by
# /api/firmware_archive so the WebUI never has to fetch it live from GitHub.
target_add_binary_data(${COMPONENT_TARGET} "generated/archive.json.gz" BINARY)
