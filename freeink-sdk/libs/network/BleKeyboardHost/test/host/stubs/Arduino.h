#pragma once

// Host-test stand-in for <Arduino.h>: the fake clock and a silent Serial.
// millis() only moves when a test advances it or the library calls vTaskDelay(),
// so timeouts in poll() and end() run deterministically and fast.

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t byte;

unsigned long millis();

struct SerialStub {
  void print(char) {}
  void print(const char*) {}
  void print(int) {}
  void println(const char* = "") {}
  void printf(const char*, ...) {}
};

extern SerialStub Serial;
