/*
 *  panic_transcript.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  Modified work Copyright 2025 Xerolux
 *
 *  Licensed under CC BY-NC-SA 4.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

// Panic transcript capture (issue #362).
//
// Field devices have no serial console, and the 4 MB flash has no room for
// a coredump partition. Every Interrupt-Watchdog reset therefore arrived
// without the one piece of evidence that names the culprit: the panic
// handler's register dump and backtrace, which ESP-IDF prints to UART0 and
// nobody ever sees.
//
// ESP-IDF's panic handler writes every character through
// uart_hal_write_txfifo(&UART0, ...). That HAL function is wrapped at link
// time (-Wl,--wrap=uart_hal_write_txfifo, see main/CMakeLists.txt); the
// wrapper mirrors everything written to UART0 into an RTC-slow-memory
// buffer that survives the watchdog/panic reboot, then forwards the bytes to
// the real function. Nothing else in this firmware writes UART0 through the
// HAL (the console uses the ROM/VFS path and no UART0 driver is installed),
// so the buffer only ever holds panic output. The wrapper is IRAM-resident
// and touches RTC memory only, which keeps it safe in the cache-disabled
// windows where interrupt-watchdog panics can originate.
//
// At the next boot the RTC copy is latched into RAM first thing, the
// essential lines (Guru Meditation header, PC / EXCVADDR, backtraces of
// both cores) are written to the system log, and the full transcript is
// stored in the WebUI's crash-tail slot ("Crash-Tail laden" in the system
// log page). The backtrace addresses resolve with
//   xtensa-esp32-elf-addr2line -pfiaC -e build/HB-RF-ETH-ng.elf <addr> ...
// against the ELF of the running firmware version.
//
// If a watchdog reset arrives WITHOUT a transcript, that is a finding too:
// the stage-0 interrupt of the interrupt watchdog was never serviced, i.e.
// the stalled core could not take even a level-4 interrupt (hardware-level
// stall rather than a software spin). ResetInfo::getRtcResetCause() tells
// the two apart independently (SW_CPU reset = panic handler ran, TG1WDT_SYS
// reset = hard stage-1 reset).

#define PANIC_TRANSCRIPT_CAPACITY 2048

#ifdef __cplusplus
extern "C" {
#endif

// Copy the RTC transcript (if any) into RAM and release the RTC slot for the
// next crash. Must run before anything else can panic; app_main calls it
// right after arming the tick sentinel.
void panic_transcript_boot_latch(void);

// Text captured before the last reset (NUL-terminated, at most
// PANIC_TRANSCRIPT_CAPACITY bytes) or NULL when the last reset left none.
// `total` receives the number of bytes the panic handler emitted, which can
// exceed the stored length when the transcript was longer than the buffer.
const char *panic_transcript_get(size_t *len, size_t *total);

// Log the essential lines to the system log and persist the whole transcript
// into the crash-tail NVS slot so the WebUI can show it. Call once after NVS
// is initialised. No-op without a transcript.
void panic_transcript_report(void);

#ifdef __cplusplus
}
#endif
