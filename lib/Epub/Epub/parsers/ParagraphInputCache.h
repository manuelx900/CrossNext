#pragma once

#include <HalMemory.h>

#include <atomic>

#include "Epub/blocks/BlockStyle.h"

class ParsedText;

// Exact raw callback comparison guards the reusable normalized paragraph input.
// Layout, relative CSS resolution and pagination are always rerun by the parser.
class ParagraphInputCache {
 public:
  using ProcessText = void (*)(void*, const char*, int);
  enum class Result { Miss, Hit, OutOfMemory };
  ~ParagraphInputCache();
  static void retainReader(bool enabled);
  bool acquire();
  void chapter(const char* path);
  void start(const BlockStyle& style, uint32_t semantics, uint32_t visibleBase);
  bool text(const char* bytes, size_t length, ProcessText process, void* context);
  void cancel(ProcessText process, void* context);
  Result finish(ParsedText& text, uint32_t& visibleOffset, ProcessText process, void* context);
  void capture(const ParsedText& text, uint32_t visibleOffset);
  void release();
  uint32_t hitCount() const { return hits; }

 private:
  static constexpr size_t Capacity = 128 * 1024, ParagraphLimit = 24 * 1024, RawLimit = 8192;
  struct Entry;
  struct Data;
  static HalMemory::PsramBuffer memory;
  static Data* data;
  static std::atomic_flag lease;
  static std::atomic<bool> enabled, releaseRequested;
  static bool attempted;
  enum class Mode { Idle, Probe, Capture, Hit };
  Mode mode = Mode::Idle;
  bool owned = false;
  BlockStyle style;
  uint32_t semantics = 0, visibleBase = 0, hits = 0;
  size_t begin = 0, rawEnd = 0, rawTextBytes = 0, chunks = 0, hit = 0, matched = 0;
  void discard();
  void replay(ProcessText process, void* context);
};
