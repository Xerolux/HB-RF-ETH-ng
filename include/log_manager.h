#pragma once

#include <cstdint>
#include <string>
#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class LogManager {
public:
    static LogManager& instance();

    // Static wrappers for initialization (called from main.cpp)
    static void begin(size_t size = 8192);
    static void clear();

    // Instance methods
    std::string getLogContent(size_t offset = 0);
    size_t getTotalWritten() const;

private:
    LogManager();

    // Internal implementation
    void _begin(size_t size);
    void _clear();
    void write(const char* data, size_t len);

    char *log_buffer = nullptr;
    size_t log_buffer_size = 0;
    uint64_t total_written = 0;
    mutable SemaphoreHandle_t _mutex = nullptr;

    // Allow the C-style callback to access private members
    friend int log_vprintf(const char *fmt, va_list args);
};
