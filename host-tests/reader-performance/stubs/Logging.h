#pragma once
#ifdef CROSSPOINT_PERFORMANCE_TRACE
void logPrintf(const char*, const char*, ...);
#define LOG_INF(origin, ...) logPrintf(origin, __VA_ARGS__)
#else
#define LOG_INF(...) ((void)0)
#endif
#define LOG_ERR(...) ((void)0)
#define LOG_DBG(...) ((void)0)
