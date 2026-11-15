#pragma once

#include <cstddef>
#include <cstdint>

namespace performance {
enum class Operation : uint8_t { Navigation, Format, PageTurn };
enum class Stage : uint8_t { Enter, Leave, Font, Metadata, Layout, PageLoad, Extract, Decode, Draw };
enum class Counter : uint8_t {
  LayoutHit,
  LayoutMiss,
  PagePrefetchHit,
  PagePrefetchMiss,
  ImageRamHit,
  ImageDiskHit,
  ImageDecode,
  EventMemoHit,
  EventMemoMiss,
  CoverSourceReuse,
  CoverSourceRead,
  CoverSceneHit,
  CoverSceneMiss,
  PageReuseHit,
  PageReuseMiss,
  CoverPackHit,
  Count
};
enum class Io : uint8_t { Read, Write };

struct Stamp {
  uint64_t at = 0;
  uint32_t operation = 0;
};

#ifdef CROSSPOINT_PERFORMANCE_TRACE
uint64_t timeMicros();
uint32_t begin(Operation operation, const void* owner, uint64_t requestedAt = 0);
uint32_t current();
void finish(uint32_t operation, const void* owner);
Stamp stamp();
void count(Counter counter);
void span(Stamp start, Stage stage);
void storageWait(Stamp start);
void storageIo(Stamp start, Io kind, size_t bytes);
#else
inline uint64_t timeMicros() { return 0; }
inline uint32_t begin(Operation, const void*, uint64_t = 0) { return 0; }
inline uint32_t current() { return 0; }
inline void finish(uint32_t, const void*) {}
inline Stamp stamp() { return {}; }
inline void count(Counter) {}
inline void span(Stamp, Stage) {}
inline void storageWait(Stamp) {}
inline void storageIo(Stamp, Io, size_t) {}
#endif

class Scope {
  Stamp start;
  Stage stage;

 public:
  explicit Scope(Stage stage) : start(stamp()), stage(stage) {}
  ~Scope() { span(start, stage); }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
};
}  // namespace performance
