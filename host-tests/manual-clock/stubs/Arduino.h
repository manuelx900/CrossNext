#pragma once
#include <sys/time.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
inline unsigned long millis() { return 10; }
inline void delay(int) {}
inline void configTzTime(const char*, const char*, const char*) {}
