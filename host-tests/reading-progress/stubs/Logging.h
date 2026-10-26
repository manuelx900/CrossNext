#pragma once
inline void testLog(const char*, const char*, ...) {}
#define LOG_ERR(...) testLog(__VA_ARGS__)
#define LOG_DBG(...) testLog(__VA_ARGS__)
