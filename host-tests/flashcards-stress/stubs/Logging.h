#pragma once
#include <cstdio>
#define LOG_ERR(module, fmt, ...) std::fprintf(stderr, "ERR %s: " fmt "\n", module, ##__VA_ARGS__)
#define LOG_INF(module, fmt, ...) ((void)0)
#define LOG_DBG(module, fmt, ...) std::fprintf(stderr, "DBG %s: " fmt "\n", module, ##__VA_ARGS__)
