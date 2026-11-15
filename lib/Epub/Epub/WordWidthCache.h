#pragma once

#include <HalMemory.h>
#include <Logging.h>

#include <cstring>
#include <limits>
#include <string_view>

#include "hyphenation/Hyphenator.h"

// Exclusively owned or leased by one chapter parser. Exact text comparison resolves hash collisions;
// words exceeding the fixed slot length use ordinary measurement.
class WordWidthCache {
 public:
  struct Key {
    uint64_t font, emphasis;
    int32_t fontId;
    uint8_t style, focus, spacing;
    int8_t tracking;
  };
  bool get(std::string_view text, const Key& key, uint16_t& width) {
    if ((!key.font || (key.focus && !key.emphasis)) || text.empty() || text.size() >= TEXT_BYTES) return false;
    if (!attempted) {
      attempted = true;
      // At most 48 KiB per live parser; never task-stack or internal RAM.
      const auto heap = HalMemory::getPsramHeap();
      if (heap.totalBytes && heap.freeBytes >= SLOTS * sizeof(Entry) + 2 * 1024 * 1024)
        storage = HalMemory::allocatePsram(SLOTS * sizeof(Entry));
      if (!storage)
        LOG_ERR("LAYOUT", "Word metrics cache unavailable; measuring directly");
      else
        std::memset(storage.get(), 0, SLOTS * sizeof(Entry));
    }
    if (!storage) return false;
    const auto& entry = entries()[slot(text, key)];
    if (!same(entry.key, key) || entry.length != text.size() || std::memcmp(entry.text, text.data(), text.size()))
      return false;
    width = entry.width;
    ++hits;
    return true;
  }
  void put(std::string_view text, const Key& key, uint16_t width) {
    if (!storage || (!key.font || (key.focus && !key.emphasis)) || text.empty() || text.size() >= TEXT_BYTES) return;
    auto& entry = entries()[slot(text, key)];
    entry.key = key;
    entry.width = width;
    entry.length = static_cast<uint8_t>(text.size());
    std::memcpy(entry.text, text.data(), text.size());
  }
  uint32_t hitCount() const { return hits; }
  bool getPrefix(std::string_view text, const Key& key, bool hyphen, uint16_t& width) {
    if (!key.font || (key.focus && !key.emphasis) || text.empty() || text.size() >= TEXT_BYTES) return false;
    if (!prefixAttempted) {
      prefixAttempted = true;
      constexpr size_t bytes = PREFIX_SLOTS * sizeof(Entry);
      if (HalMemory::getPsramHeap().freeBytes >= bytes + 2 * 1024 * 1024)
        prefixStorage = HalMemory::allocatePsram(bytes);
      if (prefixStorage)
        std::memset(prefixStorage.get(), 0, bytes);
      else
        LOG_ERR("LAYOUT", "Prefix memo unavailable (%u bytes)", unsigned(bytes));
    }
    if (!prefixStorage) return false;
    const auto& entry = prefixEntries()[prefixSlot(text, key, hyphen)];
    if (!same(entry.key, key) || entry.length != text.size() || std::memcmp(entry.text, text.data(), text.size()))
      return false;
    width = entry.width;
    ++prefixHits;
    return true;
  }
  void putPrefix(std::string_view text, const Key& key, bool hyphen, uint16_t width) {
    if (!prefixStorage || !key.font || (key.focus && !key.emphasis) || text.empty() || text.size() >= TEXT_BYTES)
      return;
    auto& entry = prefixEntries()[prefixSlot(text, key, hyphen)];
    entry.key = key;
    entry.width = width;
    entry.length = uint8_t(text.size());
    std::memcpy(entry.text, text.data(), text.size());
  }
  uint32_t prefixHitCount() const { return prefixHits; }
  bool getBreaks(const std::string& word, bool fallback, std::vector<Hyphenator::BreakInfo>& output) {
    if (!breaksAttempted) {
      breaksAttempted = true;
      constexpr size_t bytes = sizeof(BreakEntry) * BREAK_SLOTS;
      if (HalMemory::getPsramHeap().freeBytes >= bytes + 2 * 1024 * 1024)
        breakStorage = HalMemory::allocatePsram(bytes);
      if (breakStorage)
        std::memset(breakStorage.get(), 0, bytes);
      else
        LOG_ERR("LAYOUT", "Break memo unavailable (%u bytes)", unsigned(bytes));
    }
    if (!breakStorage || word.empty() || word.size() >= TEXT_BYTES) return false;
    const auto language = Hyphenator::cacheIdentity();
    for (size_t n = 0; n < BREAK_SLOTS; ++n) {
      const auto& entry = breakEntries()[n];
      if (entry.length != word.size() || entry.language != language || entry.fallback != fallback ||
          std::memcmp(entry.text, word.data(), word.size()))
        continue;
      output.clear();
      output.reserve(entry.count);
      for (size_t i = 0; i < entry.count; ++i)
        output.push_back({entry.offsets[i], bool(entry.inserted & (uint32_t(1) << i))});
      ++breakHits;
      return true;
    }
    return false;
  }
  void putBreaks(const std::string& word, bool fallback, const std::vector<Hyphenator::BreakInfo>& values) {
    if (!breakStorage || word.empty() || word.size() >= TEXT_BYTES || values.size() > BREAK_OFFSETS) return;
    auto& entry = breakEntries()[nextBreak++ % BREAK_SLOTS];
    entry = {};
    entry.length = uint8_t(word.size());
    entry.language = Hyphenator::cacheIdentity();
    entry.fallback = fallback;
    entry.count = uint8_t(values.size());
    std::memcpy(entry.text, word.data(), word.size());
    for (size_t n = 0; n < values.size(); ++n) {
      if (values[n].byteOffset >= word.size()) {
        entry.length = 0;
        return;
      }
      entry.offsets[n] = uint8_t(values[n].byteOffset);
      if (values[n].requiresInsertedHyphen) entry.inserted |= uint32_t(1) << n;
    }
  }
  uint32_t breakHitCount() const { return breakHits; }
  static constexpr size_t MAX_PARAGRAPH_GAPS = 4096;
  int32_t* resetParagraphGaps(size_t count) {
    if (!count || count > MAX_PARAGRAPH_GAPS) return nullptr;
    if (!gapsAttempted) {
      gapsAttempted = true;
      // Reused by this parser: 16 KiB is too large for the task stack.
      constexpr size_t bytes = MAX_PARAGRAPH_GAPS * sizeof(int32_t);
      const auto heap = HalMemory::getPsramHeap();
      if (heap.totalBytes && heap.freeBytes >= bytes + 2 * 1024 * 1024) gaps = HalMemory::allocatePsram(bytes);
      if (!gaps) LOG_ERR("LAYOUT", "Paragraph metrics unavailable; measuring directly");
    }
    auto* values = reinterpret_cast<int32_t*>(gaps.get());
    if (values)
      for (size_t i = 0; i < count; ++i) values[i] = std::numeric_limits<int32_t>::min();
    return values;
  }

 private:
  static constexpr size_t SLOTS = 512, TEXT_BYTES = 64;
  static constexpr size_t PREFIX_SLOTS = 64;
  static constexpr size_t BREAK_SLOTS = 16, BREAK_OFFSETS = 32;
  struct BreakEntry {
    uintptr_t language;
    uint32_t inserted;
    char text[TEXT_BYTES];
    uint8_t offsets[BREAK_OFFSETS];
    uint8_t length, count;
    bool fallback;
  };
  static_assert(BREAK_SLOTS * sizeof(BreakEntry) <= 2048);
  BreakEntry* breakEntries() { return reinterpret_cast<BreakEntry*>(breakStorage.get()); }
  struct Entry {
    Key key;
    uint16_t width;
    uint8_t length;
    char text[TEXT_BYTES];
  };
  static_assert(SLOTS * sizeof(Entry) <= 48 * 1024);
  static_assert(PREFIX_SLOTS * sizeof(Entry) <= 6 * 1024);
  static bool same(const Key& a, const Key& b) {
    return a.font == b.font && a.emphasis == b.emphasis && a.fontId == b.fontId && a.style == b.style &&
           a.focus == b.focus && a.spacing == b.spacing && a.tracking == b.tracking;
  }
  static size_t slot(std::string_view text, const Key& key) {
    uint32_t hash = static_cast<uint32_t>(key.font) ^ static_cast<uint32_t>(key.font >> 32) ^ key.fontId ^
                    static_cast<uint32_t>(key.emphasis) ^ static_cast<uint32_t>(key.emphasis >> 32);
    hash = (hash ^ key.style ^ (uint32_t(key.focus) << 8) ^ (uint32_t(key.spacing) << 16) ^
            (uint32_t(static_cast<uint8_t>(key.tracking)) << 24)) *
           16777619U;
    for (const unsigned char byte : text) hash = (hash ^ byte) * 16777619U;
    return hash & (SLOTS - 1);
  }
  Entry* entries() { return reinterpret_cast<Entry*>(storage.get()); }
  Entry* prefixEntries() { return reinterpret_cast<Entry*>(prefixStorage.get()); }
  static size_t prefixSlot(std::string_view text, const Key& key, bool hyphen) {
    return (hyphen ? PREFIX_SLOTS / 2 : 0) + (slot(text, key) & (PREFIX_SLOTS / 2 - 1));
  }
  HalMemory::PsramBuffer storage;
  HalMemory::PsramBuffer gaps;
  // Separate 6 KiB bank preserves all eight renderer style bits and the hyphen domain.
  HalMemory::PsramBuffer prefixStorage;
  bool prefixAttempted = false;
  uint32_t prefixHits = 0;
  // At most 2 KiB, parser-owned and reused; too large for the task stack.
  HalMemory::PsramBuffer breakStorage;
  bool breaksAttempted = false;
  uint32_t breakHits = 0, nextBreak = 0;
  bool attempted = false;
  bool gapsAttempted = false;
  uint32_t hits = 0;
};

// One session cache can be borrowed by one parser; nested/preparation parsers use local scratch.
class WordWidthCacheLease {
 public:
  ~WordWidthCacheLease();
  static void retainReader(bool enabled);
  void acquire();
  void release();
  WordWidthCache* cache();

 private:
  WordWidthCache local;
  bool owned = false;
};
