#pragma once
#include <cstdint>
using oflag_t = uint16_t;
constexpr oflag_t O_RDONLY = 0, O_WRITE = 1, O_RDWR = 3, O_CREAT = 16, O_TRUNC = 32, O_APPEND = 64, O_AT_END = 128;
