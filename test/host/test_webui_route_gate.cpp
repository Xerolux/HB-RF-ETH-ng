// Host regression test for the web server's boot-time route gate (issue #491).
//
// The first half mirrors the ordering of ESP-IDF v6.1.1 that makes the race
// possible; the second half runs the real gate (main/webui_route_gate.cpp)
// against two threads and checks the contract WebUI::start() relies on: no
// request is dispatched before the last route is registered.
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread -Itest/host/stubs -Iinclude
//       main/webui_route_gate.cpp test/host/test_webui_route_gate.cpp

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>

#include "freertos/event_groups.h"
#include "webui_route_gate.h"

// ---------------------------------------------------------------------------
// FreeRTOS event group backed by a real condition variable, so a thread that
// waits in the gate really blocks and is really woken by the release.
// ---------------------------------------------------------------------------
struct StubEventGroup {
    std::mutex mutex;
    std::condition_variable cv;
    EventBits_t bits = 0;
    unsigned waiters = 0;          // threads currently blocked in WaitBits
    unsigned wait_calls = 0;
    EventBits_t requested_bits = 0;
    TickType_t requested_ticks = 0;
    bool expire_next_wait = false; // test hook: model an elapsed timeout
};

static StubEventGroup *g_live = nullptr;

EventGroupHandle_t xEventGroupCreateStatic(StaticEventGroup_t *storage)
{
    static_assert(sizeof(StubEventGroup) <= sizeof(StaticEventGroup_t),
                  "StaticEventGroup_t stub is too small");
    if (g_live) {
        g_live->~StubEventGroup();
    }
    g_live = new (storage->storage) StubEventGroup();
    return g_live;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits)
{
    std::lock_guard<std::mutex> lock(group->mutex);
    group->bits |= bits;
    group->cv.notify_all();
    return group->bits;
}

static bool bits_satisfied(const StubEventGroup *group, EventBits_t bits_to_wait_for,
                           BaseType_t wait_for_all_bits)
{
    const EventBits_t held = group->bits & bits_to_wait_for;
    return wait_for_all_bits == pdTRUE ? held == bits_to_wait_for : held != 0;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t group,
                                EventBits_t bits_to_wait_for,
                                BaseType_t clear_on_exit,
                                BaseType_t wait_for_all_bits,
                                TickType_t ticks_to_wait)
{
    std::unique_lock<std::mutex> lock(group->mutex);
    ++group->wait_calls;
    group->requested_bits = bits_to_wait_for;
    group->requested_ticks = ticks_to_wait;

    const auto satisfied = [&] {
        return bits_satisfied(group, bits_to_wait_for, wait_for_all_bits);
    };

    ++group->waiters;
    // Let a test that waits for "a thread is parked here" re-check its predicate.
    group->cv.notify_all();
    bool expired = false;
    if (group->expire_next_wait) {
        group->expire_next_wait = false;
        expired = true;
    } else if (ticks_to_wait == portMAX_DELAY) {
        group->cv.wait(lock, satisfied);
    } else {
        expired = !group->cv.wait_for(lock, std::chrono::milliseconds(ticks_to_wait),
                                      satisfied);
    }
    --group->waiters;

    if (expired) {
        return 0;
    }
    const EventBits_t held = group->bits & bits_to_wait_for;
    if (clear_on_exit == pdTRUE) {
        group->bits &= ~held;
    }
    return held;
}

// ---------------------------------------------------------------------------
// Mirror of the relevant part of esp_http_server/src/httpd_uri.c (v6.1.1)
// ---------------------------------------------------------------------------

// What an uninitialised httpd_uri_t::uri is to strlen(): an address no load may
// touch. The model compares the pointer value instead of loading through it.
static constexpr uintptr_t kUninitialised = 0xa5a5a5a5u;
static constexpr std::size_t kMaxHandlers = 64;

struct ModelHandler {
    std::atomic<uintptr_t> uri{kUninitialised};
};

static std::atomic<ModelHandler *> g_calls[kMaxHandlers];

// Test hook: invoked right after a slot is published and before it is
// initialised - the window in which the server thread may preempt.
static void (*g_after_publish)(void *) = nullptr;
static void *g_after_publish_context = nullptr;

static void model_reset()
{
    for (auto &slot : g_calls) {
        delete slot.exchange(nullptr);
    }
}

// httpd_register_uri_handler(), reduced to the ordering that matters.
static void model_register(const char *uri)
{
    for (auto &slot : g_calls) {
        ModelHandler *expected = nullptr;
        ModelHandler *fresh = new ModelHandler;
        if (slot.compare_exchange_strong(expected, fresh)) {
            if (g_after_publish) {
                g_after_publish(g_after_publish_context);
            }
            fresh->uri.store(reinterpret_cast<uintptr_t>(uri), std::memory_order_release);
            return;
        }
        delete fresh;
    }
    assert(false && "handler table full");
}

enum class Dispatch { kFound, kNotFound, kLoadProhibited };

// httpd_find_uri_handler(): walks the published slots and dereferences uri in
// the matcher for each of them.
static Dispatch model_dispatch(const char *uri)
{
    for (auto &slot : g_calls) {
        ModelHandler *handler = slot.load(std::memory_order_acquire);
        if (handler == nullptr) {
            break;
        }
        const uintptr_t candidate = handler->uri.load(std::memory_order_acquire);
        if (candidate == kUninitialised) {
            return Dispatch::kLoadProhibited; // strlen() through a wild pointer
        }
        if (std::strcmp(reinterpret_cast<const char *>(candidate), uri) == 0) {
            return Dispatch::kFound;
        }
    }
    return Dispatch::kNotFound;
}

// A slice of what WebUI::start() enters, in its order: the exact routes first,
// the "/*" SPA catch-all last.
static const char *const kRoutes[] = {
    "/api/log/stream",   "/api/system/overview", "/login.json",       "/sysinfo.json",
    "/settings.json",    "/ota_update",          "/api/update/status", "/api/log",
    "/api/log/download", "/favicon.ico",         "/main.js",           "/main.css",
    "/",                 // the catch-all is registered last
};

static const std::size_t kRouteCount = sizeof(kRoutes) / sizeof(kRoutes[0]);

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Without the gate, a request dispatched while a route is being registered
// reads the freshly published, not yet initialised slot. This is the reported
// LoadProhibited in httpd_uri_match_wildcard(), and it pins the model to the
// defect: if this stops faulting, the model no longer reproduces IDF.
static void test_ungated_dispatch_reads_uninitialised_slot()
{
    model_reset();
    model_register(kRoutes[0]);

    // The server thread preempts the registering task between the two stores
    // of httpd_register_uri_handler() and dispatches a request of any kind.
    Dispatch seen = Dispatch::kNotFound;
    g_after_publish_context = &seen;
    g_after_publish = [](void *context) {
        *static_cast<Dispatch *>(context) = model_dispatch("/sysinfo.json");
    };
    model_register(kRoutes[1]);
    g_after_publish = nullptr;
    g_after_publish_context = nullptr;

    assert(seen == Dispatch::kLoadProhibited);

    // Once registration has moved on the same request is served normally.
    assert(model_dispatch("/api/system/overview") == Dispatch::kFound);
    assert(model_dispatch(kRoutes[0]) == Dispatch::kFound);
}

static void wait_for_waiter()
{
    // Give the server thread ample time to reach the gate. Not asserted: the
    // gate must hold it whenever it arrives, whether or not it got that far.
    if (g_live == nullptr) {
        return;
    }
    std::unique_lock<std::mutex> lock(g_live->mutex);
    g_live->cv.wait_for(lock, std::chrono::seconds(5), [] { return g_live->waiters > 0; });
}

// With the gate, the server worker may start at any point of the registration
// sequence: it is held until the last route is in, then serves the complete
// table. The iteration count varies where the worker interleaves.
static void test_gate_holds_sessions_until_routes_are_complete()
{
    for (std::size_t already_registered = 0; already_registered < kRouteCount;
         ++already_registered) {
        for (int round = 0; round < 8; ++round) {
            model_reset();
            webui_route_gate_init();
            assert(!webui_route_gate_released());

            std::atomic<bool> registration_complete{false};
            std::atomic<bool> served{false};
            std::atomic<bool> served_too_early{false};
            std::atomic<int> open_result{-1};
            std::atomic<Dispatch> dispatched{Dispatch::kNotFound};

            for (std::size_t i = 0; i < already_registered; ++i) {
                model_register(kRoutes[i]);
            }

            // The server worker: one session, then one request for the route
            // that is registered last.
            std::thread server([&] {
                open_result = webui_route_gate_open_session(nullptr, 7);
                if (!registration_complete.load()) {
                    served_too_early = true;
                }
                dispatched = model_dispatch("/");
                served = true;
            });

            if (round % 2 == 0) {
                wait_for_waiter();
            } else {
                std::this_thread::yield();
            }

            // Whatever the worker did so far, nothing was dispatched.
            assert(!served.load());

            for (std::size_t i = already_registered; i < kRouteCount; ++i) {
                model_register(kRoutes[i]);
                if (round % 4 == 1) {
                    std::this_thread::yield();
                }
                assert(!served.load());
            }

            registration_complete = true;
            webui_route_gate_release();
            assert(webui_route_gate_released());
            server.join();

            assert(open_result.load() == ESP_OK);
            assert(!served_too_early.load());
            assert(dispatched.load() == Dispatch::kFound);
            assert(served.load());

            // Released: later sessions pass without being held.
            assert(webui_route_gate_open_session(nullptr, 8) == ESP_OK);
        }
    }

    // The waiting thread was really parked in the gate in the rounds that
    // waited for it; otherwise the test would pass on scheduling luck alone.
    assert(g_live != nullptr && g_live->wait_calls > 0);
    assert(g_live->waiters == 0);
}

// A registration that never completes must not turn the gate into a dead web
// server: the wait is bounded and the session is served, never refused.
static void test_bounded_wait_never_refuses_a_session()
{
    webui_route_gate_init();
    assert(g_live != nullptr);

    const unsigned before = g_live->wait_calls;
    g_live->expire_next_wait = true;
    assert(webui_route_gate_open_session(nullptr, 9) == ESP_OK);

    assert(g_live->wait_calls == before + 1);
    assert(g_live->requested_bits == BIT0);
    assert(g_live->requested_ticks == pdMS_TO_TICKS(WEBUI_ROUTE_GATE_WAIT_MS));
    assert(g_live->requested_ticks > 0);
    assert(g_live->requested_ticks != portMAX_DELAY);
    assert(!webui_route_gate_released());
}

static void test_released_gate_does_not_delay_sessions()
{
    webui_route_gate_init();
    webui_route_gate_release();

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 2000; ++i) {
        assert(webui_route_gate_open_session(nullptr, i) == ESP_OK);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    // No thread released anything in between: every call returned at once.
    assert(elapsed < std::chrono::seconds(1));

    // Releasing again is harmless and the gate stays open.
    webui_route_gate_release();
    assert(webui_route_gate_released());
    assert(webui_route_gate_open_session(nullptr, 0) == ESP_OK);
}

int main()
{
    test_ungated_dispatch_reads_uninitialised_slot();
    test_gate_holds_sessions_until_routes_are_complete();
    test_bounded_wait_never_refuses_a_session();
    test_released_gate_does_not_delay_sessions();
    model_reset();
    return 0;
}
