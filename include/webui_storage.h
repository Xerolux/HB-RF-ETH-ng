#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_http_server.h"

using httpd_uri_func = esp_err_t (*)(httpd_req_t *request);

// Increment only when the firmware/WebUI API contract changes incompatibly.
constexpr uint32_t HB_WEBUI_API_VERSION = 1;

struct WebUIStorageStatus
{
    bool partitionFound;
    bool mounted;
    bool valid;
    bool updateActive;
    size_t partitionSize;
    size_t totalBytes;
    size_t usedBytes;
    size_t bytesWritten;
    uint32_t apiVersion;
    char version[32];
    char minFirmwareVersion[32];
    char lastError[96];
};

/**
 * Mount the existing `spiffs` partition without formatting it.
 *
 * The function deliberately never formats a failed or empty partition. Existing
 * devices therefore keep the embedded New Design as recovery fallback. A
 * persistent NVS transaction marker is checked before mounting; after a reboot
 * caused by an interrupted update, the unverified SPIFFS header is invalidated
 * and the device safely continues with the embedded New Design.
 */
esp_err_t webui_storage_init();

/** Return a snapshot of the external WebUI storage state. */
WebUIStorageStatus webui_storage_get_status();

/** True only when the partition is mounted and the New Design manifest is valid. */
bool webui_storage_is_valid();

/**
 * Start a raw SPIFFS image update.
 *
 * expectedSha256Hex may be null or empty. If supplied, it must contain exactly
 * 64 hexadecimal characters. The embedded New Design remains available while
 * the separate partition is erased and written. The persistent transaction
 * marker is committed before the first destructive flash operation.
 */
esp_err_t webui_storage_update_begin(size_t expectedSize,
                                     const char *expectedSha256Hex);

/** Stream one image chunk directly into the SPIFFS partition. */
esp_err_t webui_storage_update_write(const uint8_t *data, size_t length);

/** Finalize SHA-256 verification, remount and validate the New Design image. */
esp_err_t webui_storage_update_finish();

/** Abort an active update and invalidate the partial filesystem image. */
void webui_storage_update_abort();
