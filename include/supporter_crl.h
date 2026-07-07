/*
 *  supporter_crl.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Modified work Copyright 2025 Xerolux
 *
 *  Licensed under CC BY-NC-SA 4.0
 */

#pragma once

#include <stdbool.h>

// Revocation list (CRL) for supporter keys. Lets the maintainer invalidate a
// key before its embedded expiry — e.g. after a refund or when a key was
// shared publicly. The list is fetched from the repo's revoked_keys.json
// (which contains ONLY 128-bit SHA-256 fingerprints of the keys, never the
// keys themselves, so publishing it reveals nothing) and cached locally so
// revocation also works offline and right after boot.

// Loads the last cached revocation list from NVS. Call once at boot.
// Does NOT start the background refresh task — call
// supporter_crl_start_refresh_task() afterwards when (and only when) a
// supporter key is configured. Skipping the task on key-less devices saves
// 8 KB of task stack and avoids a TLS heap spike (30-50 KB) every refresh
// cycle for a list that nothing on the device consumes.
void supporter_crl_init(void);

// Starts the background refresh task (~6 h interval, first fetch after 60 s)
// if it is not already running. Idempotent — safe to call repeatedly, e.g.
// from the settings-save handler when a supporter key is added at runtime.
// No-op when the task is already running.
void supporter_crl_start_refresh_task(void);

// Re-fetches revoked_keys.json from GitHub and refreshes the in-RAM + NVS
// cache. Network call — serialised on g_net_fetch_mutex like every other
// outbound TLS fetch. Returns true on success. Safe to call manually too.
bool supporter_crl_refresh(void);

// True when the given key's fingerprint is on the cached revocation list.
// Pure RAM check, no network: SHA-256(normalised key)[:16 bytes] is compared
// against the cached 128-bit fingerprints. Fast — mbedTLS uses the ESP32's
// hardware SHA accelerator.
bool supporter_crl_is_revoked(const char *key);
