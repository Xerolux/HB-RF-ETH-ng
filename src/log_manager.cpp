#include "log_manager.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

static const char* TAG = "LogManager";

LogManager::LogManager() :
    _started(false),
    _basePath("/spiffs"),
    _currentLogFile("/spiffs/log.txt"),
    _prevLogFile("/spiffs/log.txt.1"),
    _maxLogSize(64 * 1024) // 64KB
{
}

LogManager::~LogManager() {
}

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

int LogManager::vprintf_handler(const char* fmt, va_list ap) {
    LogManager::instance().writeLog(fmt, ap);
    return vprintf(fmt, ap);
}

void LogManager::start() {
    if (_started) return;

    esp_vfs_spiffs_conf_t conf = {
        .base_path = _basePath,
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    // Register vprintf handler
    esp_log_set_vprintf(vprintf_handler);

    _started = true;
    ESP_LOGI(TAG, "LogManager started, logging to %s", _currentLogFile);
}

void LogManager::writeLog(const char* fmt, va_list ap) {
    if (!_started) return;

    FILE* f = fopen(_currentLogFile, "a");
    if (f == NULL) {
        return;
    }

    // Check size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    if (size >= _maxLogSize) {
        fclose(f);
        // Rotate
        unlink(_prevLogFile);
        rename(_currentLogFile, _prevLogFile);
        f = fopen(_currentLogFile, "w");
        if (f == NULL) return;
    }

    vfprintf(f, fmt, ap);
    fclose(f);
}

std::string LogManager::getLogContent() {
    if (!_started) return "LogManager not started";

    std::string content;

    // Read previous log first
    FILE* f = fopen(_prevLogFile, "r");
    if (f != NULL) {
        char buf[256];
        while (fgets(buf, sizeof(buf), f)) {
            content += buf;
        }
        fclose(f);
        content += "\n--- ROTATED ---\n";
    }

    // Read current log
    f = fopen(_currentLogFile, "r");
    if (f != NULL) {
        char buf[256];
        while (fgets(buf, sizeof(buf), f)) {
            content += buf;
        }
        fclose(f);
    }

    return content;
}

void LogManager::clearLog() {
    if (!_started) return;

    unlink(_prevLogFile);
    unlink(_currentLogFile);

    // Create empty current log
    FILE* f = fopen(_currentLogFile, "w");
    if (f) fclose(f);
}
