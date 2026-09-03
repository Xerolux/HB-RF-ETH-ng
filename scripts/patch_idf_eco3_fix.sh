#!/usr/bin/env bash
#
# Enable the ESP-IDF ECO3 cache-lock livelock workaround on non-PSRAM builds.
#
# ESP32 chip revisions v3.0/v3.1 have a silicon bug (Espressif errata
# WDT-3.15: "Chip May Have A Live Lock Under Certain Conditions That Will
# Cause Interrupt Watchdog Issue"). When three of the four IBUS/DBUS
# external-memory accesses simultaneously miss on the same cache set, both
# CPUs can livelock inside the memory access and stop executing
# instructions. The interrupt watchdog then resets the board with no panic
# output, no backtrace and no log line — exactly the field signature of
# issue #362 ("Stop working", random uptime, healthy heap).
#
# ESP-IDF ships a complete workaround (CONFIG_ESP32_ECO3_CACHE_LOCK_FIX:
# reconfigured IWDT stage-0 feed plus the livelock-aware high-level
# interrupt handler), but its Kconfig gate
#
#     depends on !ESP_SYSTEM_SINGLE_CORE_MODE && SPIRAM
#
# excludes boards without PSRAM. The HB-RF-ETH has no PSRAM, yet all four
# of its cache buses serve external flash, so the livelock condition
# applies (see the errata text — it does not require PSRAM, Espressif only
# considered PSRAM systems worth protecting). On chip revisions <= v2 the
# patched-in workaround is a no-op (soc_has_cache_lock_bug() == false).
#
# This script removes the SPIRAM term from that Kconfig dependency in a
# freshly cloned ESP-IDF checkout so the (default-y, promptless) option
# turns on for our build. It is idempotent and safe to re-run.
#
# Usage: scripts/patch_idf_eco3_fix.sh [idf_path]   (default: $IDF_PATH or ~/esp-idf)

set -euo pipefail

IDF_PATH_ARG="${1:-${IDF_PATH:-$HOME/esp-idf}}"
KFILE="$IDF_PATH_ARG/components/esp_system/port/soc/esp32/Kconfig.system"

if [ ! -f "$KFILE" ]; then
    echo "patch_idf_eco3_fix.sh: $KFILE not found — is this an ESP-IDF checkout?" >&2
    exit 1
fi

if grep -q "config ESP32_ECO3_CACHE_LOCK_FIX" "$KFILE" &&
   grep -A3 "config ESP32_ECO3_CACHE_LOCK_FIX" "$KFILE" | grep -q "depends on !ESP_SYSTEM_SINGLE_CORE_MODE && SPIRAM"; then
    # Keep the option's own line untouched; only drop the SPIRAM term.
    sed -i '/config ESP32_ECO3_CACHE_LOCK_FIX/,+3 s/depends on !ESP_SYSTEM_SINGLE_CORE_MODE && SPIRAM/depends on !ESP_SYSTEM_SINGLE_CORE_MODE/' "$KFILE"
    echo "patch_idf_eco3_fix.sh: removed SPIRAM gate from ESP32_ECO3_CACHE_LOCK_FIX in $KFILE"
elif grep -q "config ESP32_ECO3_CACHE_LOCK_FIX" "$KFILE"; then
    echo "patch_idf_eco3_fix.sh: gate already absent or already patched — nothing to do"
else
    echo "patch_idf_eco3_fix.sh: ESP32_ECO3_CACHE_LOCK_FIX not found in $KFILE — unexpected IDF layout, refusing to guess" >&2
    exit 1
fi
