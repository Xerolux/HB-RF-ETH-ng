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

#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "EVENT_JOURNAL";

// NVS namespace/key of the persisted mirror. The blob stores the same ring
// the RAM keeps, oldest first (load reverses into newest-first).
static constexpr const char *NVS_NAMESPACE = "journal";
static constexpr const char *NVS_KEY = "ring";

static event_journal_entry_t s_ring[EVENT_JOURNAL_CAPACITY];
static size_t s_count = 0;
static bool s_dirty = false;
// Single-writer discipline is enough: add() is called from the same emit
// origins serially and the ring ops are trivial; the mutex only keeps a
// concurrent flush() from tearing a snapshot write.
static SemaphoreHandle_t s_mutex = NULL;

static const char *event_id_name(uint8_t id)
{
    switch (id)
    {
    case 0: return "eth_link_down";
    case 1: return "eth_link_up";
    case 2: return "rf_module_lost";
    case 3: return "rf_module_detected";
    case 7: return "mqtt_disconnected";
    case 8: return "mqtt_reconnected";
    case 9: return "factory_reset";
    case 10: return "restart";
    case 11: return "ccu_connected";
    case 12: return "ccu_disconnected";
    case 13: return "low_heap";
    case 14: return "crash_transcript";
    case 254: return "test";
    case EVENT_JOURNAL_WITNESS_DEFERRED: return "net_witness_deferred";
    case EVENT_JOURNAL_SCHEDULED_RESTART: return "scheduled_restart";
    default: return "unknown";
    }
}

const char *event_journal_name(uint8_t event_id) { return event_id_name(event_id); }

void event_journal_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK)
    {
        size_t len = 0;
        if (nvs_get_blob(handle, NVS_KEY, NULL, &len) == ESP_OK &&
            len > 0 && len % sizeof(event_journal_entry_t) == 0)
        {
            // The blob holds the same count as RAM would, oldest first.
            event_journal_entry_t tmp[EVENT_JOURNAL_CAPACITY];
            const size_t n = len / sizeof(event_journal_entry_t);
            if (nvs_get_blob(handle, NVS_KEY, tmp, &len) == ESP_OK)
            {
                s_count = n > EVENT_JOURNAL_CAPACITY ? EVENT_JOURNAL_CAPACITY : n;
                // Reverse into newest-first.
                for (size_t i = 0; i < s_count; i++)
                    s_ring[i] = tmp[s_count - 1 - i];
                ESP_LOGI(TAG, "Loaded %u journal entries from NVS", (unsigned)s_count);
            }
        }
        nvs_close(handle);
    }
}

void event_journal_add(uint8_t event_id, const char *detail)
{
    if (!s_mutex) return;

    // Shift the ring down by one; drop the oldest.
    if (s_count == EVENT_JOURNAL_CAPACITY)
        memmove(&s_ring[1], &s_ring[0], sizeof(s_ring) - sizeof(s_ring[0]));
    else if (s_count > 0)
        memmove(&s_ring[1], &s_ring[0], s_count * sizeof(s_ring[0]));
    if (s_count < EVENT_JOURNAL_CAPACITY) s_count++;

    event_journal_entry_t *e = &s_ring[0];
    const time_t now = time(NULL);
    e->epoch = now > 1600000000 ? (int64_t)now : 0; // 0 while unsynced
    e->event_id = event_id;
    if (detail)
    {
        strncpy(e->detail, detail, sizeof(e->detail) - 1);
        e->detail[sizeof(e->detail) - 1] = '\0';
    }
    else
    {
        e->detail[0] = '\0';
    }
    s_dirty = true;
}

void event_journal_flush(void)
{
    if (!s_mutex || !s_dirty) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!s_dirty) { xSemaphoreGive(s_mutex); return; }

    event_journal_entry_t tmp[EVENT_JOURNAL_CAPACITY];
    const size_t n = s_count;
    // Copy newest-first into oldest-first blob order.
    for (size_t i = 0; i < n; i++)
        tmp[i] = s_ring[n - 1 - i];

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        err = nvs_set_blob(handle, NVS_KEY, tmp, n * sizeof(event_journal_entry_t));
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err == ESP_OK) s_dirty = false;
    else ESP_LOGW(TAG, "Journal persist failed: %s", esp_err_to_name(err));
    xSemaphoreGive(s_mutex);
}

size_t event_journal_count(void) { return s_count; }

const event_journal_entry_t *event_journal_entry(size_t index)
{
    return index < s_count ? &s_ring[index] : NULL;
}
