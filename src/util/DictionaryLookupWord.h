#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace dictionaryLookup {

inline constexpr size_t MAX_WORD_BYTES = 192;
struct Fragment {
  const char* text = nullptr;
  uint32_t start = UINT32_MAX;
  uint32_t end = UINT32_MAX;
  bool insertedHyphen = false;
  bool paragraphStart = false;
};
using Neighbor = Fragment (*)(const void*, uint32_t boundary, bool previous);

// Neighbors borrow the retained definition layout, including adjacent pages.
std::string joinWord(Fragment hit, const void* context, Neighbor neighbor);
std::string plainWord(std::string_view definition, size_t offset);

}  // namespace dictionaryLookup
