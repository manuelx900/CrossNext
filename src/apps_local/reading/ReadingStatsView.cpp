#include "ReadingStatsView.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace readingstats {
namespace {
uint32_t checksum(const uint8_t* p) {
  uint32_t hash = 2166136261u;
  for (size_t n = 0; n < 248; ++n) hash = (hash ^ p[n]) * 16777619u;
  return hash;
}
void put(uint8_t* p, uint64_t value, unsigned bytes) {
  for (unsigned n = 0; n < bytes; ++n, value >>= 8) p[n] = uint8_t(value);
}
uint64_t get(const uint8_t* p, unsigned bytes) {
  uint64_t value = 0;
  for (unsigned n = 0; n < bytes; ++n) value |= uint64_t(p[n]) << (8 * n);
  return value;
}
}  // namespace
const char* PagedView::path(bool second) const {
  if (kind == Kind::Days) return second ? "/.crosspoint/stats-days-b.tmp" : "/.crosspoint/stats-days-a.tmp";
  return second ? "/.crosspoint/stats-period-b.tmp" : "/.crosspoint/stats-period-a.tmp";
}
void PagedView::close() {
  input.close();
  output.close();
  if (phase != State::Closed) {
    for (bool second : {false, true})
      if (Storage.exists(path(second)) && !Storage.remove(path(second))) LOG_ERR("STVIEW", "Cannot remove view cache");
  }
  phase = State::Closed;
  pendingFrom = pendingTo = 0;
  rows = 0;
}
bool PagedView::fail(const char* reason) {
  LOG_ERR("STVIEW", "%s", reason);
  close();
  phase = State::Failed;
  return false;
}
bool PagedView::begin(Kind next, Book* workspace, uint8_t* buffer, int from, int through, const char* selected) {
  close();
  kind = next;
  work = workspace;
  io = buffer;
  first = from;
  last = through;
  copyText(document, sizeof(document), selected);
  sourceB = ranked = group = false;
  collected = buffered = 0;
  written = pair = left = right = leftEnd = rightEnd = 0;
  run = 8;
  phase = State::Collecting;
  if (!work || !io || (kind == Kind::Days && !validDocument(document))) return fail("Invalid view workspace");
  if (!Storage.openFileForWrite("STVIEW", path(false), output)) return fail("Cannot create view");
  return true;
}
bool PagedView::add(const Record& record) {
  if (phase != State::Collecting || pending()) return fail("Invalid view input state");
  if (record.kind != RecordKind::Session || !record.seconds || !validDocument(record.book.document)) return true;
  if (kind == Kind::Days && std::strcmp(document, record.book.document)) return true;
  pendingFrom = int64_t(record.localDay) * 86400 + record.secondOfDay;
  pendingTo = pendingFrom + record.seconds;
  if (kind == Kind::Period) {
    pendingFrom = std::max(pendingFrom, int64_t(first) * 86400);
    pendingTo = std::min(pendingTo, (int64_t(last) + 1) * 86400);
  }
  if (!pending()) return true;
  work[8] = record.book;
  return true;
}
bool PagedView::drain() {
  if (phase != State::Collecting) return fail("Invalid view collection state");
  if (!pending()) return true;
  auto& book = work[collected];
  book = work[8];
  const int day = int(pendingFrom / 86400);
  const int64_t end = kind == Kind::Days ? std::min(pendingTo, (int64_t(day) + 1) * 86400) : pendingTo;
  book.seconds = uint64_t(end - pendingFrom);
  book.lastDay = kind == Kind::Days ? day : 0;
  pendingFrom = end;
  if (++collected == 8) return flushCollected();
  return true;
}
int PagedView::compare(const Book& a, const Book& b) const {
  if (kind == Kind::Days) return a.lastDay > b.lastDay ? -1 : a.lastDay < b.lastDay ? 1 : 0;
  if (ranked && a.seconds != b.seconds) return a.seconds > b.seconds ? -1 : 1;
  return std::strcmp(a.document, b.document);
}
bool PagedView::sameKey(const Book& a, const Book& b) const {
  return kind == Kind::Days ? a.lastDay == b.lastDay : !std::strcmp(a.document, b.document);
}
void PagedView::encode(const Book& book, uint8_t* bytes) {
  std::memset(bytes, 0, ROW_BYTES);
  std::memcpy(bytes, book.document, sizeof(book.document));
  std::memcpy(bytes + 33, book.title, sizeof(book.title));
  std::memcpy(bytes + 161, book.author, sizeof(book.author));
  put(bytes + 225, book.seconds, 8);
  put(bytes + 233, uint32_t(book.lastDay), 4);
  put(bytes + 248, checksum(bytes), 4);
  std::memcpy(bytes + 252, "STV1", 4);
}
bool PagedView::decode(const uint8_t* bytes, Book& book) {
  if (std::memcmp(bytes + 252, "STV1", 4) || checksum(bytes) != get(bytes + 248, 4) || bytes[32] || bytes[160] ||
      bytes[224])
    return false;
  book = {};
  std::memcpy(book.document, bytes, sizeof(book.document));
  std::memcpy(book.title, bytes + 33, sizeof(book.title));
  std::memcpy(book.author, bytes + 161, sizeof(book.author));
  book.seconds = get(bytes + 225, 8);
  book.lastDay = int(get(bytes + 233, 4));
  return validDocument(book.document);
}
bool PagedView::flushCollected() {
  // Stable insertion sort retains the latest journal snapshot's title for equal keys.
  for (unsigned n = 1; n < collected; ++n) {
    work[9] = work[n];
    unsigned at = n;
    for (; at && compare(work[9], work[at - 1]) < 0; --at) work[at] = work[at - 1];
    work[at] = work[9];
  }
  unsigned unique = 0;
  for (unsigned n = 0; n < collected; ++n) {
    if (unique && sameKey(work[n], work[unique - 1]))
      work[unique - 1].seconds +=
          std::min(work[n].seconds, std::numeric_limits<uint64_t>::max() - work[unique - 1].seconds);
    else {
      if (unique != n) work[unique] = work[n];
      ++unique;
    }
  }
  if (unique != collected) run = 1;
  collected = uint8_t(unique);
  auto* bytes = reinterpret_cast<uint8_t*>(&work[9]);
  static_assert(sizeof(Book) >= ROW_BYTES);
  for (unsigned n = 0; n < collected; ++n) {
    if (rows == UINT32_MAX / ROW_BYTES) return fail("View exceeds file offset range");
    encode(work[n], bytes);
    if (output.write(bytes, ROW_BYTES) != ROW_BYTES) return fail("Cannot write view input");
    ++rows;
  }
  collected = 0;
  return true;
}
bool PagedView::finishInput() {
  if (phase != State::Collecting || pending()) return fail("Unfinished view input");
  if (!flushCollected()) return false;
  if (!output.close()) return fail("Cannot close view input");
  return startPass(rows > run ? State::Sorting : State::Folding);
}
bool PagedView::startPass(State next) {
  if (!Storage.openFileForRead("STVIEW", path(sourceB), input) || input.fileSize64() != uint64_t(rows) * ROW_BYTES ||
      !Storage.openFileForWrite("STVIEW", path(!sourceB), output))
    return fail("Cannot open view pass");
  phase = next;
  pair = left = right = leftEnd = rightEnd = written = 0;
  buffered = 0;
  head[0] = head[1] = group = false;
  loadedCount[0] = loadedCount[1] = 0;
  return true;
}
bool PagedView::read(uint32_t row, Book& book, unsigned side) {
  if (row >= rows) return fail("View row out of range");
  auto* bytes = io + side * 1024;
  if (!loadedCount[side] || row < loadedAt[side] || row - loadedAt[side] >= loadedCount[side]) {
    loadedAt[side] = row;
    loadedCount[side] = std::min<uint32_t>(4, rows - row);
    const auto amount = loadedCount[side] * ROW_BYTES;
    if (!input.seek64(uint64_t(row) * ROW_BYTES) || input.read(bytes, amount) != int(amount))
      return fail("Cannot read view rows");
  }
  if (!decode(bytes + (row - loadedAt[side]) * ROW_BYTES, book)) return fail("Damaged view row");
  return true;
}
bool PagedView::append(const Book& book) {
  encode(book, io + 2048 + buffered * ROW_BYTES);
  ++written;
  return ++buffered < 4 || flush();
}
bool PagedView::flush() {
  const size_t bytes = buffered * ROW_BYTES;
  if (bytes && output.write(io + 2048, bytes) != bytes) return fail("Cannot write view rows");
  buffered = 0;
  return true;
}
bool PagedView::finishPass() {
  if (phase == State::Sorting && written != rows) return fail("Incomplete view sort");
  if (!flush() || !output.close()) return fail("Cannot finish view pass");
  input.close();
  sourceB = !sourceB;
  if (phase == State::Sorting) {
    run = run > rows / 2 ? rows : run * 2;
    if (run < rows) return startPass(State::Sorting);
    if (!ranked) return startPass(State::Folding);
  } else {
    rows = written;
    if (kind == Kind::Period && rows > 1) {
      ranked = true;
      run = 1;
      return startPass(State::Sorting);
    }
  }
  phase = State::Ready;
  if (!Storage.openFileForRead("STVIEW", path(sourceB), input)) return fail("Cannot open prepared view");
  if (Storage.exists(path(!sourceB))) Storage.remove(path(!sourceB));
  loadedCount[0] = loadedCount[1] = 0;
  return true;
}
void PagedView::step() {
  for (unsigned budget = 0; budget < 8; ++budget) {
    switch (phase) {
      case State::Sorting: {
        if (left == leftEnd && right == rightEnd) {
          if (pair == rows) {
            finishPass();
            return;
          }
          left = pair;
          leftEnd = std::min(rows, left + run);
          right = leftEnd;
          rightEnd = std::min(rows, right + run);
          pair = rightEnd;
          head[0] = head[1] = false;
        }
        if (left < leftEnd && !head[0]) {
          if (!read(left, work[0], 0)) return;
          head[0] = true;
        }
        if (right < rightEnd && !head[1]) {
          if (!read(right, work[1], 1)) return;
          head[1] = true;
        }
        const unsigned side = !head[0] || (head[1] && compare(work[1], work[0]) < 0) ? 1 : 0;
        if (!append(work[side])) return;
        if (side)
          ++right;
        else
          ++left;
        head[side] = false;
        break;
      }
      case State::Folding:
        if (left == rows) {
          if (group && work[2].seconds >= kMinimumVisibleSeconds && !append(work[2])) return;
          finishPass();
          return;
        }
        if (!read(left++, work[0], 0)) return;
        if (group && sameKey(work[0], work[2]))
          work[2].seconds += std::min(work[0].seconds, std::numeric_limits<uint64_t>::max() - work[2].seconds);
        else {
          if (group && work[2].seconds >= kMinimumVisibleSeconds && !append(work[2])) return;
          work[2] = work[0];
          group = true;
        }
        break;
      case State::Closed:
      case State::Collecting:
      case State::Ready:
      case State::Failed:
        return;
    }
  }
}
bool PagedView::readPage(int page, int capacity, Book* out, int& count) {
  count = 0;
  if (phase != State::Ready || !out || page < 0 || capacity < 1 || capacity > kBooksPerPage) return false;
  const uint64_t offset = uint64_t(page) * unsigned(capacity);
  if (offset > rows || (offset == rows && page)) return false;
  const unsigned amount = std::min<uint64_t>(capacity, rows - offset);
  if (amount && (!input.seek64(offset * ROW_BYTES) || input.read(io, amount * ROW_BYTES) != int(amount * ROW_BYTES)))
    return fail("Cannot read prepared page");
  for (unsigned n = 0; n < amount; ++n)
    if (!decode(io + n * ROW_BYTES, out[n])) return fail("Damaged prepared page");
  count = int(amount);
  return true;
}
}  // namespace readingstats
