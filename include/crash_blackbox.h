/*
 *  crash_blackbox.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#pragma once

#include <stdint.h>

// A tiny "flight recorder" for diagnosing sudden watchdog/panic reboots
// (issue #362: long-uptime "Stop working" -> Interrupt Watchdog reset).
//
// The reset reason says "Interrupt Watchdog" but the WebUI log ring buffer
// (in RAM) does not survive the reboot, and CONFIG_ESP_COREDUMP_ENABLE_TO_NONE
// is set (the 4 MB flash is full — no room for a coredump partition). So
// without a serial console attached at crash time there is NO backtrace and
// NO heap snapshot, making the crash effectively undiagnosable.
//
// This black box lives in RTC slow memory (RTC_NOINIT_ATTR), which is preserved
// across software/watchdog/panic resets and only wiped on power-loss. The
// heap watchdog task writes a fresh sample every cycle; after a crash the next
// boot reads the last sample and surfaces it in the reset-reason diagnostic
// field, so we can finally tell whether heap exhaustion preceded the reset.
//
// This is intentionally minimal (no backtrace) because it needs no extra flash
// partition and is safe to ship on the stable devices. Once a crash yields a
// low-heap sample we know the direction; a full coredump can be added later by
// shrinking another partition.

#define CRASH_BLACKBOX_MAGIC 0xB0BE1EAFu

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t magic;         // CRASH_BLACKBOX_MAGIC when the slot holds a valid sample
    uint32_t free_heap;     // heap_caps_get_free_size(MALLOC_CAP_DEFAULT) at sample time
    uint32_t largest_block; // heap_caps_get_largest_free_block(...) at sample time
    uint32_t min_heap;      // heap_caps_get_minimum_free_size(...) at sample time
    uint32_t internal_free; // free internal (non-DMA) heap, catches SPI-RAM-only drift
    uint32_t uptime_s;      // xTaskGetTickCount()/portTICK_PERIOD_MS/1000 at sample time
    uint32_t low_streak;    // heap_watchdog low-heap streak at sample time
    uint32_t sample_count;  // number of samples written since first boot of this RTC cycle
} crash_blackbox_t;

// Record a fresh sample. Called periodically (e.g. every 60 s) from the
// heap_watchdog task. Always writes — overwriting the previous sample so the
// slot always holds the most recent pre-crash snapshot.
void crash_blackbox_update(uint32_t free_heap, uint32_t largest_block,
                           uint32_t min_heap, uint32_t internal_free,
                           uint32_t uptime_s, uint32_t low_streak);

// Returns a pointer to the stored sample if the magic matches (i.e. the RTC
// slot contains data from before the current boot), or NULL otherwise. The
// pointer is valid for the lifetime of the process.
const crash_blackbox_t *crash_blackbox_read(void);

// Invalidate the slot so a subsequent normal reboot does not surface stale
// crash data. Called after the boot path has consumed and logged the sample.
void crash_blackbox_clear(void);

#ifdef __cplusplus
}
#endif
