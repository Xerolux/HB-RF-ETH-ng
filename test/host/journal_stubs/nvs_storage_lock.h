#pragma once
#include "freertos/FreeRTOS.h"
bool journal_test_storage_take(TickType_t);
void journal_test_storage_release();
class NvsStorageLock
{
public:
    explicit NvsStorageLock(TickType_t timeout = portMAX_DELAY, const char * = nullptr)
        : locked_(journal_test_storage_take(timeout))
    {}
    ~NvsStorageLock() { release(); }
    explicit operator bool() const { return locked_; }
    void release()
    {
        if (locked_) {
            journal_test_storage_release();
            locked_ = false;
        }
    }

private:
    bool locked_;
};
