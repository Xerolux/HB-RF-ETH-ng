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
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <cstdlib>

static const char *TAG = "LogManager";

LogManager::LogManager() : _mutex(xSemaphoreCreateMutex()) {
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

    // FIX: va_list can only be consumed once. We need two copies:
    // one for measuring length + formatting, one for forwarding to UART.
    va_list args_for_uart;
    va_copy(args_for_uart, args);

    // Estimate length using the original args
    va_list args_for_len;
    va_copy(args_for_len, args);
    int len = vsnprintf(NULL, 0, fmt, args_for_len);
    va_end(args_for_len);

    if (len < 0) {
        // Still forward to UART even if we can't capture
        int ret = manager._orig_vprintf ? manager._orig_vprintf(fmt, args_for_uart) : vprintf(fmt, args_for_uart);
        va_end(args_for_uart);
        return ret;
    }

    // Use a small stack buffer for formatting to avoid malloc for typical log lines
    char stack_buf[256];
    char *buf = stack_buf;
    bool heap_alloc = false;

    if ((size_t)(len + 1) > sizeof(stack_buf)) {
        buf = (char*)malloc(len + 1);
        if (!buf) {
            // OOM, skip ring buffer but still forward to UART
            int ret = manager._orig_vprintf ? manager._orig_vprintf(fmt, args_for_uart) : vprintf(fmt, args_for_uart);
            va_end(args_for_uart);
            return ret;
        }
        heap_alloc = true;
    }

    // Format using original args (consumed after this)
    vsnprintf(buf, len + 1, fmt, args);

    // Write to ring buffer
    manager.write(buf, len);

    if (heap_alloc) free(buf);

    // Forward to the previous ESP-IDF log sink using the copy.
    int ret = manager._orig_vprintf ? manager._orig_vprintf(fmt, args_for_uart) : vprintf(fmt, args_for_uart);
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
    if (!m._mutex) {
        m._mutex = xSemaphoreCreateMutex();
    }
    if (m._hook_installed) return;
    xSemaphoreTake(m._mutex, portMAX_DELAY);
    if (!m._hook_installed && m._orig_vprintf == nullptr) {
        m._orig_vprintf = esp_log_set_vprintf(log_vprintf);
        m._hook_installed = true;
    }
    xSemaphoreGive(m._mutex);
}

void LogManager::addSubscriber(log_line_subscriber_t sub) {
    if (!sub) return;
    LogManager &m = instance();
    // Ensure the capture hook is installed so log_vprintf actually runs.
    init();
    xSemaphoreTake(m._mutex, portMAX_DELAY);
    bool found = false;
    for (int i = 0; i < m._subscriber_count; i++) {
        if (m._subscribers[i] == sub) { found = true; break; }
    }
    if (!found && m._subscriber_count < LOG_MAX_SUBSCRIBERS) {
        m._subscribers[m._subscriber_count++] = sub;
    }
    xSemaphoreGive(m._mutex);
}

void LogManager::removeSubscriber(log_line_subscriber_t sub) {
    if (!sub) return;
    LogManager &m = instance();
    xSemaphoreTake(m._mutex, portMAX_DELAY);
    for (int i = 0; i < m._subscriber_count; i++) {
        if (m._subscribers[i] == sub) {
            // Shift-down compact; order is not significant.
            for (int j = i + 1; j < m._subscriber_count; j++) {
                m._subscribers[j - 1] = m._subscribers[j];
            }
            m._subscriber_count--;
            break;
        }
    }
    xSemaphoreGive(m._mutex);
}

int LogManager::subscriberCount() const {
    return _subscriber_count;
}

void LogManager::_begin(size_t size) {
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
    }
    if (!_mutex) {
        ESP_LOGE(TAG, "Failed to create log buffer mutex");
        return;
    }

    bool enabled = false;
    xSemaphoreTake(_mutex, portMAX_DELAY);

    // The capture hook is installed unconditionally at boot via init().
    // _begin() only manages the ring buffer from here on.
    if (!_hook_installed && _orig_vprintf == nullptr) {
        _orig_vprintf = esp_log_set_vprintf(log_vprintf);
        _hook_installed = true;
    }

    if (log_buffer) {
        free(log_buffer);
    }
    log_buffer_size = size;
    log_buffer = (char *)malloc(log_buffer_size);
    total_written = 0;

    if (log_buffer) {
        // Zero out for cleanliness, though not strictly required for ring buffer
        memset(log_buffer, 0, log_buffer_size);
        enabled = true;
    }

    xSemaphoreGive(_mutex);

    if (enabled) {
        ESP_LOGI(TAG, "Log buffering enabled (%d bytes, RingBuffer)", size);
    } else {
        ESP_LOGE(TAG, "Failed to allocate log buffer");
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
    total_written = 0;

    xSemaphoreGive(_mutex);
    // Logged through the still-installed hook (UART passthrough).
    ESP_LOGI(TAG, "log buffering disabled (buffer freed)");
}

bool LogManager::isEnabled() const {
    // Pointer reads are atomic on the ESP32 (32-bit aligned); a torn read at
    // worst returns a just-freed pointer, which is harmless because write() /
    // getLogContent() re-check under the mutex.
    return log_buffer != nullptr;
}

void LogManager::_clear() {
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    total_written = 0;
    if (log_buffer) memset(log_buffer, 0, log_buffer_size);
    xSemaphoreGive(_mutex);
}

void LogManager::write(const char* data, size_t len) {
    if (len == 0 || !_mutex) return;

    // Snapshot the subscriber list under the lock, then call them unlocked.
    // Subscribers are expected to be non-blocking (they enqueue internally),
    // but a snapshot avoids holding the lock during their execution so a
    // slow subscriber cannot stall the ring-buffer write below.
    log_line_subscriber_t snap[LOG_MAX_SUBSCRIBERS];
    int snap_count = 0;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        snap_count = _subscriber_count;
        for (int i = 0; i < snap_count; i++) snap[i] = _subscribers[i];
        xSemaphoreGive(_mutex);
    }
    for (int i = 0; i < snap_count; i++) {
        snap[i](data, len);
    }

    if (!log_buffer || log_buffer_size == 0) return;

    // Use timeout to avoid blocking logging if something is stuck
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Only the tail of an oversized log entry can fit in the ring.
        // Advance total_written for the skipped bytes so client offsets
        // still reflect the full stream of log data that passed through.
        if (len > log_buffer_size) {
            size_t skipped = len - log_buffer_size;
            data += skipped;
            len = log_buffer_size;
            total_written += skipped;
        }

        size_t current_idx = total_written % log_buffer_size;
        size_t space_at_end = log_buffer_size - current_idx;

        if (len <= space_at_end) {
            memcpy(log_buffer + current_idx, data, len);
        } else {
            // Wrap around
            memcpy(log_buffer + current_idx, data, space_at_end);
            memcpy(log_buffer, data + space_at_end, len - space_at_end);
        }
        total_written += len;
        xSemaphoreGive(_mutex);
    }
}

std::string LogManager::getLogContent(size_t offset) {
    if (!log_buffer) return "";

    std::string result;

    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint64_t local_total = total_written;

            // If client asks for future data (shouldn't happen), return empty
            if (offset >= local_total) {
                xSemaphoreGive(_mutex);
                return "";
            }

            size_t data_len = local_total - offset;

            // If the client is asking for data that has been overwritten (lagging behind)
            if (data_len > log_buffer_size) {
                // Return the entire valid buffer to catch them up (partially)
                offset = local_total - log_buffer_size;
                data_len = log_buffer_size;
            }

            // FIX: Check heap before allocating to prevent OOM crash
            // std::string::resize will abort on ESP-IDF if allocation fails
            uint32_t free_heap = esp_get_free_heap_size();
            if (data_len > free_heap / 2) {
                // Cap to half of free heap to leave room for other operations
                data_len = free_heap / 2;
                if (data_len > log_buffer_size) data_len = log_buffer_size;
                offset = local_total - data_len;
            }

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

size_t LogManager::getTotalWritten() const {
    size_t result = 0;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        result = total_written;
        xSemaphoreGive(_mutex);
    }
    return result;
}
