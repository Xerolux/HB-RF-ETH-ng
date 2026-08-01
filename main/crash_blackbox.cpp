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
