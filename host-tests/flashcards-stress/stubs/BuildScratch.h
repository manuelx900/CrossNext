#pragma once
#include <cstddef>
#include <cstdint>
namespace buildscratch {
inline uint8_t* claim(size_t) { return nullptr; }
inline void release(uint8_t*) {}
}  // namespace buildscratch
