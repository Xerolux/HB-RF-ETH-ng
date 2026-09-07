/*
 *  panic_transcript.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  Modified work Copyright 2025 Xerolux
 *
 *  Licensed under CC BY-NC-SA 4.0
 */

#include "panic_transcript.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "hal/uart_hal.h"
#include "nvs.h"
#include "nvs_storage_lock.h"
#include "soc/uart_struct.h"

#include <stdio.h>
#include <string.h>
#include <string>

static const char *TAG = "PanicLog";

#define PANIC_TRANSCRIPT_MAGIC 0x9A41C7C5u

// Same NVS slot LogManager::saveCrashTailNvs() uses for the pre-restart log
// tail: /api/crash_log serves it and erases it after the first read, and the
// WebUI's system-log page already knows how to display it. After a panic the
// RAM log tail is gone anyway, so the panic transcript is the crash tail.
static const char *CRASH_TAIL_NVS_NS  = "reset_info";
static const char *CRASH_TAIL_NVS_KEY = "clog";

typedef struct {
    uint32_t magic;   // PANIC_TRANSCRIPT_MAGIC while a capture is in progress
    uint32_t len;     // bytes stored in buf
    uint32_t total;   // bytes offered by the panic handler (>= len on overflow)
    char     buf[PANIC_TRANSCRIPT_CAPACITY];
} panic_transcript_rtc_t;

// RTC slow memory: preserved across software, watchdog and panic resets,
// only lost on power-off. ~2 KB of the 8 KB segment; the crash black box
// uses another ~190 bytes.
static RTC_NOINIT_ATTR panic_transcript_rtc_t s_rtc;

// RAM copy latched at boot (the RTC slot is released immediately so the
// next crash starts a fresh capture).
static char   s_latched[PANIC_TRANSCRIPT_CAPACITY + 1];
static size_t s_latched_len = 0;
static size_t s_latched_total = 0;
static bool   s_have_transcript = false;

extern "C" void __real_uart_hal_write_txfifo(uart_hal_context_t *hal, const uint8_t *buf,
                                             uint32_t data_size, uint32_t *write_size);

// Link-time wrapper for every uart_hal_write_txfifo() call in the image
// (main/CMakeLists.txt adds -Wl,--wrap=uart_hal_write_txfifo). Two callers
// exist: the UART driver's TX path (UART1, the radio module — passed
// through untouched) and the panic handler's console sink (UART0 — mirrored
// into RTC memory). Runs in the panic context: IRAM only, no logging, no
// locks, no flash-resident data. `&UART0` is a link-time constant.
extern "C" void IRAM_ATTR __wrap_uart_hal_write_txfifo(uart_hal_context_t *hal, const uint8_t *buf,
                                                       uint32_t data_size, uint32_t *write_size)
{
    __real_uart_hal_write_txfifo(hal, buf, data_size, write_size);

    if (hal == NULL || buf == NULL || write_size == NULL || hal->dev != &UART0) {
        return;
    }

    if (s_rtc.magic != PANIC_TRANSCRIPT_MAGIC) {
        s_rtc.magic = PANIC_TRANSCRIPT_MAGIC;
        s_rtc.len = 0;
        s_rtc.total = 0;
    }
    const uint32_t written = *write_size;
    for (uint32_t i = 0; i < written; i++) {
        if (s_rtc.len < PANIC_TRANSCRIPT_CAPACITY) {
            s_rtc.buf[s_rtc.len++] = (char)buf[i];
        }
        s_rtc.total++;
    }
}

void panic_transcript_boot_latch(void)
{
    if (s_rtc.magic == PANIC_TRANSCRIPT_MAGIC && s_rtc.len > 0 &&
        s_rtc.len <= PANIC_TRANSCRIPT_CAPACITY) {
        memcpy(s_latched, s_rtc.buf, s_rtc.len);
        s_latched[s_rtc.len] = '\0';
        s_latched_len = s_rtc.len;
        s_latched_total = s_rtc.total;
        s_have_transcript = true;
    }
    s_rtc.magic = 0;
    s_rtc.len = 0;
    s_rtc.total = 0;
}

const char *panic_transcript_get(size_t *len, size_t *total)
{
    if (!s_have_transcript) {
        if (len) *len = 0;
        if (total) *total = 0;
        return NULL;
    }
    if (len) *len = s_latched_len;
    if (total) *total = s_latched_total;
    return s_latched;
}

// Lines worth putting into the (small) system log ring buffer: the panic
// header, the register lines carrying PC / EXCVADDR, and the backtraces.
// The rest of the register dump goes to NVS only.
static bool is_essential_line(const char *line)
{
    static const char *const prefixes[] = {
        "Guru Meditation", "Core ", "PC ", "EXCCAUSE", "Backtrace",
        "abort()", "assert", "Panic", "ELF file", "Debug exception",
    };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        if (strncmp(line, prefixes[i], strlen(prefixes[i])) == 0) return true;
    }
    return false;
}

// The log capture path formats into a 256-byte line buffer; a backtrace with
// many frames is longer than that. Emit long lines in pieces split at
// spaces so no address gets cut in half.
static void log_line_chunked(const char *line, size_t line_len)
{
    static const size_t CHUNK = 150;
    size_t pos = 0;
    bool first = true;
    while (pos < line_len) {
        size_t take = line_len - pos;
        if (take > CHUNK) {
            take = CHUNK;
            // Back up to the last space inside the chunk.
            size_t cut = take;
            while (cut > 0 && line[pos + cut] != ' ') cut--;
            if (cut > 0) take = cut;
        }
        ESP_LOGW(TAG, "%s%.*s", first ? "  " : "  .. ", (int)take, line + pos);
        first = false;
        pos += take;
        while (pos < line_len && line[pos] == ' ') pos++;
    }
}

static void persist_to_crash_tail(void)
{
    std::string blob;
    blob.reserve(s_latched_len + 64);
    blob.append("[panic transcript] ");
    if (s_latched_total > s_latched_len) {
        char note[64];
        snprintf(note, sizeof(note), "(first %u of %u bytes) ",
                 (unsigned)s_latched_len, (unsigned)s_latched_total);
        blob.append(note);
    }
    blob.append("\n");
    blob.append(s_latched, s_latched_len);

    NvsStorageLock storage_lock(pdMS_TO_TICKS(500), "panic_transcript.save");
    if (!storage_lock) {
        ESP_LOGW(TAG, "Could not reserve NVS to persist the panic transcript");
        return;
    }
    nvs_handle_t h;
    if (nvs_open(CRASH_TAIL_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "Could not open NVS to persist the panic transcript");
        return;
    }
    esp_err_t err = nvs_set_blob(h, CRASH_TAIL_NVS_KEY, blob.data(), blob.size());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Persisting the panic transcript failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Full panic transcript stored; available as crash tail in the WebUI (/api/crash_log)");
    }
}

void panic_transcript_report(void)
{
    if (!s_have_transcript) return;

    // A power-on wipes RTC memory, so a transcript can only be stale if the
    // magic survived by accident; the reset-reason gate below also keeps a
    // hypothetical non-panic UART0 HAL write from being reported as a crash.
    const esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_POWERON || reason == ESP_RST_DEEPSLEEP) {
        s_have_transcript = false;
        return;
    }

    ESP_LOGW(TAG, "Panic transcript captured before the last reset (%u of %u bytes). "
                  "Resolve addresses with xtensa-esp32-elf-addr2line -pfiaC -e HB-RF-ETH-ng.elf",
             (unsigned)s_latched_len, (unsigned)s_latched_total);

    const char *p = s_latched;
    const char *end = s_latched + s_latched_len;
    int emitted = 0;
    const int max_lines = 16;
    while (p < end && emitted < max_lines) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        size_t line_len = nl ? (size_t)(nl - p) : (size_t)(end - p);
        // Strip trailing CR / spaces.
        while (line_len > 0 && (p[line_len - 1] == '\r' || p[line_len - 1] == ' ')) line_len--;
        if (line_len > 0 && is_essential_line(p)) {
            log_line_chunked(p, line_len);
            emitted++;
        }
        if (!nl) break;
        p = nl + 1;
    }

    persist_to_crash_tail();
}
