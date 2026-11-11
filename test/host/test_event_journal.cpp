// Execute the real journal against a capacity-limited NVS and real host mutexes.
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include "event_journal.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "nvs_storage_lock.h"

static std::vector<unsigned char> stored;
static std::vector<unsigned char> pending;
static std::recursive_mutex storage_mutex;
static std::mutex ring_mutex;
static size_t usable_entries     = 400;
static int set_calls             = 0;
static int erase_calls           = 0;
static int64_t now_us            = 1;
static bool fail_alloc           = false;
static bool fail_commit          = false;
static size_t largest_allocation = 0;
static std::function<void()> during_commit;

static void *journal_malloc(size_t n)
{
    largest_allocation = n > largest_allocation ? n : largest_allocation;
    return fail_alloc ? nullptr : std::malloc(n);
}
static void journal_free(void *p)
{
    std::free(p);
}
// Only allocations in the production journal are intercepted, not the harness.
#define malloc journal_malloc
#define free   journal_free
#include "../../main/event_journal.cpp"
#undef malloc
#undef free

bool journal_test_storage_take(TickType_t timeout)
{
    if (timeout == 0) return storage_mutex.try_lock();
    storage_mutex.lock();
    return true;
}
void journal_test_storage_release()
{
    storage_mutex.unlock();
}
extern "C" SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *)
{
    return &ring_mutex;
}
extern "C" BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t)
{
    assert(m == &ring_mutex);
    ring_mutex.lock();
    return pdTRUE;
}
extern "C" BaseType_t xSemaphoreGive(SemaphoreHandle_t m)
{
    assert(m == &ring_mutex);
    ring_mutex.unlock();
    return pdTRUE;
}
int64_t esp_timer_get_time()
{
    return now_us;
}
const char *esp_err_to_name(esp_err_t)
{
    return "mock error";
}
static size_t entries_for(size_t bytes)
{
    return bytes ? 3 + (bytes + 31) / 32 : 0;
}
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h)
{
    assert(std::strcmp(ns, "journal") == 0);
    if (mode == NVS_READONLY && stored.empty()) return ESP_ERR_NVS_NOT_FOUND;
    *h = 1;
    return ESP_OK;
}
void nvs_close(nvs_handle_t h)
{
    assert(h == 1);
}
esp_err_t nvs_get_blob(nvs_handle_t, const char *key, void *out, size_t *len)
{
    assert(std::strcmp(key, "ring") == 0);
    if (stored.empty()) return ESP_ERR_NVS_NOT_FOUND;
    if (!out) {
        *len = stored.size();
        return ESP_OK;
    }
    if (*len < stored.size()) return ESP_ERR_NVS_INVALID_LENGTH;
    std::memcpy(out, stored.data(), stored.size());
    *len = stored.size();
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t, const char *key, const void *in, size_t len)
{
    assert(std::strcmp(key, "ring") == 0);
    set_calls++;
    if (entries_for(stored.size()) + entries_for(len) > usable_entries)
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    const auto *p = static_cast<const unsigned char *>(in);
    pending.assign(p, p + len);
    return ESP_OK;
}
esp_err_t nvs_erase_key(nvs_handle_t, const char *key)
{
    assert(std::strcmp(key, "ring") == 0);
    erase_calls++;
    stored.clear();
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t)
{
    if (during_commit) {
        auto callback = std::move(during_commit);
        during_commit = {};
        callback();
    }
    if (fail_commit) {
        pending.clear();
        return ESP_FAIL;
    }
    if (!pending.empty()) {
        stored = pending;
        pending.clear();
    }
    return ESP_OK;
}
esp_err_t nvs_get_stats(const char *, nvs_stats_t *stats)
{
    const size_t used        = entries_for(stored.size());
    stats->available_entries = usable_entries > used ? usable_entries - used : 0;
    return ESP_OK;
}
static void reset()
{
    stored.clear();
    pending.clear();
    usable_entries     = 400;
    set_calls          = 0;
    erase_calls        = 0;
    now_us             = 1;
    fail_alloc         = false;
    fail_commit        = false;
    largest_allocation = 0;
    during_commit      = {};
    event_journal_init();
}
static void add(unsigned n)
{
    for (unsigned i = 0; i < n; ++i) {
        const std::string text = "event-" + std::to_string(i);
        event_journal_add(static_cast<uint8_t>(i), text.c_str());
    }
}
static event_journal_entry_t get(size_t i)
{
    event_journal_entry_t result{};
    assert(event_journal_copy(i, &result));
    return result;
}
static void seed_legacy(unsigned n)
{
    std::vector<event_journal_entry_t> old(n);
    for (unsigned i = 0; i < n; ++i) {
        old[i].epoch    = 1700000000 + i;
        old[i].event_id = i;
        std::memset(old[i].detail, 'x', sizeof(old[i].detail));
    }
    const auto *p = reinterpret_cast<const unsigned char *>(old.data());
    stored.assign(p, p + old.size() * sizeof(old[0]));
}
int main()
{
    reset();
    add(50);
    assert(event_journal_count() == 24);
    assert(get(0).event_id == 49);
    assert(get(23).event_id == 26);
    event_journal_flush();
    assert(stored.size() == 8 * sizeof(event_journal_entry_t));
    assert(largest_allocation <= 896);
    event_journal_init();
    assert(event_journal_count() == 8);
    assert(get(0).event_id == 49);
    assert(get(7).event_id == 42);

    reset();
    seed_legacy(40);
    event_journal_init();
    assert(event_journal_count() == 24);
    assert(get(0).event_id == 39);
    assert(get(23).event_id == 16);
    assert(get(0).detail[95] == '\0');
    usable_entries = 160; // old blob occupies 143: no room for an atomic replacement
    event_journal_flush();
    assert(erase_calls == 1);
    assert(stored.size() == 896);
    event_journal_init();
    assert(get(0).event_id == 39);
    assert(get(7).event_id == 32);

    reset();
    usable_entries = 40;
    add(10);
    event_journal_flush();
    assert(stored.size() == 6 * sizeof(event_journal_entry_t)); // leave 16 NVS entries reserved
    assert(usable_entries - entries_for(stored.size()) >= 16);
    event_journal_init();
    assert(get(0).event_id == 9);
    assert(get(5).event_id == 4);

    reset();
    usable_entries = 10;
    add(1);
    event_journal_flush();
    assert(set_calls == 0);
    assert(erase_calls == 0);
    usable_entries = 400;
    now_us += 60000000;
    event_journal_flush();
    assert(set_calls == 0);
    now_us += 240000000;
    event_journal_flush();
    assert(set_calls == 1);

    reset();
    add(1);
    fail_alloc = true;
    event_journal_flush();
    assert(stored.empty());
    fail_alloc = false;
    now_us += 300000000;
    event_journal_flush();
    assert(!stored.empty());

    reset();
    add(1);
    during_commit = [] {
        std::thread producer([] { event_journal_add(99, "during commit"); });
        producer.join();
    };
    event_journal_flush();
    event_journal_flush();
    event_journal_init();
    assert(get(0).event_id == 99);
    assert(event_journal_count() == 2);

    reset();
    add(1);
    fail_commit = true;
    event_journal_flush();
    fail_commit = false;
    now_us += 300000000;
    event_journal_flush();
    event_journal_init();
    assert(event_journal_count() == 1);

    reset();
    stored.assign(LEGACY_CAPACITY * sizeof(event_journal_entry_t) + 112, 0);
    event_journal_init();
    assert(event_journal_count() == 0);
    assert(largest_allocation == 0);
    reset();
    stored.assign(113, 0);
    event_journal_init();
    assert(event_journal_count() == 0);

    reset();
    std::thread a([] { add(100); });
    std::thread b([] { add(100); });
    a.join();
    b.join();
    assert(event_journal_count() == 24);
    event_journal_flush();
    assert(stored.size() == 896);
    event_journal_entry_t entry;
    assert(!event_journal_copy(24, &entry));
    assert(!event_journal_copy(0, nullptr));
    std::cout << "Journal tests passed: bounded migration, NVS reserve/recovery, backoff, "
                 "allocation/commit failure, concurrent producers and flush.\n";
}
