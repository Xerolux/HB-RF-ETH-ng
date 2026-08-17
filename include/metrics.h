/*
 *  metrics.h is part of the HB-RF-ETH firmware v2.0
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
#include <cstddef>

// Central registry of process-wide metric counters. Each 64-bit modular
// value is represented by native lock-free 32-bit low/high atomics, so it can
// be incremented from any task or ISR-adjacent context without the ESP32's
// non-lock-free 64-bit atomic helper or an interrupt-disabling spinlock.
//
// They are exposed via the Prometheus `/metrics` endpoint and (optionally)
// surfaced through MQTT topics / notifications.

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle returned by metrics_register_counter. Safe to cache in a
// static after metrics_init() has run.
typedef struct metrics_counter *metrics_counter_t;

// Initialise the registry. Safe to call once from app_main; idempotent.
void metrics_init(void);

// Register (or look up) a named counter. Returns a stable handle. The help
// text is shown in the Prometheus `# HELP` line. Not thread-safe with
// respect to concurrent registrations of the *same* name — register all
// counters at boot from a single task.
metrics_counter_t metrics_register_counter(const char *name, const char *help);

// Increment a counter by `delta` (default 1). Safe from any task.
void metrics_inc(metrics_counter_t counter, uint32_t delta);
void metrics_inc_one(metrics_counter_t counter);

// Read a counter's current value from task context. Native 32-bit writer-count
// and generation words make the high/low snapshot coherent even with
// overlapping writers and at the rollover boundary. A reader yields for one
// scheduler tick only while an update is in flight or its snapshot changed.
uint64_t metrics_get(metrics_counter_t counter);

// --- High-water gauges -----------------------------------------------------
//
// A counter cannot express "the worst latency seen so far", which is the one
// number that matters when a user reports that switching commands arrived
// 20-30 seconds late: an average hides it and a rate says nothing at all.
// A gauge holds a single 32-bit high-water value, updated with a lock-free
// compare-exchange so the receive path can record into it without a mutex.

typedef struct metrics_gauge *metrics_gauge_t;

// Register (or look up) a named high-water gauge. Same boot-time
// registration rules as metrics_register_counter.
metrics_gauge_t metrics_register_gauge(const char *name, const char *help);

// Raise the gauge to `value` if it is currently lower. Safe from any task.
void metrics_gauge_record_max(metrics_gauge_t gauge, uint32_t value);

// Read the current high-water value.
uint32_t metrics_gauge_get(metrics_gauge_t gauge);

// Clear the high-water mark back to 0, so an operator can watch a fresh
// window after changing something rather than staring at a mark set days ago.
void metrics_gauge_reset(metrics_gauge_t gauge);

// Render every registered counter and gauge in Prometheus text exposition
// format and append it to `out` (always NUL-terminated). `offset` is the
// current write position, `cap` the total buffer capacity. Returns the new
// length.
size_t metrics_render_prometheus(char *out, size_t cap, size_t offset);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// Convenience RAII wrapper around a metrics_counter_t looked up at boot.
// Usage:
//   static MetricsCounter udp_rx("hbrfeth_udp_frames_total", "...");
//   udp_rx.inc();
class MetricsCounter {
public:
    MetricsCounter(const char *name, const char *help)
        : _handle(metrics_register_counter(name, help)) {}

    void inc() { metrics_inc_one(_handle); }
    void inc(uint32_t delta) { metrics_inc(_handle, delta); }
    uint64_t get() const { return metrics_get(_handle); }

private:
    metrics_counter_t _handle;
};
#endif
