"""Run real diagnostic methods with bounded allocators and failing RTOS/NVS I/O."""
from pathlib import Path
import os
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def execute(source):
    with tempfile.TemporaryDirectory() as temp:
        cpp = Path(temp) / "diagnostics.cpp"
        binary = Path(temp) / "diagnostics"
        cpp.write_text(source, encoding="utf-8")
        command = ["g++", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pthread", "-I" + str(ROOT / "include"), str(cpp), "-o", str(binary)]
        if os.environ.get("DIAGNOSTICS_SANITIZE") == "1":
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        built = subprocess.run(command, capture_output=True, text=True)
        if built.returncode:
            raise AssertionError(built.stderr)
        completed = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
        if completed.returncode:
            raise AssertionError(completed.stdout + completed.stderr)
        return completed.stdout


@unittest.skipUnless(shutil.which("g++"), "requires host C++ compiler")
class DiagnosticBuffersTest(unittest.TestCase):
    def test_task_snapshot_one_allocation_full_output_failure_and_concurrency(self):
        source = (ROOT / "main/sysinfo.cpp").read_text(encoding="utf-8")
        methods = source[source.index("std::string SysInfo::getTaskStackInfo()") :]
        harness = r'''
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <climits>
#include <string>
#include <thread>
#include <vector>
#include "sysinfo.h"
#define CONFIG_FREERTOS_MAX_TASK_NAME_LEN 16
using UBaseType_t = unsigned;
struct TaskStatus_t {
    void *xHandle;
    const char *pcTaskName;
    uint32_t usStackHighWaterMark;
    uint32_t other[6];
};
static std::vector<std::string> names;
static std::vector<TaskStatus_t> records;
static std::atomic<size_t> active{0}, allocations{0}, largest{0};
static std::atomic<bool> allocation_fail{false};
static bool snapshot_fail = false;
static size_t reported_count = 0;
static void *diag_malloc(size_t size) {
    allocations++; largest = size;
    if (allocation_fail) return nullptr;
    void *p = std::malloc(size); if (p) active++; return p;
}
static void diag_free(void *p) { if (p) active--; std::free(p); }
UBaseType_t uxTaskGetNumberOfTasks() { return reported_count; }
UBaseType_t uxTaskGetSystemState(TaskStatus_t *out, size_t maximum, void *) {
    if (snapshot_fail || records.size() > maximum) return 0;
    std::memcpy(out, records.data(), records.size() * sizeof(records[0]));
    return records.size();
}
SysInfo::SysInfo() {}
#define malloc diag_malloc
#define free diag_free
'''
        assertions = r'''
#undef malloc
#undef free
static bool capture(void *context, const char *text, size_t length) {
    static_cast<std::string *>(context)->assign(text, length); return true;
}
int main() {
    SysInfo info;
    names.reserve(80); records.reserve(80);
    for (unsigned i = 0; i < 72; ++i) names.push_back("task-" + std::to_string(i));
    names[0] = "IDLE0"; names[1] = "IDLE1"; names[2] = "abcdefghijklmno";
    for (unsigned i = 0; i < names.size(); ++i)
        records.push_back({nullptr, names[i].c_str(), i == 2 ? UINT_MAX : 72 - i, {}});
    reported_count = records.size();
    auto expected_records = records;
    std::stable_sort(expected_records.begin(), expected_records.end(), [](const auto &a, const auto &b) { return a.usStackHighWaterMark < b.usStackHighWaterMark; });
    std::string expected;
    for (const auto &task : expected_records) {
        if (std::strcmp(task.pcTaskName, "IDLE0") == 0 || std::strcmp(task.pcTaskName, "IDLE1") == 0) continue;
        if (!expected.empty()) expected += ' ';
        expected += task.pcTaskName; expected += '='; expected += std::to_string(task.usStackHighWaterMark);
    }
    std::string actual;
    assert(info.writeTaskStackInfo(capture, &actual)); assert(actual == expected);
    assert(allocations == 1); assert(largest == (reported_count + 8) * sizeof(TaskStatus_t)); assert(active == 0);
    assert(actual.find("abcdefghijklmno=4294967295") != std::string::npos);
    assert(info.getTaskStackInfo() == expected);
    const size_t before = allocations;
    assert(!info.writeTaskStackInfo(nullptr, nullptr)); assert(allocations == before);
    assert(!info.writeTaskStackInfo([](void *, const char *, size_t) { return false; }, nullptr)); assert(active == 0);
    allocation_fail = true;
    assert(info.writeTaskStackInfo(capture, &actual)); assert(actual == "alloc-failed"); assert(active == 0);
    allocation_fail = false; snapshot_fail = true;
    assert(info.writeTaskStackInfo(capture, &actual)); assert(actual == "no-tasks"); assert(active == 0);
    snapshot_fail = false; reported_count = 1; // more than the count + 8: no partial diagnostics
    assert(info.writeTaskStackInfo(capture, &actual)); assert(actual == "no-tasks");
    reported_count = 0;
    assert(info.writeTaskStackInfo(capture, &actual)); assert(actual == "no-tasks");
    reported_count = records.size();
    auto parallel = [&] { for (int i = 0; i < 100; i++) { std::string out; assert(info.writeTaskStackInfo(capture, &out)); assert(out == expected); } };
    std::thread a(parallel), b(parallel); a.join(); b.join(); assert(active == 0);
}
'''
        execute(harness + methods + assertions)

    def test_crash_tail_ring_wrap_allocation_limit_clear_and_nvs_failures(self):
        source = (ROOT / "main/log_manager.cpp").read_text(encoding="utf-8")
        read_tail = source[source.index("size_t LogManager::readTail(") : source.index("uint64_t LogManager::getTotalWritten()")]
        buffered = source[source.index("size_t LogManager::getBufferedBytes()") : source.index("// NVS keys for the crash-tail snapshot")]
        save = source[source.index("bool LogManager::saveCrashTailNvs(") : source.index("std::string LogManager::loadCrashTailNvs()")]
        harness = r'''
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, NVS_READWRITE = 1, pdTRUE = 1;
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGE(...) ((void)0)
using nvs_handle_t = int;
static bool deny_ring = false, deny_storage = false, allocation_fail = false, clear_on_allocate = false;
static int open_error = 0, write_error = 0, commit_error = 0;
static int writes = 0, commits = 0, closes = 0, allocations = 0, active = 0, takes = 0;
static size_t largest = 0;
static std::string persisted;
static std::vector<char> ring;
class LogManager {
public:
    static constexpr size_t CRASH_TAIL_MAX = 1024;
    char *log_buffer = nullptr; size_t log_buffer_size = 0;
    uint64_t total_written = 0, ring_start_offset = 0;
    void *_mutex = reinterpret_cast<void *>(1);
    size_t readTail(char *, size_t);
    size_t getBufferedBytes() const;
    bool saveCrashTailNvs(const char *);
};
static LogManager logger;
static int xSemaphoreTake(void *, int timeout) { assert(timeout == 10 || timeout == 20); takes++; return deny_ring ? 0 : pdTRUE; }
static void xSemaphoreGive(void *) {}
class NvsStorageLock {
public:
    NvsStorageLock(int timeout, const char *) { assert(timeout == 20); }
    explicit operator bool() const { return !deny_storage; }
};
static int nvs_open(const char *ns, int, nvs_handle_t *h) { assert(std::strcmp(ns, "reset_info") == 0); *h = 1; return open_error; }
static int nvs_set_blob(nvs_handle_t, const char *key, const void *data, size_t size) {
    assert(std::strcmp(key, "clog") == 0); writes++;
    if (!write_error) persisted.assign(static_cast<const char *>(data), size);
    return write_error;
}
static int nvs_commit(nvs_handle_t) { commits++; return commit_error; }
static void nvs_close(nvs_handle_t) { closes++; }
static void *diag_malloc(size_t size) {
    allocations++; largest = size;
    if (clear_on_allocate) logger.ring_start_offset = logger.total_written;
    if (allocation_fail) return nullptr;
    void *p = std::malloc(size); if (p) active++; return p;
}
static void diag_free(void *p) { if (p) active--; std::free(p); }
static const char *CRASH_TAIL_NVS_NS = "reset_info", *CRASH_TAIL_NVS_KEY = "clog";
#define malloc diag_malloc
#define free diag_free
'''
        assertions = r'''
#undef malloc
#undef free
static void fill(size_t capacity, const std::string &input) {
    ring.assign(capacity, 0);
    for (size_t i = 0; i < input.size(); i++) ring[i % capacity] = input[i];
    logger.log_buffer = ring.data(); logger.log_buffer_size = capacity;
    logger.total_written = input.size(); logger.ring_start_offset = 0;
}
int main() {
    char out[2048];
    assert(logger.readTail(nullptr, 10) == 0); assert(logger.readTail(out, 0) == 0); assert(takes == 0);
    assert(!logger.saveCrashTailNvs("test")); assert(allocations == 0);
    fill(8, "abcdefghijkl");
    assert(logger.readTail(out, 5) == 5); assert(std::string(out, 5) == "hijkl");
    assert(logger.readTail(out, sizeof(out)) == 8); assert(std::string(out, 8) == "efghijkl");
    logger.ring_start_offset = logger.total_written;
    assert(logger.readTail(out, sizeof(out)) == 0); assert(!logger.saveCrashTailNvs("empty"));
    std::string input(16384, 'x');
    for (size_t i = 0; i < input.size(); i++) input[i] = static_cast<char>(i % 256);
    fill(8192, input);
    assert(logger.saveCrashTailNvs("heap_watchdog"));
    assert(persisted == "[heap_watchdog] " + input.substr(input.size() - 1024));
    assert(largest <= 1051); assert(active == 0); assert(writes == 1);
    assert(logger.saveCrashTailNvs(nullptr)); assert(persisted == "[crash] " + input.substr(input.size() - 1024));
    assert(logger.saveCrashTailNvs(std::string(400, 'a').c_str())); assert(persisted.size() == 1050); assert(largest == 1051);
    fill(64, "short log"); assert(logger.saveCrashTailNvs("s")); assert(persisted == "[s] short log");
    allocation_fail = true; int before = writes;
    assert(!logger.saveCrashTailNvs("oom")); assert(writes == before); assert(active == 0); allocation_fail = false;
    deny_ring = true; before = allocations;
    assert(!logger.saveCrashTailNvs("busy")); assert(allocations == before); deny_ring = false;
    clear_on_allocate = true; before = writes;
    assert(!logger.saveCrashTailNvs("clear raced")); assert(writes == before); assert(active == 0); clear_on_allocate = false;
    fill(64, "short log"); deny_storage = true;
    assert(!logger.saveCrashTailNvs("locked")); assert(active == 0); deny_storage = false;
    open_error = ESP_FAIL; before = closes;
    assert(!logger.saveCrashTailNvs("open failed")); assert(closes == before); assert(active == 0); open_error = 0;
    write_error = ESP_FAIL; before = commits;
    assert(!logger.saveCrashTailNvs("write failed")); assert(commits == before); assert(active == 0); write_error = 0;
    commit_error = ESP_FAIL; before = closes;
    assert(!logger.saveCrashTailNvs("commit failed")); assert(closes == before + 1); assert(active == 0); commit_error = 0;
    logger.log_buffer = nullptr; assert(logger.readTail(out, 8) == 0);
}
'''
        execute(harness + read_tail + buffered + save + assertions)


    def test_http_json_chunks_preserve_panic_text_and_keep_failed_downloads(self):
        source = (ROOT / "main/webui.cpp").read_text(encoding="utf-8")
        helper = source[source.index("static esp_err_t send_json_string_chunks("):source.index("esp_err_t get_sysinfo_json_handler_func(")]
        start = source.index("esp_err_t get_crash_log_handler_func(")
        handler = source[start:source.index("httpd_uri_t get_crash_log_handler", start)]
        harness = r"""
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
using esp_err_t = int;
using nvs_handle_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, NVS_READWRITE = 1, portMAX_DELAY = -1;
struct httpd_req_t {
    std::string body;
    bool auth = true, ended = false;
    size_t chunks = 0, maximum = 0, fail_at = 0;
};
static std::string snapshot;
static int reads = 0, erases = 0;
static bool deny_storage = false;
static httpd_req_t *current = nullptr;
class LogManager { public: static std::string loadCrashTailNvs() { reads++; return snapshot; } };
void add_security_headers(httpd_req_t *) {}
int validate_auth(httpd_req_t *req) { return req->auth ? ESP_OK : ESP_FAIL; }
void httpd_resp_set_status(httpd_req_t *, const char *) {}
void httpd_resp_set_type(httpd_req_t *, const char *) {}
void httpd_resp_sendstr(httpd_req_t *req, const char *body) { req->body = body; req->ended = true; }
esp_err_t httpd_resp_send_chunk(httpd_req_t *req, const char *text, size_t length) {
    if (++req->chunks == req->fail_at) return ESP_FAIL;
    if (length > req->maximum) req->maximum = length;
    if (text) req->body.append(text, length); else req->ended = true;
    return ESP_OK;
}
class NvsStorageLock {
public:
    NvsStorageLock(int, const char *) { assert(current->ended); }
    explicit operator bool() const { return !deny_storage; }
};
esp_err_t nvs_open(const char *ns, int, nvs_handle_t *h) { assert(std::strcmp(ns, "reset_info") == 0); *h = 1; return 0; }
void nvs_erase_key(nvs_handle_t, const char *key) { assert(current->ended); assert(std::strcmp(key, "clog") == 0); snapshot.clear(); erases++; }
void nvs_commit(nvs_handle_t) {}
void nvs_close(nvs_handle_t) {}
"""
        assertions = r"""
int main() {
    snapshot = "[panic transcript] ";
    const std::string unit = "abc\\\"\n\t\r\x01";
    for (int i = 0; i < 500; i++) snapshot += unit;
    const std::string original = snapshot;
    httpd_req_t success; current = &success;
    assert(get_crash_log_handler_func(&success) == ESP_OK); assert(success.ended);
    assert(success.maximum <= 128); assert(snapshot.empty()); assert(erases == 1);
    std::cout << success.body;
    const size_t chunks = success.chunks;
    for (size_t failure = 1; failure <= chunks; failure++) {
        snapshot = original; httpd_req_t failed; failed.fail_at = failure; current = &failed;
        assert(get_crash_log_handler_func(&failed) == ESP_FAIL);
        assert(snapshot == original); assert(erases == 1);
    }
    snapshot = original; deny_storage = true; httpd_req_t locked; current = &locked;
    assert(get_crash_log_handler_func(&locked) == ESP_OK); assert(snapshot == original); deny_storage = false;
    httpd_req_t denied; denied.auth = false; current = &denied; int before = reads;
    assert(get_crash_log_handler_func(&denied) == ESP_OK); assert(reads == before); assert(erases == 1);
    snapshot.clear(); httpd_req_t empty; current = &empty;
    assert(get_crash_log_handler_func(&empty) == ESP_OK); assert(empty.body == "{\"available\":false,\"tail\":\"\"}");
    for (size_t length : {0, 1, 63, 64, 127, 128, 129, 1024}) {
        httpd_req_t req; std::string text(length, 'x');
        assert(send_json_string_chunks(&req, text.data(), text.size()) == ESP_OK);
        assert(req.maximum <= 128); assert(req.body == text);
    }
}
"""
        payload = json.loads(execute(harness + helper + handler + assertions))
        self.assertTrue(payload["available"])
        self.assertEqual(payload["tail"], "[panic transcript] " + ('abc\\"\n\t' * 500))



if __name__ == "__main__":
    unittest.main()
