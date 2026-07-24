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

# Keep the WebUI version independent from the firmware version.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../webui/package.json" WEBUI_PACKAGE_JSON)
string(JSON HB_WEBUI_VERSION GET "${WEBUI_PACKAGE_JSON}" version)
target_compile_definitions(${COMPONENT_LIB} PRIVATE HB_WEBUI_VERSION=\"${HB_WEBUI_VERSION}\")

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
# Build the raw WWW image from the gzip-compressed New Design shell.
# Optional PWA/icon assets stay embedded and are served through the fallback
# handlers, preserving functionality while fitting the existing 320 KiB SPIFFS.
# The resulting build/spiffs.bin is accepted by POST /api/webui/update and
# published as the independent New Design release asset.
spiffs_create_partition_image(spiffs "../webui/spiffs_image" FLASH_IN_PROJECT)
