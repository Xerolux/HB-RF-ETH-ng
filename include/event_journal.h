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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bounded diagnostic history: 24 full-detail entries in RAM (2688 bytes),
// with only the newest eight mirrored to the shared 16 KiB NVS partition.
#define EVENT_JOURNAL_CAPACITY         24
#define EVENT_JOURNAL_PERSIST_CAPACITY 8

// Journal-internal ids (>= 200) never map to a notification Event.
#define EVENT_JOURNAL_WITNESS_DEFERRED  200
#define EVENT_JOURNAL_SCHEDULED_RESTART 201

typedef struct {
    int64_t epoch;    // unix seconds at capture; 0 while the clock is unsynced
    uint8_t event_id; // Event enum or EVENT_JOURNAL_*
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

// Copy one complete entry under the ring lock; never expose mutable storage.
// Returns false when index is out of range. 0 = newest.
bool event_journal_copy(size_t index, event_journal_entry_t *entry);

// Stable, wire-format name for an event id ("ccu_connected", ...).
const char *event_journal_name(uint8_t event_id);

// Optional synchronous listener invoked after every appended entry (outside
// the ring lock, with a private copy of the entry). Registered by the MQTT
// subsystem so live diagnostics do not need to poll the journal. The
// callback runs on the emitting task and must never block; keep the
// processing bounded (single publish, no flash I/O).
typedef void (*event_journal_listener_t)(const event_journal_entry_t *entry);
void event_journal_set_listener(event_journal_listener_t listener);

#ifdef __cplusplus
}
#endif
