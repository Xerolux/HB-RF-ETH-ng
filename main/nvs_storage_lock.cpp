#include "nvs_storage_lock.h"

namespace {

StaticSemaphore_t s_nvs_storage_mutex_buffer;

SemaphoreHandle_t nvs_storage_mutex()
{
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateRecursiveMutexStatic(&s_nvs_storage_mutex_buffer);
    return mutex;
}

} // namespace

NvsStorageLock::NvsStorageLock(TickType_t timeout)
    : mutex_(nvs_storage_mutex()),
      locked_(mutex_ != nullptr &&
              xSemaphoreTakeRecursive(mutex_, timeout) == pdTRUE)
{
}

NvsStorageLock::~NvsStorageLock()
{
    release();
}

void NvsStorageLock::release()
{
    if (locked_) {
        xSemaphoreGiveRecursive(mutex_);
        locked_ = false;
    }
}
