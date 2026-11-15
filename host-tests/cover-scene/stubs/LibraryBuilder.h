#pragma once
#include <cstdint>
namespace library {
inline uint32_t testRevision = 1;
inline uint32_t libraryContentRevision() { return testRevision; }
}  // namespace library
