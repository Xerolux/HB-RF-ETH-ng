/*
 *  crash_blackbox.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "crash_blackbox.h"
#include "esp_attr.h" // RTC_NOINIT_ATTR
// freertos/FreeRTOS.h must be included before anything that pulls in
// portmacro.h (esp_freertos_hooks.h does) — it defines the config* macros
// portmacro depends on.
#include "freertos/FreeRTOS.h"
#include "esp_cpu.h"
#include "esp_freertos_hooks.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include <cstdio>
#include <cstring>

#define CRASH_BLACKBOX_NVS_OP_MAGIC 0xA55AF00Du
#define CRASH_BLACKBOX_NET_OP_MAGIC 0xA55AF00Eu

// RTC_NOINIT_ATTR places this struct in the ".rtc_noinit" section of RTC slow
// memory. ESP-IDF does NOT initialize this section at boot, so its contents
// survive software resets (watchdog, panic, esp_restart) and deep sleep. Only a
// power-on / cold boot (RTC power domain lost) wipes it, which is exactly the
// reset type we do not need to diagnose. The magic field distinguishes a valid
// pre-crash sample from garbage after a cold boot.
static RTC_NOINIT_ATTR crash_blackbox_t s_blackbox;

void crash_blackbox_update(uint32_t free_heap, uint32_t largest_block,
                           uint32_t min_heap, uint32_t internal_free,
                           uint32_t uptime_s, uint32_t low_streak)
{
    s_blackbox.free_heap = free_heap;
    s_blackbox.largest_block = largest_block;
    s_blackbox.min_heap = min_heap;
    s_blackbox.internal_free = internal_free;
    s_blackbox.uptime_s = uptime_s;
    s_blackbox.low_streak = low_streak;
    if (s_blackbox.magic != CRASH_BLACKBOX_MAGIC) {
        // First sample of a fresh RTC cycle (cold boot or wiped slot).
        s_blackbox.sample_count = 0;
        s_blackbox.magic = CRASH_BLACKBOX_MAGIC;
    }
    s_blackbox.sample_count++;
}

const crash_blackbox_t *crash_blackbox_read(void)
{
    if (s_blackbox.magic != CRASH_BLACKBOX_MAGIC) {
        return NULL;
    }
    return &s_blackbox;
}

void crash_blackbox_clear(void)
{
    s_blackbox.magic = 0;
    s_blackbox.sample_count = 0;
}

static void nvs_op_stack_reset_if_stale(void)
{
    if (s_blackbox.nvs_op_magic != CRASH_BLACKBOX_NVS_OP_MAGIC) {
        s_blackbox.nvs_op_magic = CRASH_BLACKBOX_NVS_OP_MAGIC;
        s_blackbox.nvs_op_depth = 0;
    }
}

void crash_blackbox_nvs_op_begin(const char *tag)
{
    nvs_op_stack_reset_if_stale();

    if (s_blackbox.nvs_op_depth < CRASH_BLACKBOX_NVS_STACK_DEPTH) {
        char *dst = s_blackbox.nvs_op_stack[s_blackbox.nvs_op_depth];
        strncpy(dst, tag ? tag : "?", CRASH_BLACKBOX_TAG_LEN - 1);
        dst[CRASH_BLACKBOX_TAG_LEN - 1] = '\0';
    }
    // Depth still counts past the array capacity so a deeply-nested caller's
    // end() calls stay balanced with begin(); only the label is dropped.
    if (s_blackbox.nvs_op_depth < 255) {
        s_blackbox.nvs_op_depth++;
    }
}

void crash_blackbox_nvs_op_end(void)
{
    if (s_blackbox.nvs_op_magic == CRASH_BLACKBOX_NVS_OP_MAGIC &&
        s_blackbox.nvs_op_depth > 0) {
        s_blackbox.nvs_op_depth--;
    }
}

void crash_blackbox_net_op_begin(const char *tag)
{
    s_blackbox.net_op_magic = CRASH_BLACKBOX_NET_OP_MAGIC;
    strncpy(s_blackbox.net_op_tag, tag ? tag : "?", CRASH_BLACKBOX_TAG_LEN - 1);
    s_blackbox.net_op_tag[CRASH_BLACKBOX_TAG_LEN - 1] = '\0';
}

void crash_blackbox_net_op_end(void)
{
    s_blackbox.net_op_magic = 0;
    s_blackbox.net_op_tag[0] = '\0';
}

const char *crash_blackbox_describe_stuck_op(void)
{
    static char buf[CRASH_BLACKBOX_TAG_LEN * 2 + 8];

    const bool nvs_stuck = s_blackbox.nvs_op_magic == CRASH_BLACKBOX_NVS_OP_MAGIC &&
                            s_blackbox.nvs_op_depth > 0;
    const bool net_stuck = s_blackbox.net_op_magic == CRASH_BLACKBOX_NET_OP_MAGIC &&
                            s_blackbox.net_op_tag[0] != '\0';

    if (!nvs_stuck && !net_stuck) {
        return NULL;
    }

    // The outermost (index 0) NVS frame names the top-level operation the
    // caller started; that is more useful than the innermost helper it
    // happened to be nested in when the reset hit.
    if (nvs_stuck && net_stuck) {
        snprintf(buf, sizeof(buf), "nvs:%s net:%s",
                 s_blackbox.nvs_op_stack[0], s_blackbox.net_op_tag);
    } else if (nvs_stuck) {
        snprintf(buf, sizeof(buf), "nvs:%s", s_blackbox.nvs_op_stack[0]);
    } else {
        snprintf(buf, sizeof(buf), "net:%s", s_blackbox.net_op_tag);
    }
    return buf;
}

// --- Tick sentinel ---------------------------------------------------------
//
// Everything in this hook must stay in IRAM with no flash-resident callees:
// the tick also fires while a flash operation has the cache disabled, and
// that is precisely a window where an interrupt-watchdog hang could start.
// This mirrors the constraint on the interrupt watchdog's own tick hook:
// esp_cpu_get_core_id() is a forced inline reading the PRID register and
// esp_timer_get_time() is an alias of an ESP_TIMER_IRAM_ATTR function —
// the same pair int_wdt.c itself calls from this exact context. The store
// is one aligned 32-bit write to RTC slow memory per core per 10 ms tick.
static void IRAM_ATTR crash_blackbox_tick_hook(void)
{
    s_blackbox.last_tick_ms[(unsigned)esp_cpu_get_core_id() & 1] =
        (uint32_t)(esp_timer_get_time() / 1000);
}

void crash_blackbox_tick_sentinel_init(void)
{
    // Latch the pre-reset values FIRST — the very first tick after this
    // call would already overwrite the live slots and destroy the evidence
    // (that is exactly what the first Beta.4 revision got wrong).
    if (s_blackbox.tick_magic == CRASH_BLACKBOX_TICK_MAGIC) {
        s_blackbox.prev_tick_magic = CRASH_BLACKBOX_TICK_MAGIC;
        s_blackbox.prev_tick_ms[0] = s_blackbox.last_tick_ms[0];
        s_blackbox.prev_tick_ms[1] = s_blackbox.last_tick_ms[1];
    } else {
        s_blackbox.prev_tick_magic = 0;
    }

    s_blackbox.tick_magic = CRASH_BLACKBOX_TICK_MAGIC;
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    s_blackbox.last_tick_ms[0] = now_ms;
    s_blackbox.last_tick_ms[1] = now_ms;

    // Same registration mechanism the interrupt watchdog itself uses. A
    // failed registration leaves stale-but-armed timestamps; the boot-side
    // reader treats identical frozen values with equal deltas as "sentinel
    // data", which is still strictly better than nothing.
    esp_register_freertos_tick_hook_for_cpu(&crash_blackbox_tick_hook, 0);
    esp_register_freertos_tick_hook_for_cpu(&crash_blackbox_tick_hook, 1);
}
