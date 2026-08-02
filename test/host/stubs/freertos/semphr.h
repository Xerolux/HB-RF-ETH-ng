#pragma once

#include "freertos/FreeRTOS.h"

typedef void *SemaphoreHandle_t;
typedef struct {
    void *opaque[8];
} StaticSemaphore_t;

#ifdef __cplusplus
extern "C" {
#endif

SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage);
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore);

#ifdef __cplusplus
}
#endif
