#pragma once

#include <expat.h>

#ifdef BOARD_HAS_PSRAM
#include <HalMemory.h>
#endif

// Expat owns and frees its variable-sized chapter buffers through this suite.
// PSRAM-only allocation leaves internal memory available to the radio and tasks.
inline XML_Parser createChapterXmlParser() {
#ifdef BOARD_HAS_PSRAM
  static constexpr XML_Memory_Handling_Suite memory = {
      [](size_t bytes) -> void* { return HalMemory::allocatePsram(bytes).release(); }, &HalMemory::reallocatePsram,
      [](void* buffer) { HalMemory::PsramDeleter{}(static_cast<uint8_t*>(buffer)); }};
  return XML_ParserCreate_MM(nullptr, &memory, nullptr);
#else
  return XML_ParserCreate(nullptr);
#endif
}
