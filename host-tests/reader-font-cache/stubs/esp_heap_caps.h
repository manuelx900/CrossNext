#pragma once
#include <cstddef>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_8BIT 4
inline size_t heap_caps_get_largest_free_block(unsigned) { return 8 * 1024 * 1024; }
inline size_t heap_caps_get_free_size(unsigned) { return 8 * 1024 * 1024; }
