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

// Op-tag flight recorder: names the blocking operation in flight, if any,
// at the moment of an Interrupt-Watchdog/panic reset. This is what actually
// answers "what was the firmware doing" for issue #362 — the periodic heap
// sample above only bounds the picture (rules out slow heap exhaustion), it
// cannot name a specific call site.
//
// Both tracked domains already have a single project-wide serialization
// primitive (NvsStorageLock's recursive mutex; g_net_fetch_mutex, a plain
// binary semaphore), so at most one task ever owns a domain's tag at a time.
// That makes plain (unlocked) RTC writes from crash_blackbox_*_op_begin/end
// race-free by construction: only the current owner of the underlying mutex
// ever writes its domain's slot. The NVS domain uses a small stack because
// the recursive mutex lets its owning task nest calls; the net-fetch domain
// is a single slot because that semaphore never nests.
#define CRASH_BLACKBOX_TAG_LEN 24
#define CRASH_BLACKBOX_NVS_STACK_DEPTH 4

typedef struct {
    uint32_t magic;         // CRASH_BLACKBOX_MAGIC when the slot holds a valid sample
    uint32_t free_heap;     // heap_caps_get_free_size(MALLOC_CAP_DEFAULT) at sample time
    uint32_t largest_block; // heap_caps_get_largest_free_block(...) at sample time
    uint32_t min_heap;      // heap_caps_get_minimum_free_size(...) at sample time
    uint32_t internal_free; // free internal (non-DMA) heap, catches SPI-RAM-only drift
    uint32_t uptime_s;      // xTaskGetTickCount()/portTICK_PERIOD_MS/1000 at sample time
    uint32_t low_streak;    // heap_watchdog low-heap streak at sample time
    uint32_t sample_count;  // number of samples written since first boot of this RTC cycle

    uint32_t nvs_op_magic;  // set on first push since this RTC cycle started
    uint8_t  nvs_op_depth;  // number of NvsStorageLock instances currently held
    char     nvs_op_stack[CRASH_BLACKBOX_NVS_STACK_DEPTH][CRASH_BLACKBOX_TAG_LEN];

    uint32_t net_op_magic;  // set while g_net_fetch_mutex is held with a tag
    char     net_op_tag[CRASH_BLACKBOX_TAG_LEN];
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

// Push/pop a short tag naming the NVS operation an NvsStorageLock instance
// guards. Called from NvsStorageLock's constructor/destructor; never call
// directly. Safe (no-op) beyond CRASH_BLACKBOX_NVS_STACK_DEPTH nesting —
// depth is still tracked so end() stays balanced, just without a label for
// levels past the array.
void crash_blackbox_nvs_op_begin(const char *tag);
void crash_blackbox_nvs_op_end(void);

// Record/clear the tag naming the operation currently holding
// g_net_fetch_mutex. Call begin() right after a successful take and end()
// right before the matching give.
void crash_blackbox_net_op_begin(const char *tag);
void crash_blackbox_net_op_end(void);

// If the previous boot's RTC slot shows an operation that was pushed but
// never popped (i.e. still "in flight" when the reset happened), returns a
// short static description such as "nvs:settings.save" or
// "nvs:settings.save net:mqtt_tls" (both domains stuck at once). Returns
// NULL if nothing was stuck. Does not clear any state — callers still own
// crash_blackbox_clear().
const char *crash_blackbox_describe_stuck_op(void);

#ifdef __cplusplus
}
#endif
