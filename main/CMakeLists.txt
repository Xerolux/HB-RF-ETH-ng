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
                           cjson
                           app_update
                           esp_adc
                           esp_partition
                           spi_flash
                           spiffs
                           )

# Transition firmware: keep the New Design embedded as a guaranteed fallback
# while the same New Design can also be served and updated independently from
# the existing 0x50000-byte `spiffs` partition.
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.css.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.js.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/index.html.gz" BINARY)
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

# Build a raw WWW image containing only the compressed New Design assets. The
# resulting build/spiffs.bin is accepted by POST /api/webui/update and published
# as the independent New Design release asset.
spiffs_create_partition_image(spiffs "../webui/spiffs_image" FLASH_IN_PROJECT)
