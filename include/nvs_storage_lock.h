#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Serializes application-owned writes to the shared 16 KiB default NVS
// partition. ESP-IDF makes individual calls thread-safe, but a multi-call
// capacity check/transaction still needs an application-level lock so another
// namespace cannot consume entries between the check and commit.
//
// Recursive semantics let a high-level transaction (for example restore)
// reserve the partition while helpers take the same lock internally.
//
// `tag` names the operation for the crash_blackbox flight recorder (see
// crash_blackbox.h): while this instance holds the lock, the tag is pushed
// onto the RTC-backed op stack and popped again on release/destruction. If a
// reset happens while the tag is still pushed, the next boot can report
// exactly which NVS operation was in flight instead of just "something
// crashed". Pass NULL (the default) to skip tracking for call sites that are
// too hot/uninteresting to name individually.
class NvsStorageLock {
public:
    explicit NvsStorageLock(TickType_t timeout = portMAX_DELAY, const char *tag = nullptr);
    ~NvsStorageLock();

    NvsStorageLock(const NvsStorageLock &) = delete;
    NvsStorageLock &operator=(const NvsStorageLock &) = delete;

    explicit operator bool() const { return locked_; }
    void release();

private:
    SemaphoreHandle_t mutex_;
    bool locked_;
    bool tracked_;
};
