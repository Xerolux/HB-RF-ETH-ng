#include "ping_service.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
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
    esp_ping_get_profile(hndl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
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
        return -1;
    }
    if (res->ai_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in *)res->ai_addr;
        target_addr.u_addr.ip4.addr = p->sin_addr.s_addr;
        target_addr.type = IPADDR_TYPE_V4;
    } else {
        freeaddrinfo(res);
        return -1; // IPv6 not supported in this simple ping
    }
    freeaddrinfo(res);

    ping_ctx_t ctx;
    ctx.event_group = xEventGroupCreate();
    ctx.latency = -1;

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = target_addr;
    config.count = 1;
    config.timeout_ms = timeout_ms;

    esp_ping_callbacks_t cbs = {
        .cb_args = &ctx,
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end
    };

    esp_ping_handle_t ping = NULL;
    esp_err_t err = esp_ping_new_session(&config, &cbs, &ping);
    if (err != ESP_OK || ping == NULL) {
        ESP_LOGE(TAG, "Failed to create ping session: %s", esp_err_to_name(err));
        vEventGroupDelete(ctx.event_group);
        return -1;
    }

    esp_ping_start(ping);

    EventBits_t bits = xEventGroupWaitBits(ctx.event_group, PING_SUCCESS_BIT | PING_END_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms + 1000));

    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
    vEventGroupDelete(ctx.event_group);

    if (bits & PING_SUCCESS_BIT) {
        return ctx.latency;
    }

    return -1;
}
