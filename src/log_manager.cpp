#include "log_manager.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>

static const char *TAG = "LogManager";

LogManager::LogManager() {
    _mutex = xSemaphoreCreateMutex();
}

// Singleton instance
LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

// Custom vprintf handler to capture logs
// Note: This must NOT be static because it's a friend function declared in the header with extern linkage
int log_vprintf(const char *fmt, va_list args) {
    // Estimate length
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) return 0;

    // Use a small stack buffer for formatting to avoid malloc for typical log lines
    char stack_buf[256];
    char *buf = stack_buf;
    bool heap_alloc = false;

    if (len + 1 > sizeof(stack_buf)) {
        buf = (char*)malloc(len + 1);
        if (!buf) return 0; // OOM, skip logging to buffer
        heap_alloc = true;
    }

    vsnprintf(buf, len + 1, fmt, args);

    // Write to ring buffer
    LogManager::instance().write(buf, len);

    if (heap_alloc) free(buf);

    // Forward to default UART handler
    return vprintf(fmt, args);
}

void LogManager::begin(size_t size) {
    instance()._begin(size);
}

void LogManager::clear() {
    instance()._clear();
}

void LogManager::_begin(size_t size) {
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);

    if (log_buffer) free(log_buffer);
    log_buffer_size = size;
    log_buffer = (char *)malloc(log_buffer_size);
    total_written = 0;

    if (log_buffer) {
        // Zero out for cleanliness, though not strictly required for ring buffer
        memset(log_buffer, 0, log_buffer_size);
        esp_log_set_vprintf(log_vprintf);
        ESP_LOGI(TAG, "Log buffering enabled (%d bytes, RingBuffer)", size);
    } else {
        ESP_LOGE(TAG, "Failed to allocate log buffer");
    }

    xSemaphoreGive(_mutex);
}

void LogManager::_clear() {
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    total_written = 0;
    if (log_buffer) memset(log_buffer, 0, log_buffer_size);
    xSemaphoreGive(_mutex);
}

void LogManager::write(const char* data, size_t len) {
    if (!log_buffer || len == 0) return;

    if (_mutex) {
        // Use timeout to avoid blocking logging if something is stuck
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
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
                // Effectively, we give them the window [total - size, total]
                offset = local_total - log_buffer_size;
                data_len = log_buffer_size;
            }

            // Pre-allocate to avoid reallocations
            // Standard string::resize will abort on failure without exceptions enabled in ESP-IDF
            // which is fine as OOM is fatal anyway.
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
    return total_written;
}
