#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

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
    char version[32];
    char lastError[96];
};

/**
 * Mount the existing `spiffs` partition without formatting it.
 *
 * The function deliberately never formats a failed or empty partition. Existing
 * devices therefore keep the embedded WebUI as a guaranteed recovery fallback.
 */
esp_err_t webui_storage_init();

/** Return a snapshot of the external WebUI storage state. */
WebUIStorageStatus webui_storage_get_status();

/** True only when the partition is mounted and all mandatory WebUI files exist. */
bool webui_storage_is_valid();

/**
 * Start a raw SPIFFS image update.
 *
 * expectedSha256Hex may be null or empty. If supplied, it must contain exactly
 * 64 hexadecimal characters. The current external WebUI is unmounted before the
 * partition is erased; the embedded WebUI remains available throughout.
 */
esp_err_t webui_storage_update_begin(size_t expectedSize,
                                     const char *expectedSha256Hex);

/** Stream one image chunk directly into the SPIFFS partition. */
esp_err_t webui_storage_update_write(const uint8_t *data, size_t length);

/** Finalize SHA-256 verification, remount and validate the new WebUI image. */
esp_err_t webui_storage_update_finish();

/** Abort an active update and attempt to remount whatever remains. */
void webui_storage_update_abort();
