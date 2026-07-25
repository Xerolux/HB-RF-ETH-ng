#include "ping_service.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "PingService";

typedef struct {
    EventGroupHandle_t event_group;
    int latency;
} ping_ctx_t;

#define PING_SUCCESS_BIT BIT0
#define PING_END_BIT     BIT1

static void cmd_ping_on_ping_success(esp_ping_handle_t hndl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    uint32_t elapsed_time;
    esp_ping_get_profile(hndl, ESP_PING_PROF_DURATION, &elapsed_time, sizeof(elapsed_time));
    ctx->latency = elapsed_time;
    xEventGroupSetBits(ctx->event_group, PING_SUCCESS_BIT);
}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hndl, void *args)
{
    // Ignore timeout, handled by PING_END_BIT or timeout in wait
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hndl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    xEventGroupSetBits(ctx->event_group, PING_END_BIT);
}

int ping_service_ping(const char* target, uint32_t timeout_ms)
{
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_INET;
    struct addrinfo *res = NULL;
    if (getaddrinfo(target, NULL, &hint, &res) != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed for %s", target);
        return PING_SERVICE_DNS_ERROR;
    }
    if (res->ai_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in *)res->ai_addr;
        target_addr.u_addr.ip4.addr = p->sin_addr.s_addr;
        target_addr.type = IPADDR_TYPE_V4;
    } else {
        freeaddrinfo(res);
        return PING_SERVICE_DNS_ERROR; // IPv6 is not supported by this endpoint
    }
    freeaddrinfo(res);

    // The ESP-IDF ping worker invokes callbacks asynchronously. Keep the
    // callback context on the heap so an abnormal worker shutdown can never
    // turn the HTTP handler's stack frame into a use-after-free.
    ping_ctx_t *ctx = (ping_ctx_t *)calloc(1, sizeof(ping_ctx_t));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ping callback context");
        return PING_SERVICE_INTERNAL;
    }
    ctx->event_group = xEventGroupCreate();
    ctx->latency = -1;
    if (ctx->event_group == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ping event group");
        free(ctx);
        return PING_SERVICE_INTERNAL;
    }

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = target_addr;
    config.count = 1;
    // There is no next packet in a one-shot session. Avoid the default
    // one-second interval delay before on_ping_end is delivered.
    config.interval_ms = 0;
    config.timeout_ms = timeout_ms;

    esp_ping_callbacks_t cbs = {
        .cb_args = ctx,
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end
    };

    esp_ping_handle_t ping = NULL;
    esp_err_t err = esp_ping_new_session(&config, &cbs, &ping);
    if (err != ESP_OK || ping == NULL) {
        ESP_LOGE(TAG, "Failed to create ping session: %s", esp_err_to_name(err));
        vEventGroupDelete(ctx->event_group);
        free(ctx);
        return PING_SERVICE_INTERNAL;
    }

    err = esp_ping_start(ping);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start ping session: %s", esp_err_to_name(err));
        esp_ping_delete_session(ping);
        vEventGroupDelete(ctx->event_group);
        free(ctx);
        return PING_SERVICE_INTERNAL;
    }

    // Do not return on PING_SUCCESS_BIT. In ESP-IDF, on_ping_success runs
    // before on_ping_end. The old "success OR end" wait resumed immediately,
    // deleted this event group and returned its stack callback context, then
    // the ping task called on_ping_end with both objects already invalid. That
    // deterministic use-after-free caused the interrupt-watchdog reboot seen
    // in #393 and left the CCU/radio session disconnected.
    EventBits_t bits = xEventGroupWaitBits(
        ctx->event_group, PING_END_BIT, pdTRUE, pdTRUE,
        pdMS_TO_TICKS(timeout_ms + 1000));

    if ((bits & PING_END_BIT) == 0) {
        // Defensive cancellation path. A normal one-shot always emits END,
        // but if the worker is delayed, stop it and keep the callback state
        // alive until the final callback has actually completed.
        ESP_LOGW(TAG, "Ping worker did not finish in time; requesting stop");
        esp_ping_stop(ping);
        bits = xEventGroupWaitBits(
            ctx->event_group, PING_END_BIT, pdTRUE, pdTRUE,
            pdMS_TO_TICKS(timeout_ms + 1000));
    }

    const int latency = ctx->latency;
    esp_ping_delete_session(ping);
    if ((bits & PING_END_BIT) == 0) {
        // The ESP-IDF task may still deliver on_ping_end asynchronously.
        // Preserve the small callback context rather than freeing memory that
        // the worker can still access. This exceptional bounded leak is safer
        // than corrupting the radio/network tasks or rebooting the device.
        ESP_LOGE(TAG, "Ping worker did not acknowledge shutdown");
        return PING_SERVICE_INTERNAL;
    }

    vEventGroupDelete(ctx->event_group);
    free(ctx);

    if (latency >= 0) {
        return latency;
    }

    return PING_SERVICE_TIMEOUT;
}
