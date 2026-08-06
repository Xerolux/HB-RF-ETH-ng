/*
 *  log_manager.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "log_manager.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "nvs_storage_lock.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <cstdlib>

static const char *TAG = "LogManager";

LogManager::LogManager()
    : _mutex(xSemaphoreCreateMutexStatic(&_mutex_storage)) {
}

// Singleton instance
LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

// Custom vprintf handler to capture logs
// Note: This must NOT be static because it's a friend function declared in the header with extern linkage
int log_vprintf(const char *fmt, va_list args) {
    LogManager &manager = LogManager::instance();

    // Fast path: when the ring buffer is not active AND no subscribers are
    // registered, skip all the va_copy / vsnprintf / malloc overhead and just
    // forward to the original UART sink. This is the common case at runtime
    // (logging is opt-in) and avoids the ~50 % CPU regression seen on devices
    // with chatty subsystems (raw-uart bridge, network events) where every
    // log line paid the full formatting cost for nothing.
    if (!manager._capture_active.load(std::memory_order_acquire) &&
        manager._subscriber_count_fast.load(std::memory_order_acquire) == 0) {
        LogManager::vprintf_fn_t sink =
            manager._orig_vprintf.load(std::memory_order_acquire);
        return sink ? sink(fmt, args) : vprintf(fmt, args);
    }

    // Capture path. We must format once for the ring buffer / subscribers,
    // and the UART sink needs the original (fmt, va_list). To avoid the
    // expensive double-formatting of the previous implementation (a
    // vsnprintf(NULL,0,...) length probe + a malloc fallback for oversized
    // lines, both running on EVERY log line while capture was merely
    // enabled), we format into a fixed stack buffer exactly once and feed
    // that to write(). Truncation beyond 256 bytes is acceptable: the ring
    // buffer and the WebSocket / syslog forwarders are diagnostic-only and
    // already cap their payloads (log_stream: 256, syslog: 384). The UART
    // copy below still receives the full, untruncated line.
    va_list args_for_uart;
    va_copy(args_for_uart, args);

    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len > 0) {
        size_t capped = (size_t)len < sizeof(buf) ? (size_t)len : sizeof(buf) - 1;
        manager.write(buf, capped);
    }

    // Forward to the previous ESP-IDF log sink using the copy.
    LogManager::vprintf_fn_t sink =
        manager._orig_vprintf.load(std::memory_order_acquire);
    int ret = sink ? sink(fmt, args_for_uart) : vprintf(fmt, args_for_uart);
    va_end(args_for_uart);
    return ret;
}

void LogManager::begin(size_t size) {
    instance()._begin(size);
}

void LogManager::stop() {
    instance()._stop();
}

void LogManager::clear() {
    instance()._clear();
}

// Install the capture hook permanently at boot. Called from main.cpp before
// any subsystem that might want to subscribe (syslog, log_stream) starts.
// Idempotent: safe to call repeatedly.
void LogManager::init() {
    LogManager &m = instance();
    if (!m._mutex) return;
    if (m._hook_installed.load(std::memory_order_acquire)) return;
    xSemaphoreTake(m._mutex, portMAX_DELAY);
    if (!m._hook_installed.load(std::memory_order_relaxed)) {
        vprintf_fn_t previous = esp_log_set_vprintf(log_vprintf);
        m._orig_vprintf.store(previous, std::memory_order_release);
        m._hook_installed.store(true, std::memory_order_release);
    }
    xSemaphoreGive(m._mutex);
}

void LogManager::addSubscriber(log_line_subscriber_t sub) {
    if (!sub) return;
    LogManager &m = instance();
    // Ensure the capture hook is installed so log_vprintf actually runs.
    init();
    if (!m._mutex) return;
    xSemaphoreTake(m._mutex, portMAX_DELAY);
    bool found = false;
    for (int i = 0; i < m._subscriber_count; i++) {
        if (m._subscribers[i] == sub) { found = true; break; }
    }
    if (!found && m._subscriber_count < LOG_MAX_SUBSCRIBERS) {
        m._subscribers[m._subscriber_count++] = sub;
        m._subscriber_count_fast.store(
            static_cast<uint32_t>(m._subscriber_count),
            std::memory_order_release);
    }
    xSemaphoreGive(m._mutex);
}

void LogManager::removeSubscriber(log_line_subscriber_t sub) {
    if (!sub) return;
    LogManager &m = instance();
    if (!m._mutex) return;
    xSemaphoreTake(m._mutex, portMAX_DELAY);
    for (int i = 0; i < m._subscriber_count; i++) {
        if (m._subscribers[i] == sub) {
            // Shift-down compact; order is not significant.
            for (int j = i + 1; j < m._subscriber_count; j++) {
                m._subscribers[j - 1] = m._subscribers[j];
            }
            m._subscriber_count--;
            m._subscribers[m._subscriber_count] = nullptr;
            m._subscriber_count_fast.store(
                static_cast<uint32_t>(m._subscriber_count),
                std::memory_order_release);
            break;
        }
    }
    xSemaphoreGive(m._mutex);
}

int LogManager::subscriberCount() const {
    return static_cast<int>(
        _subscriber_count_fast.load(std::memory_order_acquire));
}

void LogManager::_begin(size_t size) {
    init();
    if (!_mutex) {
        ESP_LOGE(TAG, "Failed to create log buffer mutex");
        return;
    }

    bool enabled = false;
    xSemaphoreTake(_mutex, portMAX_DELAY);

    if (log_buffer) {
        free(log_buffer);
        log_buffer = nullptr;
    }
    ring_start_offset = total_written;

    // Try the requested size first, then fall back to progressively smaller
    // buffers. The ESP32-WROOM-32 has no PSRAM and only ~250 KB internal
    // heap; a single TLS handshake (for example MQTT) can
    // drop free heap by 30-50 KB, so an 8 KB contiguous allocation can fail
    // even though a 4 KB or 2 KB one still fits. A smaller log is strictly
    // better than no log — and the user's "not enough memory" error goes
    // away because begin() now succeeds with whatever fits.
    static const size_t MIN_LOG_BUFFER = 2048;
    size_t want = size;
    if (want < MIN_LOG_BUFFER) want = MIN_LOG_BUFFER;
    while (want >= MIN_LOG_BUFFER) {
        log_buffer = (char *)malloc(want);
        if (log_buffer) break;
        want >>= 1;
    }
    log_buffer_size = log_buffer ? want : 0;

    if (log_buffer) {
        // Zero out for cleanliness, though not strictly required for ring buffer
        memset(log_buffer, 0, log_buffer_size);
        enabled = true;
    }
    const size_t enabled_size = log_buffer_size;
    _capture_active.store(enabled, std::memory_order_release);

    xSemaphoreGive(_mutex);

    if (enabled) {
        if (enabled_size == size) {
            ESP_LOGI(TAG, "Log buffering enabled (%d bytes)", (int)enabled_size);
        } else {
            ESP_LOGW(TAG, "Log buffering enabled with reduced buffer (%d bytes; %d requested) — free heap was low", (int)enabled_size, (int)size);
        }
    } else {
        ESP_LOGE(TAG, "Failed to allocate log buffer (even %d bytes unavailable) — heap exhausted", (int)MIN_LOG_BUFFER);
    }
}

void LogManager::_stop() {
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);

    // Only free the ring buffer. The capture hook stays installed so any
    // registered subscribers (syslog, log_stream) keep receiving lines.
    if (log_buffer) {
        free(log_buffer);
        log_buffer = nullptr;
        log_buffer_size = 0;
    }
    ring_start_offset = total_written;
    _capture_active.store(false, std::memory_order_release);

    xSemaphoreGive(_mutex);
    // Logged through the still-installed hook (UART passthrough).
    ESP_LOGI(TAG, "log buffering disabled (buffer freed)");
}

bool LogManager::isEnabled() const {
    return _capture_active.load(std::memory_order_acquire);
}

void LogManager::_clear() {
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    ring_start_offset = total_written;
    if (log_buffer) memset(log_buffer, 0, log_buffer_size);
    xSemaphoreGive(_mutex);
}

void LogManager::write(const char* data, size_t len) {
    if (len == 0 || !_mutex) return;

    const char *subscriber_data = data;
    const size_t subscriber_len = len;

    // Update the ring and snapshot subscribers under one lock so every
    // subscriber receives the exact absolute offset for this line. The
    // callbacks themselves still run unlocked and must remain non-blocking.
    log_line_subscriber_t snap[LOG_MAX_SUBSCRIBERS];
    int snap_count = 0;
    uint64_t end_offset = 0;

    // Use timeout to avoid blocking logging if something is stuck
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snap_count = _subscriber_count;
        for (int i = 0; i < snap_count; i++) snap[i] = _subscribers[i];

        // Advance the absolute stream even when the optional ring is disabled.
        // Live subscribers need a monotonic checkpoint independent of ring
        // allocation, clear and stop/start cycles.
        total_written += len;
        end_offset = total_written;

        if (log_buffer && log_buffer_size > 0) {
            // Only the tail of an oversized log entry can fit in the ring.
            if (len > log_buffer_size) {
                size_t skipped = len - log_buffer_size;
                data += skipped;
                len = log_buffer_size;
            }

            const uint64_t write_start = total_written - len;
            size_t current_idx = write_start % log_buffer_size;
            size_t space_at_end = log_buffer_size - current_idx;

            if (len <= space_at_end) {
                memcpy(log_buffer + current_idx, data, len);
            } else {
                memcpy(log_buffer + current_idx, data, space_at_end);
                memcpy(log_buffer, data + space_at_end, len - space_at_end);
            }
        }
        xSemaphoreGive(_mutex);
    }

    for (int i = 0; i < snap_count; i++) {
        snap[i](subscriber_data, subscriber_len, end_offset);
    }
}

std::string LogManager::getLogContent(uint64_t offset) {
    return getLogSnapshot(offset, nullptr);
}

std::string LogManager::getLogSnapshot(uint64_t offset, uint64_t *snapshot_total) {
    if (snapshot_total) *snapshot_total = 0;
    std::string result;

    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            const uint64_t local_total = total_written;
            if (snapshot_total) *snapshot_total = local_total;

            // The atomic capture flag is only a lock-free hint. Recheck the
            // actual ring state under the mutex before dereferencing it.
            if (!log_buffer || log_buffer_size == 0) {
                xSemaphoreGive(_mutex);
                return "";
            }

            // If client asks for future data (shouldn't happen), return empty
            if (offset >= local_total) {
                xSemaphoreGive(_mutex);
                return "";
            }

            uint64_t oldest = ring_start_offset;
            if (local_total - ring_start_offset > log_buffer_size) {
                oldest = local_total - log_buffer_size;
            }
            if (offset < oldest) offset = oldest;
            if (offset >= local_total) {
                xSemaphoreGive(_mutex);
                return "";
            }
            uint64_t wanted_len = local_total - offset;

            // FIX: Check heap before allocating to prevent OOM crash.
            // std::string::resize may abort on ESP-IDF if allocation fails.
            // Use the largest contiguous free block (not total free heap) and
            // keep a safety margin for the HTTP server / TLS while streaming.
            size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
            size_t max_alloc = (largest_block > 1536) ? (largest_block - 1536) : 0;
            if (wanted_len > max_alloc) {
                wanted_len = max_alloc;
                if (wanted_len > log_buffer_size) wanted_len = log_buffer_size;
                if (wanted_len == 0) {
                    xSemaphoreGive(_mutex);
                    return "";
                }
                offset = local_total - wanted_len;
            }

            size_t data_len = static_cast<size_t>(wanted_len);

            // Pre-allocate to avoid reallocations
            result.resize(data_len);

            size_t start_idx = offset % log_buffer_size;
            size_t space_at_end = log_buffer_size - start_idx;

            if (data_len <= space_at_end) {
                memcpy(&result[0], log_buffer + start_idx, data_len);
            } else {
                memcpy(&result[0], log_buffer + start_idx, space_at_end);
                memcpy(&result[space_at_end], log_buffer, data_len - space_at_end);
            }

            xSemaphoreGive(_mutex);
        }
    }

    return result;
}

size_t LogManager::readChunk(uint64_t *absolute_offset, char *destination,
                             size_t maximum_length) {
    if (!absolute_offset || !destination || maximum_length == 0 ||
        !_mutex) {
        return 0;
    }

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }
    if (!log_buffer || log_buffer_size == 0) {
        xSemaphoreGive(_mutex);
        return 0;
    }

    const uint64_t local_total = total_written;
    uint64_t requested = *absolute_offset;
    if (requested > local_total) requested = local_total;

    uint64_t oldest = ring_start_offset;
    if (local_total - ring_start_offset > log_buffer_size) {
        oldest = local_total - log_buffer_size;
    }
    if (requested < oldest) requested = oldest;

    const uint64_t available64 = local_total - requested;
    const size_t available = available64 > SIZE_MAX
        ? SIZE_MAX
        : static_cast<size_t>(available64);
    const size_t count = available < maximum_length
        ? available
        : maximum_length;

    if (count > 0) {
        const size_t start = static_cast<size_t>(requested % log_buffer_size);
        const size_t first = (count < log_buffer_size - start)
            ? count
            : log_buffer_size - start;
        memcpy(destination, log_buffer + start, first);
        if (count > first) {
            memcpy(destination + first, log_buffer, count - first);
        }
    }

    *absolute_offset = requested + count;
    xSemaphoreGive(_mutex);
    return count;
}

uint64_t LogManager::getTotalWritten() const {
    uint64_t result = 0;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        result = total_written;
        xSemaphoreGive(_mutex);
    }
    return result;
}

size_t LogManager::getBufferSize() const {
    size_t result = 0;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        result = log_buffer_size;
        xSemaphoreGive(_mutex);
    }
    return result;
}

size_t LogManager::getBufferedBytes() const {
    size_t result = 0;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (log_buffer && log_buffer_size > 0) {
            const uint64_t available = total_written - ring_start_offset;
            result = available < log_buffer_size
                ? static_cast<size_t>(available)
                : log_buffer_size;
        }
        xSemaphoreGive(_mutex);
    }
    return result;
}

// NVS keys for the crash-tail snapshot. The namespace is shared with
// reset_info so a single erase on read cleans the whole post-mortem state.
static const char *CRASH_TAIL_NVS_NS  = "reset_info";
static const char *CRASH_TAIL_NVS_KEY = "clog";   // 4 chars, within NVS limit

bool LogManager::saveCrashTailNvs(const char *tag) {
    // Pull a tail of the ring buffer. getLogContent() already caps the
    // returned size by the largest free block, so this is safe to call from
    // the low-heap path that triggered the watchdog.
    std::string tail = instance().getLogContent(0);
    if (tail.empty()) {
        // No ring buffer active — nothing to persist, but not an error.
        return false;
    }

    // Trim to the last CRASH_TAIL_MAX bytes (the most recent lines), because
    // that is what reveals *why* the device is restarting.
    if (tail.size() > CRASH_TAIL_MAX) {
        tail = tail.substr(tail.size() - CRASH_TAIL_MAX);
    }

    // Prefix with a short tag + newline so the WebUI can show context.
    // strncpy avoids -Wformat-truncation (which snprintf would trigger
    // because tag is an unknown-length parameter) while still bounding the
    // copy. Manual NUL termination covers the truncation case.
    const char *tag_src = tag ? tag : "crash";
    char tag_short[24];
    strncpy(tag_short, tag_src, sizeof(tag_short) - 1);
    tag_short[sizeof(tag_short) - 1] = '\0';
    char header[48];
    snprintf(header, sizeof(header), "[%s] ", tag_short);
    std::string blob;
    blob.reserve(strlen(header) + tail.size() + 1);
    blob.append(header);
    blob.append(tail);

    // This path runs immediately before a protective restart. Never let a
    // concurrent configuration transaction turn best-effort diagnostics into
    // an unbounded wait which prevents that restart.
    NvsStorageLock storage_lock(pdMS_TO_TICKS(20));
    if (!storage_lock) return false;

    nvs_handle_t h;
    if (nvs_open(CRASH_TAIL_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(h, CRASH_TAIL_NVS_KEY, blob.data(), blob.size());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "saveCrashTailNvs: failed (%s)", esp_err_to_name(err));
        return false;
    }
    return true;
}

std::string LogManager::loadCrashTailNvs() {
    nvs_handle_t h;
    if (nvs_open(CRASH_TAIL_NVS_NS, NVS_READONLY, &h) != ESP_OK) return "";

    std::string out;
    size_t len = 0;
    if (nvs_get_blob(h, CRASH_TAIL_NVS_KEY, NULL, &len) == ESP_OK && len > 0) {
        out.resize(len);
        if (nvs_get_blob(h, CRASH_TAIL_NVS_KEY, &out[0], &len) != ESP_OK) {
            out.clear();
        }
    }
    nvs_close(h);
    return out;
}
