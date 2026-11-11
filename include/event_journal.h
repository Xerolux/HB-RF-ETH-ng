/*
 *  event_journal.h is part of the HB-RF-ETH-ng firmware
 *
 *  RAM ring of recent device events (CCU connects, link drops, crashes,
 *  deferred watchdog restarts, ...), mirrored to NVS so the history
 *  survives reboots. Backs GET /api/events.
 *
 *  This file is licensed under the Creative Commons
 *  Attribution-NonCommercial-ShareAlike 4.0 International License.
 *  http://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// RAM capacity. NVS mirror holds the same count; each entry is ~112 bytes,
// so the persisted blob stays under 4.5 KB.
#define EVENT_JOURNAL_CAPACITY 40

// Journal-internal ids (>= 200) never map to a notification Event.
#define EVENT_JOURNAL_WITNESS_DEFERRED 200
#define EVENT_JOURNAL_SCHEDULED_RESTART 201

typedef struct
{
    int64_t epoch;     // unix seconds at capture; 0 while the clock is unsynced
    uint8_t event_id;  // Event enum or EVENT_JOURNAL_*
    char detail[96];
} event_journal_entry_t;

// Load the persisted ring from NVS. Call once after nvs_flash_init().
void event_journal_init(void);

// Append an entry (newest first). RAM-only; event_journal_flush() persists.
// Safe from any task; not from ISRs.
void event_journal_add(uint8_t event_id, const char *detail);

// Persist the RAM ring to NVS when it changed since the last flush.
// Called from slow service contexts (events worker, monitoring grid) so the
// emit path never blocks on a flash write.
void event_journal_flush(void);

// Number of valid entries (newest first).
size_t event_journal_count(void);

// Entry by index, 0 = newest. NULL when index >= count.
const event_journal_entry_t *event_journal_entry(size_t index);

// Stable, wire-format name for an event id ("ccu_connected", ...).
const char *event_journal_name(uint8_t event_id);

#ifdef __cplusplus
}
#endif
