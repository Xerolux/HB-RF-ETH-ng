#include "nvs_storage_lock.h"
#include "crash_blackbox.h"

namespace {

StaticSemaphore_t s_nvs_storage_mutex_buffer;

SemaphoreHandle_t nvs_storage_mutex()
{
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateRecursiveMutexStatic(&s_nvs_storage_mutex_buffer);
    return mutex;
}

} // namespace

NvsStorageLock::NvsStorageLock(TickType_t timeout, const char *tag)
    : mutex_(nvs_storage_mutex()),
      locked_(mutex_ != nullptr &&
              xSemaphoreTakeRecursive(mutex_, timeout) == pdTRUE),
      tracked_(false)
{
    if (locked_ && tag != nullptr) {
        crash_blackbox_nvs_op_begin(tag);
        tracked_ = true;
    }
}

NvsStorageLock::~NvsStorageLock()
{
    release();
}

void NvsStorageLock::release()
{
    if (locked_) {
        if (tracked_) {
            crash_blackbox_nvs_op_end();
            tracked_ = false;
        }
        xSemaphoreGiveRecursive(mutex_);
        locked_ = false;
    }
}
