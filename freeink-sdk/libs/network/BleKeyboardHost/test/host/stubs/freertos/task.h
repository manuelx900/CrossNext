#pragma once

// Host-test stand-in for <freertos/task.h>. xTaskCreate runs the connection task
// on a std::thread, so connect() stays asynchronous as on the device.
// vTaskDelete() records whether the task was parked inside a NimBLE wait when it
// was deleted, which on the device leaves NimBLE holding a dead task handle.

#include <freertos/FreeRTOS.h>
#include <stdint.h>

BaseType_t xTaskCreate(void (*fn)(void*), const char* name, uint32_t stackDepth, void* arg, UBaseType_t priority,
                       TaskHandle_t* outHandle);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(uint32_t ticks);
void xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(BaseType_t clearOnExit, uint32_t ticksToWait);
