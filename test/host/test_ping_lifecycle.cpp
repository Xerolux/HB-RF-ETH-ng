#include <cassert>
#include <cstdint>

#include "freertos/event_groups.h"
#include "ping/ping_sock.h"
#include "ping_service.h"

struct StubEventGroup {
    EventBits_t bits = 0;
    bool deleted = false;
};

static esp_ping_callbacks_t callbacks = {};
static bool ping_ended = false;
static bool callback_after_cleanup = false;
static bool cleanup_before_end = false;
static EventBits_t requested_wait_bits = 0;

EventGroupHandle_t xEventGroupCreate()
{
    return new StubEventGroup();
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits)
{
    if (!group || group->deleted) {
        callback_after_cleanup = true;
        return 0;
    }
    group->bits |= bits;
    if (bits & BIT1) {
        ping_ended = true;
    }
    return group->bits;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t group,
                                EventBits_t bits_to_wait_for,
                                BaseType_t clear_on_exit,
                                BaseType_t wait_for_all_bits,
                                TickType_t)
{
    requested_wait_bits = bits_to_wait_for;

    // Model ESP-IDF's asynchronous ordering: success arrives before the final
    // on_ping_end callback. A caller waiting for "success OR end" resumes
    // immediately and can destroy callback state too soon. Waiting explicitly
    // for end keeps that state alive until the session is finished.
    if (bits_to_wait_for == BIT1) {
        callbacks.on_ping_end(reinterpret_cast<esp_ping_handle_t>(1),
                              callbacks.cb_args);
    }

    EventBits_t result = group->bits & bits_to_wait_for;
    if (wait_for_all_bits == pdTRUE && result != bits_to_wait_for) {
        return 0;
    }
    if (clear_on_exit == pdTRUE) {
        group->bits &= ~result;
    }
    return result;
}

void vEventGroupDelete(EventGroupHandle_t group)
{
    cleanup_before_end = !ping_ended;
    group->deleted = true;
    delete group;
}

esp_err_t esp_ping_new_session(const esp_ping_config_t *,
                               const esp_ping_callbacks_t *registered_callbacks,
                               esp_ping_handle_t *handle)
{
    callbacks = *registered_callbacks;
    *handle = reinterpret_cast<esp_ping_handle_t>(1);
    return ESP_OK;
}

esp_err_t esp_ping_start(esp_ping_handle_t handle)
{
    callbacks.on_ping_success(handle, callbacks.cb_args);
    return ESP_OK;
}

esp_err_t esp_ping_stop(esp_ping_handle_t)
{
    return ESP_OK;
}

esp_err_t esp_ping_delete_session(esp_ping_handle_t)
{
    return ESP_OK;
}

esp_err_t esp_ping_get_profile(esp_ping_handle_t,
                               esp_ping_profile_t profile,
                               void *data,
                               std::uint32_t size)
{
    assert(profile == ESP_PING_PROF_DURATION);
    assert(size == sizeof(std::uint32_t));
    *static_cast<std::uint32_t *>(data) = 17;
    return ESP_OK;
}

int main()
{
    const int latency = ping_service_ping("127.0.0.1", 4000);

    assert(latency == 17);
    assert(requested_wait_bits == BIT1);
    assert(ping_ended);
    assert(!cleanup_before_end);
    assert(!callback_after_cleanup);
    return 0;
}
