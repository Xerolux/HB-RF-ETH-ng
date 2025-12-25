#include "log_manager.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

static const char* TAG = "LogManager";

// Log message structure for queue
struct LogMessage {
    char buffer[256];
    size_t length;
};

LogManager::LogManager() :
    _started(false),
    _basePath("/spiffs"),
    _currentLogFile("/spiffs/log.txt"),
    _prevLogFile("/spiffs/log.txt.1"),
    _maxLogSize(64 * 1024), // 64KB
    _logQueue(nullptr),
    _fileMutex(nullptr),
    _logTaskHandle(nullptr)
{
}

LogManager::~LogManager() {
    if (_logQueue) {
        vQueueDelete(_logQueue);
    }
    if (_fileMutex) {
        vSemaphoreDelete(_fileMutex);
    }
}

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

int LogManager::vprintf_handler(const char* fmt, va_list ap) {
    LogManager& mgr = LogManager::instance();
    if (!mgr._started || !mgr._logQueue) {
        return vprintf(fmt, ap);
    }

    // Format message into buffer
    LogMessage msg;
    msg.length = vsnprintf(msg.buffer, sizeof(msg.buffer), fmt, ap);

    // Clamp to buffer size
    if (msg.length >= sizeof(msg.buffer)) {
        msg.length = sizeof(msg.buffer) - 1;
    }

    // Send to queue (non-blocking to avoid delays)
    xQueueSend(mgr._logQueue, &msg, 0);

    // Also print to console (original behavior)
    return vprintf(fmt, ap);
}

void LogManager::start() {
    if (_started) return;

    // Initialize SPIFFS
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

    // Create mutex for file access
    _fileMutex = xSemaphoreCreateMutex();
    if (!_fileMutex) {
        ESP_LOGE(TAG, "Failed to create file mutex");
        return;
    }

    // Create log queue (32 messages deep)
    _logQueue = xQueueCreate(32, sizeof(LogMessage));
    if (!_logQueue) {
        ESP_LOGE(TAG, "Failed to create log queue");
        vSemaphoreDelete(_fileMutex);
        _fileMutex = nullptr;
        return;
    }

    // Create log processing task
    BaseType_t taskCreated = xTaskCreate(
        log_task,
        "log_task",
        4096,  // 4KB stack
        this,
        5,     // Priority
        &_logTaskHandle
    );

    if (taskCreated != pdPASS) {
        ESP_LOGE(TAG, "Failed to create log task");
        vQueueDelete(_logQueue);
        vSemaphoreDelete(_fileMutex);
        _logQueue = nullptr;
        _fileMutex = nullptr;
        return;
    }

    // Register vprintf handler
    esp_log_set_vprintf(vprintf_handler);

    _started = true;
    ESP_LOGI(TAG, "LogManager started, logging to %s", _currentLogFile);
}

void LogManager::log_task(void* pvParameters) {
    LogManager* mgr = static_cast<LogManager*>(pvParameters);
    mgr->processLogQueue();
}

void LogManager::processLogQueue() {
    LogMessage msg;

    while (true) {
        // Wait for log messages (block indefinitely)
        if (xQueueReceive(_logQueue, &msg, portMAX_DELAY) == pdTRUE) {
            // Take mutex before file operations
            if (xSemaphoreTake(_fileMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Check if rotation needed
                rotateLogIfNeeded();

                // Write to file
                FILE* f = fopen(_currentLogFile, "a");
                if (f != NULL) {
                    fwrite(msg.buffer, 1, msg.length, f);
                    fclose(f);
                }

                xSemaphoreGive(_fileMutex);
            }
        }
    }
}

void LogManager::rotateLogIfNeeded() {
    struct stat st;
    if (stat(_currentLogFile, &st) == 0) {
        if (st.st_size >= _maxLogSize) {
            unlink(_prevLogFile);
            rename(_currentLogFile, _prevLogFile);
        }
    }
}

std::string LogManager::getLogContent() {
    if (!_started) return "LogManager not started";

    std::string content;

    // Take mutex to ensure thread-safe file access
    if (xSemaphoreTake(_fileMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return "Error: Could not acquire file lock";
    }

    // Read previous log first
    FILE* f = fopen(_prevLogFile, "r");
    if (f != NULL) {
        char buf[512];
        while (fgets(buf, sizeof(buf), f)) {
            content += buf;
        }
        fclose(f);
        if (!content.empty()) {
            content += "\n--- ROTATED ---\n";
        }
    }

    // Read current log
    f = fopen(_currentLogFile, "r");
    if (f != NULL) {
        char buf[512];
        while (fgets(buf, sizeof(buf), f)) {
            content += buf;
        }
        fclose(f);
    }

    xSemaphoreGive(_fileMutex);

    if (content.empty()) {
        return "No log data available";
    }

    return content;
}

void LogManager::clearLog() {
    if (!_started) return;

    // Take mutex to ensure thread-safe file access
    if (xSemaphoreTake(_fileMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Could not acquire file lock for clearing logs");
        return;
    }

    unlink(_prevLogFile);
    unlink(_currentLogFile);

    // Create empty current log
    FILE* f = fopen(_currentLogFile, "w");
    if (f) {
        fclose(f);
    }

    xSemaphoreGive(_fileMutex);

    ESP_LOGI(TAG, "Logs cleared");
}
