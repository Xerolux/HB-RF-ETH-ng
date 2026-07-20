/*
 *  log_manager.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 */

#pragma once

#include <cstdint>
#include <string>
#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef void (*log_line_subscriber_t)(const char *line, size_t len, uint64_t end_offset);

class LogManager {
public:
    static LogManager& instance();

    static constexpr size_t DEFAULT_BUFFER_SIZE = 4096;

    static void init();
    static void begin(size_t size = DEFAULT_BUFFER_SIZE);
    static void stop();
    static void clear();

    bool isEnabled() const;

    static constexpr int LOG_MAX_SUBSCRIBERS = 4;
    void addSubscriber(log_line_subscriber_t sub);
    void removeSubscriber(log_line_subscriber_t sub);
    int subscriberCount() const;

    std::string getLogContent(uint64_t offset = 0);
    std::string getLogSnapshot(uint64_t offset, uint64_t *total_written);

    /**
     * Copy one stable snapshot chunk directly from the ring buffer.
     *
     * `absolute_offset` is both input and output. Lagging readers are clamped to
     * the oldest byte still present in the ring. No heap allocation is used,
     * which makes this the preferred path for HTTP downloads on the WROOM-32.
     */
    size_t readChunk(uint64_t *absolute_offset, char *destination,
                     size_t maximum_length);

    uint64_t getTotalWritten() const;

    static constexpr size_t CRASH_TAIL_MAX = 1024;
    bool saveCrashTailNvs(const char *tag);
    static std::string loadCrashTailNvs();

private:
    LogManager();

    void _begin(size_t size);
    void _stop();
    void _clear();
    void write(const char* data, size_t len);

    char *log_buffer = nullptr;
    size_t log_buffer_size = 0;
    uint64_t total_written = 0;
    mutable SemaphoreHandle_t _mutex = nullptr;

    log_line_subscriber_t _subscribers[LOG_MAX_SUBSCRIBERS] = {};
    int _subscriber_count = 0;
    bool _hook_installed = false;

    int (*_orig_vprintf)(const char *, va_list) = nullptr;

    friend int log_vprintf(const char *fmt, va_list args);
};
