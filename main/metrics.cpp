/*
 *  metrics.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "metrics.h"
#include <atomic>
#include <cstring>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "metrics";

// Bounded registry — counters are registered once at boot, so a small
// fixed table avoids dynamic allocation and is safe under the boot-time
// registration lock.
static constexpr int MAX_COUNTERS = 32;

struct metrics_counter_impl {
    const char *name;
    const char *help;
    std::atomic<uint64_t> value{0};
};

static metrics_counter_impl s_counters[MAX_COUNTERS];
static int s_counter_count = 0;
static SemaphoreHandle_t s_registry_mutex = NULL;

extern "C" void metrics_init(void)
{
    if (s_registry_mutex == NULL) {
        s_registry_mutex = xSemaphoreCreateMutex();
    }
}

extern "C" metrics_counter_t metrics_register_counter(const char *name, const char *help)
{
    if (s_registry_mutex == NULL) {
        metrics_init();
    }

    metrics_counter_t handle = NULL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    for (int i = 0; i < s_counter_count; i++) {
        if (strcmp(s_counters[i].name, name) == 0) {
            handle = &s_counters[i];
            break;
        }
    }
    if (handle == NULL && s_counter_count < MAX_COUNTERS) {
        s_counters[s_counter_count].name = name;
        s_counters[s_counter_count].help = help ? help : "";
        s_counters[s_counter_count].value.store(0);
        handle = &s_counters[s_counter_count];
        s_counter_count++;
    } else if (handle == NULL) {
        ESP_LOGE(TAG, "metric registry full (%d), cannot register %s", MAX_COUNTERS, name);
    }
    xSemaphoreGive(s_registry_mutex);
    return handle;
}

extern "C" void metrics_inc(metrics_counter_t counter, uint64_t delta)
{
    if (!counter) return;
    reinterpret_cast<metrics_counter_impl *>(counter)->value.fetch_add(delta, std::memory_order_relaxed);
}

extern "C" void metrics_inc_one(metrics_counter_t counter)
{
    metrics_inc(counter, 1);
}

extern "C" uint64_t metrics_get(metrics_counter_t counter)
{
    if (!counter) return 0;
    return reinterpret_cast<metrics_counter_impl *>(counter)->value.load(std::memory_order_relaxed);
}

extern "C" size_t metrics_render_prometheus(char *out, size_t cap, size_t offset)
{
    if (!out || cap == 0) return offset;
    if (offset >= cap) offset = cap - 1;
    out[offset] = '\0';

    // Take the mutex to get a consistent snapshot of the table layout; the
    // counter values themselves are atomic and may drift between reads.
    if (s_registry_mutex == NULL) return offset;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int n = s_counter_count;
    for (int i = 0; i < n; i++) {
        if (offset + 1 >= cap) break;
        const metrics_counter_impl &c = s_counters[i];
        uint64_t v = c.value.load(std::memory_order_relaxed);
        int written = snprintf(out + offset, cap - offset,
                               "# HELP %s %s\n# TYPE %s counter\n%s %llu\n",
                               c.name, c.help[0] ? c.help : "counter",
                               c.name, c.name, (unsigned long long)v);
        if (written < 0) break;
        size_t adv = (size_t)written;
        if (adv >= cap - offset) adv = cap - offset - 1;
        offset += adv;
    }
    xSemaphoreGive(s_registry_mutex);

    out[offset] = '\0';
    return offset;
}
