#pragma once

#include <Epub/ReaderRenderSpec.h>
#include <Epub/Section.h>

#include <cstddef>
#include <cstdint>

namespace bookprep {
inline uint32_t preparationSignature(const ReaderRenderSpec& spec, bool epub, bool rotateXtc, const int* heights,
                                     size_t count) {
  uint32_t key = epub ? (readerRenderSpecSignature(spec) ^ spec.paragraphIndentSpaces) * 16777619U : 2166136261U;
  key = (key ^ Section::cacheVersion()) * 16777619U;
  key = (key ^ rotateXtc) * 16777619U;
  for (size_t n = 0; n < count; ++n) key = (key ^ uint32_t(heights[n])) * 16777619U;
  return key;
}
}  // namespace bookprep
