/*
 *  rate_limiter.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  Rate limiting for HTTP endpoints
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 */

#include "rate_limiter.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <arpa/inet.h>

static const char *TAG = "RateLimiter";

typedef struct
{
    int family;
    union
    {
        struct in_addr ip4;
        struct in6_addr ip6;
    } addr;
} client_ip_t;

typedef struct
{
    client_ip_t ip;
    int64_t last_attempt_time;
    uint8_t attempt_count;
    bool is_active;
} rate_limit_entry_t;

static rate_limit_entry_t rate_limit_table[MAX_TRACKED_IPS];
static bool initialized = false;

// Helper to compare IPs
static bool ip_equal(const client_ip_t *a, const client_ip_t *b)
{
    if (a->family != b->family)
    {
        return false;
    }

    if (a->family == AF_INET)
    {
        return a->addr.ip4.s_addr == b->addr.ip4.s_addr;
    }
    else if (a->family == AF_INET6)
    {
        return memcmp(&a->addr.ip6, &b->addr.ip6, sizeof(struct in6_addr)) == 0;
    }

    return false;
}

// Get client IP address from request
static bool get_client_ip(httpd_req_t *req, client_ip_t *out_ip)
{
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd < 0)
    {
        return false;
    }

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_len) != 0)
    {
        return false;
    }

    if (addr.ss_family == AF_INET)
    {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        out_ip->family = AF_INET;
        out_ip->addr.ip4 = s->sin_addr;
        return true;
    }
    else if (addr.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        out_ip->family = AF_INET6;
        out_ip->addr.ip6 = s->sin6_addr;
        return true;
    }

    return false;
}

// Find or create entry for IP address
static rate_limit_entry_t* find_or_create_entry(const client_ip_t *client_ip)
{
    int64_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
    rate_limit_entry_t *oldest_entry = NULL;
    int64_t oldest_time = current_time;

    // First, try to find existing entry
    for (int i = 0; i < MAX_TRACKED_IPS; i++)
    {
        if (rate_limit_table[i].is_active && ip_equal(&rate_limit_table[i].ip, client_ip))
        {
            return &rate_limit_table[i];
        }
    }

    // Try to find an inactive entry
    for (int i = 0; i < MAX_TRACKED_IPS; i++)
    {
        if (!rate_limit_table[i].is_active)
        {
            rate_limit_table[i].ip = *client_ip;
            rate_limit_table[i].last_attempt_time = current_time;
            rate_limit_table[i].attempt_count = 0;
            rate_limit_table[i].is_active = true;
            return &rate_limit_table[i];
        }

        // Track oldest entry for eviction
        if (rate_limit_table[i].last_attempt_time < oldest_time)
        {
            oldest_time = rate_limit_table[i].last_attempt_time;
            oldest_entry = &rate_limit_table[i];
        }
    }

    // Table is full, evict oldest entry
    if (oldest_entry != NULL)
    {
        ESP_LOGW(TAG, "Rate limit table full, evicting oldest entry");
        oldest_entry->ip = *client_ip;
        oldest_entry->last_attempt_time = current_time;
        oldest_entry->attempt_count = 0;
        oldest_entry->is_active = true;
        return oldest_entry;
    }

    return NULL;
}

void rate_limiter_init()
{
    if (initialized)
    {
        return;
    }

    memset(rate_limit_table, 0, sizeof(rate_limit_table));

    for (int i = 0; i < MAX_TRACKED_IPS; i++)
    {
        rate_limit_table[i].is_active = false;
    }

    initialized = true;
    ESP_LOGI(TAG, "Rate limiter initialized (max %d attempts per %d seconds)",
             MAX_LOGIN_ATTEMPTS, RATE_LIMIT_WINDOW_SECONDS);
}

bool rate_limiter_is_whitelisted(httpd_req_t *req, const ip4_addr_t *whitelist_ip)
{
    if (whitelist_ip == NULL || whitelist_ip->addr == IPADDR_ANY || whitelist_ip->addr == IPADDR_NONE) {
        return false;
    }

    client_ip_t client_ip;
    if (!get_client_ip(req, &client_ip))
    {
        return false;
    }

    // Only support IPv4 whitelist for now as CCU connection is usually IPv4
    if (client_ip.family == AF_INET) {
        if (client_ip.addr.ip4.s_addr == whitelist_ip->addr) {
            ESP_LOGD(TAG, "IP whitelisted: %s", ip4addr_ntoa(whitelist_ip));
            return true;
        }
    }

    return false;
}

bool rate_limiter_check_login(httpd_req_t *req)
{
    if (!initialized)
    {
        rate_limiter_init();
    }

    client_ip_t client_ip;
    if (!get_client_ip(req, &client_ip))
    {
        ESP_LOGW(TAG, "Could not determine client IP address");
        return true; // Allow request if we can't determine IP
    }

    rate_limit_entry_t *entry = find_or_create_entry(&client_ip);
    if (entry == NULL)
    {
        ESP_LOGE(TAG, "Could not create rate limit entry");
        return true; // Allow request on error
    }

    int64_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
    int64_t time_since_last = current_time - entry->last_attempt_time;

    // Reset counter if window has passed
    if (time_since_last > RATE_LIMIT_WINDOW_SECONDS)
    {
        entry->attempt_count = 0;
        entry->last_attempt_time = current_time;
    }

    // Check if rate limit exceeded
    if (entry->attempt_count >= MAX_LOGIN_ATTEMPTS)
    {
        int64_t remaining_time = RATE_LIMIT_WINDOW_SECONDS - time_since_last;

        char ip_str[INET6_ADDRSTRLEN];
        void *addr_ptr = (client_ip.family == AF_INET) ? (void *)&client_ip.addr.ip4 : (void *)&client_ip.addr.ip6;

        if (inet_ntop(client_ip.family, addr_ptr, ip_str, sizeof(ip_str)) == NULL) {
            strncpy(ip_str, "unknown", sizeof(ip_str));
        }

        ESP_LOGW(TAG, "Rate limit exceeded for IP %s (wait %lld seconds)",
                 ip_str, remaining_time);

        return false; // Block request
    }

    // Increment attempt counter
    entry->attempt_count++;
    entry->last_attempt_time = current_time;

    return true; // Allow request
}

void rate_limiter_reset_ip(httpd_req_t *req)
{
    if (!initialized)
    {
        return;
    }

    client_ip_t client_ip;
    if (!get_client_ip(req, &client_ip))
    {
        return;
    }

    // Find and reset entry for this IP
    for (int i = 0; i < MAX_TRACKED_IPS; i++)
    {
        if (rate_limit_table[i].is_active && ip_equal(&rate_limit_table[i].ip, &client_ip))
        {
            rate_limit_table[i].attempt_count = 0;
            rate_limit_table[i].last_attempt_time = esp_timer_get_time() / 1000000;

            char ip_str[INET6_ADDRSTRLEN];
            void *addr_ptr = (client_ip.family == AF_INET) ? (void *)&client_ip.addr.ip4 : (void *)&client_ip.addr.ip6;

            if (inet_ntop(client_ip.family, addr_ptr, ip_str, sizeof(ip_str)) == NULL) {
                strncpy(ip_str, "unknown", sizeof(ip_str));
            }

            ESP_LOGD(TAG, "Reset rate limit for IP %s", ip_str);
            break;
        }
    }
}

void rate_limiter_cleanup()
{
    if (!initialized)
    {
        return;
    }

    int64_t current_time = esp_timer_get_time() / 1000000;
    int cleaned = 0;

    for (int i = 0; i < MAX_TRACKED_IPS; i++)
    {
        if (rate_limit_table[i].is_active)
        {
            int64_t age = current_time - rate_limit_table[i].last_attempt_time;

            // Remove entries older than 5 minutes
            if (age > 300)
            {
                rate_limit_table[i].is_active = false;
                cleaned++;
            }
        }
    }

    if (cleaned > 0)
    {
        ESP_LOGD(TAG, "Cleaned %d old rate limit entries", cleaned);
    }
}
