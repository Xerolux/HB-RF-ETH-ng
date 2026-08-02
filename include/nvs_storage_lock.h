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
class NvsStorageLock {
public:
    explicit NvsStorageLock(TickType_t timeout = portMAX_DELAY);
    ~NvsStorageLock();

    NvsStorageLock(const NvsStorageLock &) = delete;
    NvsStorageLock &operator=(const NvsStorageLock &) = delete;

    explicit operator bool() const { return locked_; }
    void release();

private:
    SemaphoreHandle_t mutex_;
    bool locked_;
};
