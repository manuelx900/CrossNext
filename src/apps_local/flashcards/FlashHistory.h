#pragma once
#include "../study/StudyStats.h"
namespace flashcards {
inline constexpr uint8_t kSessionStart = 2;
struct Session {
  uint32_t first = 0, end = 0, cards = 0, answers = 0, fresh = 0, ratings[4]{};
  int64_t started = 0, ended = 0;
  uint64_t tookMs = 0;
  bool hasTiming = false;
};
struct HistoryWork {
  int64_t ids[256]{};
  bool freshSeen[256]{};
  int idCount = 0, pageIndex = -1;
  bool overflow = false;
  uint8_t readCache[1024]{};
  uint8_t record[32]{};
  Session session;
};
bool reviewedCard(study::ByteSource&, const Session&, int index, HistoryWork&, int64_t& id);
class BlockSource final : public study::ByteSource {
  study::ByteSource& source;
  uint8_t* buffer;
  uint32_t capacity, cachedAt = 0, cachedBytes = 0;

 public:
  BlockSource(study::ByteSource& s, uint8_t* b, uint32_t n) : source(s), buffer(b), capacity(n) {}
  uint32_t size() const override { return source.size(); }
  bool read(uint32_t at, void* out, uint32_t bytes) override;
};
using SessionVisitor = void (*)(void*, const Session&);
struct HistoryLimit {
  uint32_t sessions = 0;
  uint32_t records = 0;
};
inline constexpr HistoryLimit kRecentHistoryLimit{1000, 32768};
// Offsets remain aligned to the unchanged 32-byte revlog format.
bool scanHistory(study::ByteSource&, HistoryWork&, SessionVisitor, void*, HistoryLimit = {});
bool summarize(study::ByteSource&, uint32_t first, uint32_t end, HistoryWork&, Session&);
}  // namespace flashcards
