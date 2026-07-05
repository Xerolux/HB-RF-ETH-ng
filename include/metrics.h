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

// Central registry of process-wide metric counters. All counters are
// lock-free (std::atomic<uint64_t>) so they can be incremented from any
// task or ISR-adjacent context (UDP recv callback, UART queue task, MQTT
// event handler, OTA task) without extra synchronisation.
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
void metrics_inc(metrics_counter_t counter, uint64_t delta);
void metrics_inc_one(metrics_counter_t counter);

// Read a counter's current value.
uint64_t metrics_get(metrics_counter_t counter);

// Render every registered counter in Prometheus text exposition format and
// append it to `out` (always NUL-terminated). `offset` is the current write
// position, `cap` the total buffer capacity. Returns the new length.
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

    void inc(uint64_t delta = 1) { metrics_inc(_handle, delta); }
    uint64_t get() const { return metrics_get(_handle); }

private:
    metrics_counter_t _handle;
};
#endif
