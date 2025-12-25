#pragma once

#include "esp_err.h"
#include <string>

class LogManager {
public:
    static LogManager& instance();

    void start();
    std::string getLogContent();
    void clearLog();

private:
    LogManager();
    ~LogManager();

    // Prevent copy/assignment
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    static int vprintf_handler(const char* fmt, va_list ap);
    void writeLog(const char* fmt, va_list ap);

    bool _started;
    const char* _basePath;
    const char* _currentLogFile;
    const char* _prevLogFile;
    const size_t _maxLogSize;
};
