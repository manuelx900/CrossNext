#include "ParagraphInputCache.h"

#include <Logging.h>

#include <array>
#include <cstring>
#include <new>

#include "Epub/ParsedText.h"

struct ParagraphInputCache::Entry {
  BlockStyle style;
  uint32_t semantics = 0, raw = 0, rawBytes = 0, tokens = 0, tokenBytes = 0, visibleCount = 0;
  bool valid = false;
};
struct ParagraphInputCache::Data {
  std::array<Entry, 512> entries{};
  char chapter[256]{};
  uint8_t bytes[Capacity];
  size_t used = 0, count = 0;
};
HalMemory::PsramBuffer ParagraphInputCache::memory;
ParagraphInputCache::Data* ParagraphInputCache::data = nullptr;
std::atomic_flag ParagraphInputCache::lease = ATOMIC_FLAG_INIT;
std::atomic<bool> ParagraphInputCache::enabled{false}, ParagraphInputCache::releaseRequested{false};
bool ParagraphInputCache::attempted = false;

ParagraphInputCache::~ParagraphInputCache() { release(); }
void ParagraphInputCache::retainReader(bool value) {
  enabled.store(value);
  releaseRequested.store(!value);
  if (value || lease.test_and_set(std::memory_order_acquire)) return;
  data = nullptr;
  memory.reset();
  attempted = false;
  lease.clear(std::memory_order_release);
}
bool ParagraphInputCache::acquire() {
  if (owned) return true;
  if (!enabled.load() || lease.test_and_set(std::memory_order_acquire)) return false;
  if (!enabled.load()) {
    lease.clear(std::memory_order_release);
    return false;
  }
  if (data && HalMemory::getPsramHeap().freeBytes < 2 * 1024 * 1024) {
    data = nullptr;
    memory.reset();
    attempted = false;
  }
  if (!attempted) {
    attempted = true;
    static_assert(sizeof(Data) <= 192 * 1024);
    // One reader-session arena, <=192 KiB PSRAM; no internal fallback or per-paragraph allocation.
    const auto heap = HalMemory::getPsramHeap();
    if (heap.freeBytes >= sizeof(Data) + 2 * 1024 * 1024 && heap.largestBlockBytes >= sizeof(Data))
      memory = HalMemory::allocatePsram(sizeof(Data));
    if (memory)
      data = new (memory.get()) Data;
    else
      LOG_ERR("LAYOUT", "Paragraph input memo unavailable (%u bytes)", unsigned(sizeof(Data)));
  }
  if (!data) {
    lease.clear(std::memory_order_release);
    return false;
  }
  owned = true;
  return true;
}
void ParagraphInputCache::chapter(const char* path) {
  if (!owned) return;
  discard();
  if (std::strlen(path) >= sizeof(data->chapter)) {
    release();
    return;
  }
  if (!std::strcmp(data->chapter, path)) return;
  data->used = data->count = 0;
  std::strcpy(data->chapter, path);
}
void ParagraphInputCache::discard() {
  if (owned && mode == Mode::Capture) data->used = begin;
  mode = Mode::Idle;
}
void ParagraphInputCache::release() {
  if (!owned) return;
  discard();
  if (releaseRequested.load()) {
    data = nullptr;
    memory.reset();
    attempted = false;
  }
  owned = false;
  lease.clear(std::memory_order_release);
}
void ParagraphInputCache::start(const BlockStyle& value, uint32_t flags, uint32_t base) {
  discard();
  if (!owned) return;
  style = value;
  semantics = flags;
  visibleBase = base;
  chunks = rawTextBytes = matched = 0;
  mode = Mode::Probe;
}
void ParagraphInputCache::replay(ProcessText process, void* context) {
  if (mode != Mode::Hit || !process) return;
  const auto& entry = data->entries[hit];
  size_t at = entry.raw;
  const size_t end = at + matched;
  while (at < end) {
    const size_t length = size_t(data->bytes[at]) | size_t(data->bytes[at + 1]) << 8;
    at += 2;
    process(context, reinterpret_cast<const char*>(data->bytes + at), int(length));
    at += length;
  }
}
void ParagraphInputCache::cancel(ProcessText process, void* context) {
  replay(process, context);
  discard();
}
bool ParagraphInputCache::text(const char* bytes, size_t length, ProcessText process, void* context) {
  if (!owned || mode == Mode::Idle) return false;
  if (mode == Mode::Probe) {
    for (size_t n = 0; n < data->count; ++n) {
      const auto& entry = data->entries[n];
      if (!entry.valid || entry.semantics != semantics || !(entry.style == style) || entry.rawBytes < 2) continue;
      const size_t expected = size_t(data->bytes[entry.raw]) | size_t(data->bytes[entry.raw + 1]) << 8;
      if (expected == length && !std::memcmp(data->bytes + entry.raw + 2, bytes, length)) {
        mode = Mode::Hit;
        hit = n;
        matched = length + 2;
        return true;
      }
    }
    if (data->used > Capacity - ParagraphLimit || data->count == data->entries.size()) {
      discard();
      return false;
    }
    mode = Mode::Capture;
    begin = data->used;
  }
  if (mode == Mode::Hit) {
    const auto& entry = data->entries[hit];
    if (matched + 2 <= entry.rawBytes) {
      const auto at = entry.raw + matched;
      const size_t expected = size_t(data->bytes[at]) | size_t(data->bytes[at + 1]) << 8;
      if (expected == length && length <= entry.rawBytes - matched - 2 &&
          !std::memcmp(data->bytes + at + 2, bytes, length)) {
        matched += length + 2;
        return true;
      }
    }
    cancel(process, context);
    return false;
  }
  if (length > UINT16_MAX || length > RawLimit - rawTextBytes || chunks == 64) {
    discard();
    return false;
  }
  data->bytes[data->used++] = uint8_t(length);
  data->bytes[data->used++] = uint8_t(length >> 8);
  std::memcpy(data->bytes + data->used, bytes, length);
  data->used += length;
  rawTextBytes += length;
  ++chunks;
  rawEnd = data->used;
  return false;
}
ParagraphInputCache::Result ParagraphInputCache::finish(ParsedText& text, uint32_t& visibleOffset, ProcessText process,
                                                        void* context) {
  if (mode != Mode::Hit) return Result::Miss;
  auto& entry = data->entries[hit];
  if (matched == entry.rawBytes) {
    switch (text.restoreInput(data->bytes + entry.tokens, entry.tokenBytes, visibleBase, entry.visibleCount)) {
      case ParsedText::InputRestore::Restored:
        visibleOffset = visibleBase + entry.visibleCount;
        ++hits;
        mode = Mode::Idle;
        return Result::Hit;
      case ParsedText::InputRestore::OutOfMemory:
        mode = Mode::Idle;
        return Result::OutOfMemory;
      case ParsedText::InputRestore::Invalid:
        entry.valid = false;
        break;
      case ParsedText::InputRestore::Unavailable:
        break;
    }
  }
  cancel(process, context);
  return Result::Miss;
}
void ParagraphInputCache::capture(const ParsedText& text, uint32_t visibleOffset) {
  if (mode != Mode::Capture) {
    discard();
    return;
  }
  if (!(text.getInputBlockStyle() == style) || !chunks || visibleOffset < visibleBase) {
    discard();
    return;
  }
  const auto bytes = text.writeInput(data->bytes + data->used, ParagraphLimit - (data->used - begin), visibleBase);
  if (!bytes) {
    discard();
    return;
  }
  auto& entry = data->entries[data->count++];
  entry = {style,
           semantics,
           uint32_t(begin),
           uint32_t(rawEnd - begin),
           uint32_t(data->used),
           uint32_t(bytes),
           visibleOffset - visibleBase,
           true};
  data->used += bytes;
  mode = Mode::Idle;
}
