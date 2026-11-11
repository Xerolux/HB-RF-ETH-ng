/*
 *  event_journal.cpp is part of the HB-RF-ETH-ng firmware
 *
 *  RAM ring + NVS mirror of recent device events. See event_journal.h.
 *
 *  This file is licensed under the Creative Commons
 *  Attribution-NonCommercial-ShareAlike 4.0 International License.
 *  http://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#include "event_journal.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs_storage_lock.h"

static const char *TAG                     = "EVENT_JOURNAL";
static constexpr const char *NVS_NAMESPACE = "journal";
static constexpr const char *NVS_KEY       = "ring";
// The original format is retained, including full detail text and oldest-first
// ordering. Only the diagnostic retention budget changes; no settings migrate.
static constexpr size_t LEGACY_CAPACITY     = 40;
static constexpr size_t NVS_RESERVE_ENTRIES = 16;
static constexpr int64_t RETRY_DELAY_US     = 5LL * 60 * 1000000;
static_assert(sizeof(event_journal_entry_t) == 112, "Journal storage layout changed");
static_assert(EVENT_JOURNAL_PERSIST_CAPACITY * sizeof(event_journal_entry_t) <= 1024,
              "Journal NVS budget must stay below 1 KiB");

static event_journal_entry_t s_ring[EVENT_JOURNAL_CAPACITY];
static size_t s_count           = 0;
static bool s_dirty             = false;
static uint32_t s_generation    = 0;
static int64_t s_retry_after_us = 0;
static StaticSemaphore_t s_mutex_storage;
static SemaphoreHandle_t s_mutex = NULL;

static const char *event_id_name(uint8_t id)
{
    switch (id) {
        case 0:
            return "eth_link_down";
        case 1:
            return "eth_link_up";
        case 2:
            return "rf_module_lost";
        case 3:
            return "rf_module_detected";
        case 7:
            return "mqtt_disconnected";
        case 8:
            return "mqtt_reconnected";
        case 9:
            return "factory_reset";
        case 10:
            return "restart";
        case 11:
            return "ccu_connected";
        case 12:
            return "ccu_disconnected";
        case 13:
            return "low_heap";
        case 14:
            return "crash_transcript";
        case 254:
            return "test";
        case EVENT_JOURNAL_WITNESS_DEFERRED:
            return "net_witness_deferred";
        case EVENT_JOURNAL_SCHEDULED_RESTART:
            return "scheduled_restart";
        default:
            return "unknown";
    }
}

const char *event_journal_name(uint8_t event_id)
{
    return event_id_name(event_id);
}

void event_journal_init(void)
{
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_storage);
    if (!s_mutex) return;
    s_count          = 0;
    s_dirty          = false;
    s_generation     = 0;
    s_retry_after_us = 0;

    NvsStorageLock storage_lock(portMAX_DELAY, "journal.load");
    if (!storage_lock) return;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;

    size_t len = 0;
    if (nvs_get_blob(handle, NVS_KEY, NULL, &len) == ESP_OK && len > 0 &&
        len <= LEGACY_CAPACITY * sizeof(event_journal_entry_t) &&
        len % sizeof(event_journal_entry_t) == 0) {
        // Boot previously placed 4480 bytes on the 6144-byte main stack. A
        // bounded temporary allocation also handles the old 40-entry mirror.
        event_journal_entry_t *tmp = (event_journal_entry_t *)malloc(len);
        if (tmp) {
            const size_t n = len / sizeof(event_journal_entry_t);
            if (nvs_get_blob(handle, NVS_KEY, tmp, &len) == ESP_OK) {
                s_count = n > EVENT_JOURNAL_CAPACITY ? EVENT_JOURNAL_CAPACITY : n;
                for (size_t i = 0; i < s_count; i++) {
                    s_ring[i]                                      = tmp[n - 1 - i];
                    s_ring[i].detail[sizeof(s_ring[i].detail) - 1] = '\0';
                }
                s_dirty = n > EVENT_JOURNAL_PERSIST_CAPACITY;
                ESP_LOGI(TAG, "Loaded %u journal entries from NVS", (unsigned)s_count);
            }
            free(tmp);
        }
    }
    nvs_close(handle);
}

void event_journal_add(uint8_t event_id, const char *detail)
{
    if (!s_mutex) return;
    const time_t now = time(NULL);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_count == EVENT_JOURNAL_CAPACITY)
        memmove(&s_ring[1], &s_ring[0], sizeof(s_ring) - sizeof(s_ring[0]));
    else if (s_count > 0)
        memmove(&s_ring[1], &s_ring[0], s_count * sizeof(s_ring[0]));
    if (s_count < EVENT_JOURNAL_CAPACITY) s_count++;

    event_journal_entry_t *e = &s_ring[0];
    e->epoch                 = now > 1600000000 ? (int64_t)now : 0;
    e->event_id              = event_id;
    if (detail) {
        strncpy(e->detail, detail, sizeof(e->detail) - 1);
        e->detail[sizeof(e->detail) - 1] = '\0';
    } else {
        e->detail[0] = '\0';
    }
    s_dirty = true;
    s_generation++;
    xSemaphoreGive(s_mutex);
}

static esp_err_t persist_snapshot(nvs_handle_t handle, const event_journal_entry_t *entries,
                                  size_t count)
{
    nvs_stats_t stats = {};
    esp_err_t err     = nvs_get_stats(NULL, &stats);
    if (err != ESP_OK) return err;
    // Blob index/chunk metadata plus 32-byte data entries. Keep the same
    // housekeeping reserve as Settings; diagnostic data has lower priority.
    while (count > 0) {
        const size_t needed = 3 + (count * sizeof(*entries) + 31) / 32;
        if (stats.available_entries >= needed + NVS_RESERVE_ENTRIES) break;
        entries++;
        count--;
    }
    if (count == 0) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    err = nvs_set_blob(handle, NVS_KEY, entries, count * sizeof(*entries));
    if (err == ESP_OK) err = nvs_commit(handle);
    return err;
}

void event_journal_flush(void)
{
    if (!s_mutex) return;
    // Do not wait behind a settings/restore transaction. Another flush caller
    // is excluded by this same lock, while producers only take the ring lock.
    NvsStorageLock storage_lock(0, "journal.flush");
    if (!storage_lock) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!s_dirty || esp_timer_get_time() < s_retry_after_us) {
        xSemaphoreGive(s_mutex);
        return;
    }
    const size_t n =
        s_count < EVENT_JOURNAL_PERSIST_CAPACITY ? s_count : EVENT_JOURNAL_PERSIST_CAPACITY;
    const uint32_t generation  = s_generation;
    event_journal_entry_t *tmp = (event_journal_entry_t *)malloc(n * sizeof(*tmp));
    if (tmp) {
        for (size_t i = 0; i < n; i++)
            tmp[i] = s_ring[n - 1 - i];
    }
    // Producers can continue while flash I/O runs. Generation tracking keeps
    // an event added during commit dirty for the next flush.
    xSemaphoreGive(s_mutex);

    esp_err_t err = ESP_ERR_NO_MEM;
    if (tmp) {
        nvs_handle_t handle;
        err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            size_t old_len              = 0;
            const bool oversized_legacy = nvs_get_blob(handle, NVS_KEY, NULL, &old_len) == ESP_OK &&
                                          old_len > EVENT_JOURNAL_PERSIST_CAPACITY * sizeof(*tmp) &&
                                          old_len <= LEGACY_CAPACITY * sizeof(*tmp) &&
                                          old_len % sizeof(*tmp) == 0;
            err                         = persist_snapshot(handle, tmp, n);
            if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE && oversized_legacy) {
                // Only our disposable diagnostic key may be reclaimed. All
                // loaded events remain in RAM. Never erase a namespace or the
                // NVS partition: credentials/certificates/settings live there.
                err = nvs_erase_key(handle, NVS_KEY);
                if (err == ESP_OK) err = nvs_commit(handle);
                if (err == ESP_OK) err = persist_snapshot(handle, tmp, n);
            }
            nvs_close(handle);
        }
        free(tmp);
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (err == ESP_OK) {
        if (s_generation == generation) s_dirty = false;
        s_retry_after_us = 0;
    } else {
        s_retry_after_us = esp_timer_get_time() + RETRY_DELAY_US;
    }
    xSemaphoreGive(s_mutex);
    storage_lock.release();
    if (err != ESP_OK)
        ESP_LOGW(TAG, "Journal persist deferred for 5 minutes: %s", esp_err_to_name(err));
}

size_t event_journal_count(void)
{
    if (!s_mutex) return 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const size_t count = s_count;
    xSemaphoreGive(s_mutex);
    return count;
}

bool event_journal_copy(size_t index, event_journal_entry_t *entry)
{
    if (!s_mutex || !entry) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool found = index < s_count;
    if (found) *entry = s_ring[index];
    xSemaphoreGive(s_mutex);
    return found;
}
