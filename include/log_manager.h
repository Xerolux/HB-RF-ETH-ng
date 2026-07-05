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

#include <cstdint>
#include <string>
#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Subscriber hook signature. Called for every formatted log line that passes
// through LogManager::write(). `line` is NUL-terminated, `len` excludes the
// terminator. Subscribers must not block — they should enqueue and defer I/O
// to a worker task (this is how syslog.cpp and log_stream.cpp implement
// their forwarders).
typedef void (*log_line_subscriber_t)(const char *line, size_t len);

class LogManager {
public:
    static LogManager& instance();

    // Static wrappers for initialization (called from main.cpp / WebUI)
    static void init();  // install the capture hook at boot (idempotent)
    static void begin(size_t size = 8192);
    // Frees the ring buffer and restores the default log sink. The buffer is
    // NOT allocated at boot (saves ~8 KB heap needed for the TLS handshake
    // during firmware update checks); the WebUI enables it on demand.
    static void stop();
    static void clear();

    // True while the in-memory ring buffer is active (i.e. begin() was called
    // and stop() has not freed it since). Read by the WebUI to show whether
    // log capture is running.
    bool isEnabled() const;

    // Subscriber registry. add/remove are idempotent and safe to call from
    // any task. At most LOG_MAX_SUBSCRIBERS may be registered concurrently.
    static constexpr int LOG_MAX_SUBSCRIBERS = 4;
    void addSubscriber(log_line_subscriber_t sub);
    void removeSubscriber(log_line_subscriber_t sub);
    int subscriberCount() const;

    // Instance methods
    std::string getLogContent(size_t offset = 0);
    size_t getTotalWritten() const;

private:
    LogManager();

    // Internal implementation
    void _begin(size_t size);
    void _stop();
    void _clear();
    void write(const char* data, size_t len);

    char *log_buffer = nullptr;
    size_t log_buffer_size = 0;
    uint64_t total_written = 0;
    mutable SemaphoreHandle_t _mutex = nullptr;

    // Subscriber list. Iterated under _mutex on every write(); keep it small.
    log_line_subscriber_t _subscribers[LOG_MAX_SUBSCRIBERS] = {};
    int _subscriber_count = 0;
    bool _hook_installed = false;

    // Original esp_log vprintf handler saved by begin() and restored by
    // stop(); typed as a plain function pointer to avoid pulling esp_log.h
    // into this header.
    int (*_orig_vprintf)(const char *, va_list) = nullptr;

    // Allow the C-style callback to access private members
    friend int log_vprintf(const char *fmt, va_list args);
};
