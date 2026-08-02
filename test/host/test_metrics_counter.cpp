#include "metrics.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

extern "C" SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return reinterpret_cast<SemaphoreHandle_t>(1);
}

extern "C" BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore,
                                      TickType_t timeout)
{
    (void)timeout;
    return semaphore ? pdTRUE : pdFALSE;
}

extern "C" BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    return semaphore ? pdTRUE : pdFALSE;
}

extern "C" void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
    std::this_thread::yield();
}

static std::atomic<bool> test_hook_enabled{false};
static std::atomic<bool> test_hook_at_boundary{false};
static std::atomic<bool> test_hook_release{false};

extern "C" void metrics_test_after_low_update(void)
{
    if (!test_hook_enabled.load(std::memory_order_acquire)) return;
    test_hook_at_boundary.store(true, std::memory_order_release);
    while (!test_hook_release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

int main()
{
    metrics_init();
    metrics_counter_t counter = metrics_register_counter(
        "hbrfeth_test_frames_total", "native 32-bit rollover test");
    assert(counter != nullptr);
    assert(metrics_register_counter("hbrfeth_test_frames_total", "ignored") ==
           counter);
    assert(metrics_get(counter) == 0);

    // Exercise the low-word carry explicitly. The stored representation must
    // reach 2^32 without any emulated 64-bit atomic operation.
    metrics_inc(counter, UINT32_MAX);
    metrics_inc_one(counter);
    assert(metrics_get(counter) == (UINT64_C(1) << 32));

    constexpr int thread_count = 8;
    constexpr int increments_per_thread = 125000;
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    for (int thread = 0; thread < thread_count; ++thread) {
        workers.emplace_back([counter]() {
            for (int i = 0; i < increments_per_thread; ++i) {
                metrics_inc_one(counter);
            }
        });
    }
    for (std::thread &worker : workers) worker.join();

    const uint64_t expected =
        (UINT64_C(1) << 32) + thread_count * increments_per_thread;
    assert(metrics_get(counter) == expected);

    // Deterministically suspend a wrapping writer after it publishes low=0
    // but before it carries high. A coherent reader must wait for the active
    // writer instead of returning the torn value zero.
    metrics_counter_t suspended_rollover = metrics_register_counter(
        "hbrfeth_test_suspended_rollover_total",
        "deterministic in-flight rollover test");
    assert(suspended_rollover != nullptr);
    metrics_inc(suspended_rollover, UINT32_MAX);
    test_hook_at_boundary.store(false, std::memory_order_relaxed);
    test_hook_release.store(false, std::memory_order_relaxed);
    test_hook_enabled.store(true, std::memory_order_release);

    std::thread suspended_writer([&]() {
        metrics_inc_one(suspended_rollover);
    });
    while (!test_hook_at_boundary.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::atomic<bool> reader_started{false};
    std::atomic<bool> reader_finished{false};
    uint64_t suspended_value = 0;
    std::thread suspended_reader([&]() {
        reader_started.store(true, std::memory_order_release);
        suspended_value = metrics_get(suspended_rollover);
        reader_finished.store(true, std::memory_order_release);
    });
    while (!reader_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(!reader_finished.load(std::memory_order_acquire));

    test_hook_release.store(true, std::memory_order_release);
    suspended_writer.join();
    suspended_reader.join();
    test_hook_enabled.store(false, std::memory_order_release);
    assert(suspended_value == (UINT64_C(1) << 32));

    // A normal low/high split has a torn-read window after low wraps but
    // before high receives the carry. Large deltas force that boundary on
    // almost every update while a concurrent reader verifies monotonicity.
    metrics_counter_t rollover = metrics_register_counter(
        "hbrfeth_test_rollover_total", "concurrent rollover snapshot test");
    assert(rollover != nullptr);
    constexpr int rollover_writer_count = 4;
    constexpr int rollover_updates_per_writer = 100000;
    std::atomic<bool> start_rollover{false};
    std::atomic<int> writers_left{rollover_writer_count};
    std::thread reader([&]() {
        while (!start_rollover.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        uint64_t previous = 0;
        while (writers_left.load(std::memory_order_acquire) > 0) {
            const uint64_t current = metrics_get(rollover);
            assert(current >= previous);
            previous = current;
        }
        assert(metrics_get(rollover) >= previous);
    });

    std::vector<std::thread> rollover_writers;
    rollover_writers.reserve(rollover_writer_count);
    for (int thread = 0; thread < rollover_writer_count; ++thread) {
        rollover_writers.emplace_back([&]() {
            while (!start_rollover.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < rollover_updates_per_writer; ++i) {
                metrics_inc(rollover, UINT32_MAX);
            }
            writers_left.fetch_sub(1, std::memory_order_release);
        });
    }
    start_rollover.store(true, std::memory_order_release);
    for (std::thread &writer : rollover_writers) writer.join();
    reader.join();

    const uint64_t rollover_expected =
        static_cast<uint64_t>(rollover_writer_count) *
        rollover_updates_per_writer * UINT32_MAX;
    assert(metrics_get(rollover) == rollover_expected);

    char rendered[512] = {};
    const size_t length =
        metrics_render_prometheus(rendered, sizeof(rendered), 0);
    assert(length > 0);
    assert(std::strstr(rendered, "# TYPE hbrfeth_test_frames_total counter") !=
           nullptr);
    assert(std::strstr(rendered, "4295967296") != nullptr);
    return 0;
}
