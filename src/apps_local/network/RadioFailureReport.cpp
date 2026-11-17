#include "RadioFailureReport.h"

#include <Arduino.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

namespace radiofailure {
namespace {
constexpr const char* PATH = "/.crosspoint/radio-diagnostics.txt";
constexpr size_t MAX_BYTES = 4096;
}  // namespace

void write(const char* operation, const int code, const int detail) {
  const auto internal = HalMemory::getInternalHeap();
  const auto psram = HalMemory::getPsramHeap();
  char line[144];
  const int length = std::snprintf(
      line, sizeof(line), "ms=%lu fw=" CROSSPOINT_VERSION " op=%s code=%d detail=%d internal=%u largest=%u psram=%u\n",
      static_cast<unsigned long>(millis()), operation, code, detail, static_cast<unsigned>(internal.freeBytes),
      static_cast<unsigned>(internal.largestBlockBytes), static_cast<unsigned>(psram.freeBytes));
  if (length <= 0) return;
  const auto bytes = std::min(static_cast<size_t>(length), sizeof(line) - 1);
  HalFile file;
  if (!Storage.openFileForAppend("RADIO", PATH, file)) return;
  if (file.size() + bytes > MAX_BYTES) {
    file.close();
    if (!Storage.openFileForWrite("RADIO", PATH, file)) return;
  }
  if (file.write(reinterpret_cast<const uint8_t*>(line), bytes) != bytes)
    LOG_ERR("RADIO", "Unable to write failure report");
}
}  // namespace radiofailure
