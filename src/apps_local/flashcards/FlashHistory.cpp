#include "FlashHistory.h"

#include <algorithm>
#include <cstring>
namespace flashcards {
bool BlockSource::read(uint32_t at, void* out, uint32_t bytes) {
  if (uint64_t(at) + bytes > size()) return false;
  if (!capacity || bytes > capacity) return source.read(at, out, bytes);
  if (at < cachedAt || uint64_t(at) + bytes > uint64_t(cachedAt) + cachedBytes) {
    const uint32_t nextAt = at / capacity * capacity, nextBytes = std::min(capacity, size() - nextAt);
    if (uint64_t(at) + bytes > uint64_t(nextAt) + nextBytes) return source.read(at, out, bytes);
    if (!source.read(nextAt, buffer, nextBytes)) {
      cachedBytes = 0;
      return false;
    }
    cachedAt = nextAt;
    cachedBytes = nextBytes;
  }
  std::memcpy(out, buffer + at - cachedAt, bytes);
  return true;
}
namespace {
template <class T>
T number(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof(v));
  return v;
}
bool live(const uint8_t* r) {
  return number<int64_t>(r + 8) / 1000 >= study::kClockFloor && number<int64_t>(r) != 0 && r[16] >= 1 && r[16] <= 4 &&
         !(r[28] & study::kRevlogVoided);
}
void reset(HistoryWork& w) {
  w.idCount = 0;
  w.pageIndex = -1;
  w.overflow = false;
  w.session = {};
}
void add(HistoryWork& w, uint32_t offset) {
  const auto* r = w.record;
  if (!live(r)) return;
  auto& s = w.session;
  const int64_t id = number<int64_t>(r), at = number<int64_t>(r + 8);
  const bool fresh = r[17] == 0;
  if (!s.answers) {
    s.started = s.ended = at;
    s.end = offset + 32;
  }
  s.first = offset;
  s.started = std::min(s.started, at);
  s.ended = std::max(s.ended, at);
  ++s.answers;
  ++s.ratings[r[16] - 1];
  const uint32_t took = number<uint32_t>(r + 24);
  s.tookMs += took;
  s.hasTiming |= took != 0;
  for (int n = 0; n < w.idCount; ++n)
    if (w.ids[n] == id) {
      if (fresh && !w.freshSeen[n]) {
        ++s.fresh;
        w.freshSeen[n] = true;
      }
      return;
    }
  if (w.idCount == 256) {
    w.overflow = true;
    return;
  }
  w.ids[w.idCount] = id;
  w.freshSeen[w.idCount++] = fresh;
  ++s.cards;
  s.fresh += fresh;
}
// Keep the greatest 256 IDs below the ceiling. Repeated records update their
// existing slot, including whether any response introduced that card as new.
bool window(study::ByteSource& source, const Session& session, HistoryWork& w, bool bounded, int64_t ceiling) {
  w.idCount = 0;
  for (uint32_t p = session.first; p < session.end; p += 32) {
    if (!source.read(p, w.record, 32)) return false;
    if (!live(w.record)) continue;
    const int64_t id = number<int64_t>(w.record);
    if (bounded && id >= ceiling) continue;
    int at = int(std::lower_bound(w.ids, w.ids + w.idCount, id) - w.ids);
    if (at < w.idCount && w.ids[at] == id) {
      w.freshSeen[at] |= w.record[17] == 0;
      continue;
    }
    if (w.idCount == 256) {
      if (!at) continue;
      std::move(w.ids + 1, w.ids + 256, w.ids);
      std::move(w.freshSeen + 1, w.freshSeen + 256, w.freshSeen);
      --w.idCount;
      --at;
    }
    std::move_backward(w.ids + at, w.ids + w.idCount, w.ids + w.idCount + 1);
    std::move_backward(w.freshSeen + at, w.freshSeen + w.idCount, w.freshSeen + w.idCount + 1);
    w.ids[at] = id;
    w.freshSeen[at] = w.record[17] == 0;
    ++w.idCount;
  }
  return true;
}
bool finish(study::ByteSource& source, HistoryWork& w) {
  if (!w.overflow) return true;
  w.session.cards = w.session.fresh = 0;
  int64_t ceiling = 0;
  bool bounded = false;
  int page = 0;
  do {
    if (!window(source, w.session, w, bounded, ceiling)) return false;
    w.session.cards += w.idCount;
    for (int n = 0; n < w.idCount; ++n) w.session.fresh += w.freshSeen[n];
    w.pageIndex = page++;
    if (w.idCount < 256) break;
    ceiling = w.ids[0];
    bounded = true;
  } while (true);
  return true;
}
bool emit(study::ByteSource& source, HistoryWork& w, SessionVisitor visit, void* ctx) {
  if (!w.session.answers) return true;
  if (!finish(source, w)) return false;
  visit(ctx, w.session);
  return true;
}
}  // namespace
bool scanHistory(study::ByteSource& source, HistoryWork& work, SessionVisitor visit, void* ctx, HistoryLimit limit) {
  reset(work);
  const uint32_t end = source.size() / 32 * 32;
  const uint32_t start = limit.records && end / 32 > limit.records ? end - limit.records * 32 : 0;
  uint32_t emitted = 0;
  uint32_t firstMarked = end;
  for (uint32_t p = start; p < end; p += 32) {
    if (!source.read(p, work.record, 32)) return false;
    if (work.record[28] & kSessionStart) {
      firstMarked = p;
      break;
    }
  }
  int64_t previous = 0;
  for (uint32_t cursor = end; cursor > start;) {
    cursor -= 32;
    if (!source.read(cursor, work.record, 32)) return false;
    const int64_t at = number<int64_t>(work.record + 8);
    const bool boundary = work.record[28] & kSessionStart;
    if (cursor < firstMarked && live(work.record) && previous && (previous - at > 30 * 60 * 1000LL || at > previous)) {
      const bool populated = work.session.answers != 0;
      if (!emit(source, work, visit, ctx)) return false;
      if (populated && ++emitted == limit.sessions) return true;
      reset(work);
      if (!source.read(cursor, work.record, 32)) return false;
    }
    const bool valid = live(work.record);
    add(work, cursor);
    if (valid) previous = at;
    if (boundary) {
      const bool populated = work.session.answers != 0;
      if (!emit(source, work, visit, ctx)) return false;
      if (populated && ++emitted == limit.sessions) return true;
      reset(work);
      previous = 0;
    }
  }
  // A record window can begin inside a session. Do not report a partial session.
  return start ? true : emit(source, work, visit, ctx);
}
bool summarize(study::ByteSource& source, uint32_t first, uint32_t end, HistoryWork& work, Session& result) {
  if (first % 32 || end % 32 || first > end || end > source.size()) return false;
  reset(work);
  for (uint32_t p = end; p > first;) {
    p -= 32;
    if (!source.read(p, work.record, 32)) return false;
    add(work, p);
  }
  if (!finish(source, work)) return false;
  result = work.session;
  return true;
}
bool reviewedCard(study::ByteSource& source, const Session& session, int index, HistoryWork& work, int64_t& id) {
  if (index < 0 || uint32_t(index) >= session.cards) return false;
  if (!work.overflow) {
    if (index >= work.idCount) return false;
    id = work.ids[index];
    return true;
  }
  const int wanted = index / 256;
  if (work.pageIndex != wanted) {
    int64_t ceiling = 0;
    for (int page = 0; page <= wanted; ++page) {
      if (!window(source, session, work, page != 0, ceiling) || !work.idCount) return false;
      ceiling = work.ids[0];
    }
    work.pageIndex = wanted;
  }
  const int at = work.idCount - 1 - index % 256;
  if (at < 0) return false;
  id = work.ids[at];
  return true;
}
}  // namespace flashcards
