"""Execute the actual scheduler cache/pending code against failing NVS boundaries."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class SchedulerStorageTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++"), "requires the CI host C++ compiler")
    def test_cache_reservation_and_failed_pending_persistence(self):
        source = (ROOT / "main/update_scheduler.cpp").read_text(encoding="utf-8")
        cache = source[source.index("static bool cache_open("):source.index("// --- gate inputs")]
        pending = source[source.index("bool update_scheduler_set_pending("):source.index("bool update_scheduler_clear_pending(")]
        # Compile production function bodies rather than a reimplementation.
        harness = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
using esp_err_t = int;
using nvs_handle_t = int;
constexpr int ESP_OK = 0, ESP_ERR_NVS_NOT_FOUND = 2, NVS_READWRITE = 1;
constexpr size_t UPDATE_CHECK_VERSION_LEN = 32, UPDATE_PENDING_MAX_BYTES = 128;
static const char *CACHE_NVS_NAMESPACE = "upd_cache", *CACHE_KEY_PENDING = "upd_pend";
static int lock_depth = 0, write_error = 0, commit_error = 0, writes = 0;
static bool deny_lock = false;
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGW(...) ((void)0)
#define portMAX_DELAY 0
class NvsStorageLock {
    bool locked;
public:
    NvsStorageLock(int, const char *) : locked(!deny_lock) { if (locked) ++lock_depth; }
    ~NvsStorageLock() { if (locked) --lock_depth; }
    explicit operator bool() const { return locked; }
};
static int nvs_open(const char *, int, nvs_handle_t *h) { assert(lock_depth); *h = 1; return 0; }
static void nvs_close(nvs_handle_t) { assert(lock_depth); }
static int nvs_get_str(nvs_handle_t, const char *, char *out, size_t *) { assert(lock_depth); out[0] = 0; return 0; }
static int nvs_get_i32(nvs_handle_t, const char *, int32_t *out) { assert(lock_depth); *out = 0; return 0; }
static int nvs_get_i64(nvs_handle_t, const char *, int64_t *out) { assert(lock_depth); *out = 0; return 0; }
static int nvs_set_str(nvs_handle_t, const char *, const char *) { assert(lock_depth); ++writes; return write_error; }
static int nvs_set_i32(nvs_handle_t, const char *, int32_t) { assert(lock_depth); return write_error; }
static int nvs_set_i64(nvs_handle_t, const char *, int64_t) { assert(lock_depth); return write_error; }
static int nvs_erase_key(nvs_handle_t, const char *) { assert(lock_depth); return write_error; }
static int nvs_commit(nvs_handle_t) { assert(lock_depth); return commit_error; }
static int s_lock = 1;
static void xSemaphoreTake(int, int) {}
static void xSemaphoreGive(int) {}
static struct { char pending[UPDATE_PENDING_MAX_BYTES]; } s_state = {};
static int64_t now_unix() { return 1800000000; }
static bool update_pending_format(char *out, size_t size, bool, const char *v, int64_t) {
    return std::snprintf(out, size, "%s", v) < static_cast<int>(size);
}
static size_t strlcpy(char *dst, const char *src, size_t size) {
    std::snprintf(dst, size, "%s", src); return std::strlen(src);
}
'''
        assertions = r'''
int main() {
    char buffer[32]; int32_t i32; int64_t i64;
    assert(cache_read_str("s", buffer, sizeof(buffer)));
    assert(cache_read_i32("i32", &i32)); assert(cache_read_i64("i64", &i64));
    assert(cache_write_i32("i32", 1)); assert(cache_write_i64("i64", 1)); cache_erase("s");
    assert(lock_depth == 0);
    std::strcpy(s_state.pending, "previous");
    assert(!update_scheduler_set_pending(true, nullptr));
    assert(!update_scheduler_set_pending(true, ""));
    write_error = 1;
    assert(!update_scheduler_set_pending(true, "2.3.1"));
    assert(std::strcmp(s_state.pending, "previous") == 0);
    write_error = 0; commit_error = 1;
    assert(!update_scheduler_set_pending(true, "2.3.1"));
    assert(std::strcmp(s_state.pending, "previous") == 0);
    commit_error = 0; deny_lock = true;
    const int previous_writes = writes;
    assert(!update_scheduler_set_pending(true, "2.3.1"));
    assert(writes == previous_writes);
    assert(std::strcmp(s_state.pending, "previous") == 0);
    deny_lock = false;
    assert(update_scheduler_set_pending(true, "2.3.1"));
    assert(std::strcmp(s_state.pending, "2.3.1") == 0);
    assert(lock_depth == 0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "scheduler.cpp"
            binary = Path(directory) / "scheduler.exe"
            cpp.write_text(harness + cache + pending + assertions, encoding="utf-8")
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            str(cpp), "-o", str(binary)], check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
