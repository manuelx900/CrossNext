#pragma once

// Host-test stand-in for <freertos/FreeRTOS.h>: the types and critical-section
// macros the BLE HID host uses. The critical section is a real recursive mutex,
// so a test thread can deliver reports while the connection task runs.

#include <stddef.h>
#include <stdint.h>

typedef void* TaskHandle_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) (ms)

struct portMUX_TYPE {};
#define portMUX_INITIALIZER_UNLOCKED portMUX_TYPE()

void portEnterCritical(portMUX_TYPE* mux);
void portExitCritical(portMUX_TYPE* mux);
#define portENTER_CRITICAL(mux) portEnterCritical(mux)
#define portEXIT_CRITICAL(mux) portExitCritical(mux)
