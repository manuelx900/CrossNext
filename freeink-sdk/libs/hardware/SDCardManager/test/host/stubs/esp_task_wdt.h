#pragma once
#include "Arduino.h"
constexpr int ESP_OK = 0;
inline int esp_task_wdt_status(void*) { return watchdogSubscribed ? ESP_OK : -1; }
inline int esp_task_wdt_reset() { ++watchdogResets; return ESP_OK; }
