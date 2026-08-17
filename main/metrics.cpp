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
#include "freertos/task.h"

static const char *TAG = "metrics";

// Bounded registry — counters are registered once at boot, so a small
// fixed table avoids dynamic allocation and is safe under the boot-time
// registration lock.
static constexpr int MAX_COUNTERS = 32;

static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "ESP32 metrics require lock-free native 32-bit atomics");

struct metrics_counter {
    const char *name;
    const char *help;
    std::atomic<uint32_t> low{0};
    std::atomic<uint32_t> high{0};
    std::atomic<uint32_t> active_writers{0};
    std::atomic<uint32_t> generation{0};
};

static metrics_counter s_counters[MAX_COUNTERS];
static int s_counter_count = 0;
static SemaphoreHandle_t s_registry_mutex = NULL;

// High-water gauges. A single 32-bit atomic is enough: the value is a
// maximum, not an accumulator, so there is no 64-bit rollover to track and
// no writer-count/generation protocol to make the read coherent.
static constexpr int MAX_GAUGES = 8;

struct metrics_gauge {
    const char *name;
    const char *help;
    std::atomic<uint32_t> high_water{0};
};

static metrics_gauge s_gauges[MAX_GAUGES];
static int s_gauge_count = 0;

#ifdef METRICS_TEST_HOOKS
extern "C" void metrics_test_after_low_update(void);
#endif

static uint64_t metrics_snapshot(const metrics_counter *counter)
{
    // All four fields are native lock-free 32-bit atomics. active_writers
    // excludes an in-flight update and generation detects a complete update
    // between the two snapshots (including the 0 -> N -> 0 writer-count ABA).
    // Sequential consistency is intentional here: it gives these independent
    // words one total order without falling back to ESP32's emulated 64-bit
    // atomic helpers or an interrupt-disabling critical section.
    for (;;) {
        if (counter->active_writers.load(std::memory_order_seq_cst) != 0) {
            vTaskDelay(1);
            continue;
        }
        const uint32_t generation_before =
            counter->generation.load(std::memory_order_seq_cst);
        const uint32_t high =
            counter->high.load(std::memory_order_seq_cst);
        const uint32_t low =
            counter->low.load(std::memory_order_seq_cst);
        const uint32_t generation_after =
            counter->generation.load(std::memory_order_seq_cst);
        if (counter->active_writers.load(std::memory_order_seq_cst) == 0 &&
            generation_before == generation_after) {
            return (static_cast<uint64_t>(high) << 32) | low;
        }
        vTaskDelay(1);
    }
}

static void metrics_add(metrics_counter *counter, uint32_t delta)
{
    counter->active_writers.fetch_add(1, std::memory_order_seq_cst);
    const uint32_t old_low =
        counter->low.fetch_add(delta, std::memory_order_seq_cst);
#ifdef METRICS_TEST_HOOKS
    metrics_test_after_low_update();
#endif
    if (old_low + delta < old_low) {
        counter->high.fetch_add(1, std::memory_order_seq_cst);
    }
    counter->generation.fetch_add(1, std::memory_order_seq_cst);
    counter->active_writers.fetch_sub(1, std::memory_order_seq_cst);
}

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
        s_counters[s_counter_count].low.store(0, std::memory_order_relaxed);
        s_counters[s_counter_count].high.store(0, std::memory_order_relaxed);
        s_counters[s_counter_count].active_writers.store(
            0, std::memory_order_relaxed);
        s_counters[s_counter_count].generation.store(
            0, std::memory_order_relaxed);
        handle = &s_counters[s_counter_count];
        s_counter_count++;
    } else if (handle == NULL) {
        ESP_LOGE(TAG, "metric registry full (%d), cannot register %s", MAX_COUNTERS, name);
    }
    xSemaphoreGive(s_registry_mutex);
    return handle;
}

extern "C" void metrics_inc(metrics_counter_t counter, uint32_t delta)
{
    if (!counter) return;
    metrics_add(counter, delta);
}

extern "C" void metrics_inc_one(metrics_counter_t counter)
{
    if (!counter) return;
    metrics_add(counter, 1);
}

extern "C" uint64_t metrics_get(metrics_counter_t counter)
{
    if (!counter) return 0;
    return metrics_snapshot(counter);
}

extern "C" metrics_gauge_t metrics_register_gauge(const char *name, const char *help)
{
    if (s_registry_mutex == NULL) {
        metrics_init();
    }

    metrics_gauge_t handle = NULL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    for (int i = 0; i < s_gauge_count; i++) {
        if (strcmp(s_gauges[i].name, name) == 0) {
            handle = &s_gauges[i];
            break;
        }
    }
    if (handle == NULL && s_gauge_count < MAX_GAUGES) {
        s_gauges[s_gauge_count].name = name;
        s_gauges[s_gauge_count].help = help ? help : "";
        s_gauges[s_gauge_count].high_water.store(0, std::memory_order_relaxed);
        handle = &s_gauges[s_gauge_count];
        s_gauge_count++;
    } else if (handle == NULL) {
        ESP_LOGE(TAG, "gauge registry full (%d), cannot register %s", MAX_GAUGES, name);
    }
    xSemaphoreGive(s_registry_mutex);
    return handle;
}

extern "C" void metrics_gauge_record_max(metrics_gauge_t gauge, uint32_t value)
{
    if (!gauge) return;
    // Retry only while a concurrent writer raised the mark below `value`.
    // Once the observed maximum is already at least as large there is nothing
    // to do, so the common case costs a single relaxed load.
    uint32_t observed = gauge->high_water.load(std::memory_order_relaxed);
    while (value > observed) {
        if (gauge->high_water.compare_exchange_weak(observed, value, std::memory_order_release,
                                                    std::memory_order_relaxed)) {
            return;
        }
    }
}

extern "C" uint32_t metrics_gauge_get(metrics_gauge_t gauge)
{
    if (!gauge) return 0;
    return gauge->high_water.load(std::memory_order_acquire);
}

extern "C" void metrics_gauge_reset(metrics_gauge_t gauge)
{
    if (!gauge) return;
    gauge->high_water.store(0, std::memory_order_release);
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
        const metrics_counter &c = s_counters[i];
        uint64_t v = metrics_snapshot(&c);
        int written = snprintf(out + offset, cap - offset,
                               "# HELP %s %s\n# TYPE %s counter\n%s %llu\n",
                               c.name, c.help[0] ? c.help : "counter",
                               c.name, c.name, (unsigned long long)v);
        if (written < 0) break;
        size_t adv = (size_t)written;
        if (adv >= cap - offset) adv = cap - offset - 1;
        offset += adv;
    }
    int gauges = s_gauge_count;
    for (int i = 0; i < gauges; i++) {
        if (offset + 1 >= cap) break;
        const metrics_gauge &g = s_gauges[i];
        int written = snprintf(out + offset, cap - offset, "# HELP %s %s\n# TYPE %s gauge\n%s %u\n",
                               g.name, g.help[0] ? g.help : "gauge", g.name, g.name,
                               (unsigned)g.high_water.load(std::memory_order_acquire));
        if (written < 0) break;
        size_t adv = (size_t)written;
        if (adv >= cap - offset) adv = cap - offset - 1;
        offset += adv;
    }
    xSemaphoreGive(s_registry_mutex);

    out[offset] = '\0';
    return offset;
}
