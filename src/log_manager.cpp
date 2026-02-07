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
        int ret = vprintf(fmt, args_for_uart);
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
            int ret = vprintf(fmt, args_for_uart);
            va_end(args_for_uart);
            return ret;
        }
        heap_alloc = true;
    }

    // Format using original args (consumed after this)
    vsnprintf(buf, len + 1, fmt, args);

    // Write to ring buffer
    LogManager::instance().write(buf, len);

    if (heap_alloc) free(buf);

    // Forward to default UART handler using the copy
    int ret = vprintf(fmt, args_for_uart);
    va_end(args_for_uart);
    return ret;
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
