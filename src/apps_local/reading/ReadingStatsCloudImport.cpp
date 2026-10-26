#include <ArduinoJson.h>
#include <BufferedFile.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "ReadingStatsCloud.h"
#include "ReadingStatsJson.h"
#include "ReadingStatsProjection.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace readingstats::cloud {
namespace {
constexpr const char* kFinished = "/.crosspoint/reading-history/sync-finished.tmp";
constexpr const char* kUnfinished = "/.crosspoint/reading-history/sync-unfinished.tmp";
constexpr const char* kDays = "/.crosspoint/reading-history/sync-days.tmp";
uint64_t number(JsonVariantConst value, bool& valid, uint64_t maximum = UINT64_MAX) {
  if (!value.is<uint64_t>() || value.as<uint64_t>() > maximum) {
    valid = false;
    return 0;
  }
  return value.as<uint64_t>();
}
uint64_t optionalNumber(JsonVariantConst value, bool& valid, uint64_t maximum = UINT64_MAX) {
  return value.isNull() ? 0 : number(value, valid, maximum);
}
void put(uint8_t* p, uint64_t v, size_t n) {
  for (size_t i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (i * 8));
}
bool add(uint64_t& to, uint64_t value) {
  if (UINT64_MAX - to < value) return false;
  to += value;
  return true;
}
bool add32(uint32_t& to, uint32_t value) {
  if (UINT32_MAX - to < value) return false;
  to += value;
  return true;
}
bool merge(Book& to, const Book& value) {
  if (!add(to.seconds, value.seconds) || !add32(to.sessions, value.sessions) || !add32(to.pages, value.pages))
    return false;
  for (int n = 0; n < 4; ++n)
    if (!add(to.buckets[n], value.buckets[n])) return false;
  if (!to.firstEpoch || (value.firstEpoch && value.firstEpoch < to.firstEpoch)) to.firstEpoch = value.firstEpoch;
  to.finishedEpoch = std::max(to.finishedEpoch, value.finishedEpoch);
  to.lastEpoch = std::max(to.lastEpoch, value.lastEpoch);
  to.completed |= value.completed;
  to.progressBp = to.completed ? 10000 : 0;
  return true;
}
void setBit(uint8_t* mask, int offset) {
  if (offset >= 0 && offset < kHistoryDays) mask[offset / 8] |= 1u << (offset % 8);
}
bool active(const Header& h, int offset) {
  return offset >= 0 && offset < kHistoryDays && (h.active[offset / 8] & (1u << (offset % 8)));
}
bool history(const char* text, Header& h, int anchor) {
  if (!text || !*text) return true;
  // CrossPoint's 730-day bitmap is exactly 92 bytes encoded as base64.
  if (std::strlen(text) != 124 || text[123] != '=') return false;
  uint32_t bits = 0;
  int available = 0, bytes = 0;
  for (int n = 0; n < 123; ++n) {
    const char c = text[n];
    const int v = c >= 'A' && c <= 'Z'   ? c - 'A'
                  : c >= 'a' && c <= 'z' ? c - 'a' + 26
                  : c >= '0' && c <= '9' ? c - '0' + 52
                  : c == '+'             ? 62
                  : c == '/'             ? 63
                                         : -1;
    if (v < 0) return false;
    bits = (bits << 6) | v;
    available += 6;
    if (available >= 8) {
      available -= 8;
      const uint8_t byte = bits >> available;
      for (int b = 0; b < 8; ++b)
        if (byte & (1u << b)) {
          const int old = bytes * 8 + b;
          if (old >= kHistoryDays) return false;
          setBit(h.active, h.summary.anchorDay - (anchor - old));
        }
      ++bytes;
    }
  }
  return bytes == 92 && (bits & 3u) == 0;
}
bool book(JsonVariantConst item, Book& b) {
  b = {};
  const char* document = item["document"] | "";
  if (!validDocument(document) || !item["stats"].is<JsonObjectConst>()) return false;
  copyText(b.document, sizeof(b.document), document);
  const char* title = item["title"] | "";
  if (!*title) title = item["filename"] | document;
  copyText(b.title, sizeof(b.title), title);
  copyText(b.author, sizeof(b.author), item["author"] | "");
  const auto s = item["stats"];
  bool valid = true;
  b.seconds = number(s["seconds"], valid);
  b.sessions = number(s["sessions"], valid, UINT32_MAX);
  b.pages = number(s["pages"], valid, UINT32_MAX);
  b.firstEpoch = optionalNumber(s["start_date"], valid, 4102444799ULL);
  b.finishedEpoch = optionalNumber(s["finished_date"], valid, 4102444799ULL);
  if (!s["completed"].is<bool>()) valid = false;
  b.completed = s["completed"] | false;
  b.lastEpoch = b.finishedEpoch;
  b.progressBp = b.completed ? 10000 : 0;
  if (!s["tod"].isNull()) {
    if (!s["tod"].is<JsonArrayConst>() || s["tod"].size() != 4) return false;
    for (int n = 0; n < 4; ++n) b.buckets[(n + 1) % 4] = number(s["tod"][n], valid);
  }
  return valid;
}
DeserializationError parse(HalFile& file, JsonDocument& doc, JsonDocument& filter) {
  JsonReader reader{file};
  const auto error =
      deserializeJson(doc, reader, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(12));
  if (error) return error;
  for (int c = reader.read(); c >= 0; c = reader.read())
    if (c != ' ' && c != '\r' && c != '\n' && c != '\t') return DeserializationError::InvalidInput;
  return DeserializationError::Ok;
}
}  // namespace

void Importer::setDevice(const char* device) { copyText(ownDevice, sizeof(ownDevice), device); }
Importer::~Importer() { cancel(); }
void Importer::cleanup() {
  completeBooks.close();
  unfinishedBooks.close();
  days.close();
  if (writingSlot && result != Result::Success) Storage.remove(slotPath(slot));
  writingSlot = false;
  for (const char* name : {kFinished, kUnfinished, kDays, kResponse})
    if (Storage.exists(name)) Storage.remove(name);
}
void Importer::cancel() {
  if (result == Result::Busy) result = Result::Cancelled;
  cleanup();
}
Result Importer::fail(Result error) {
  LOG_ERR("STATS-SYNC", "Import stopped: %d", static_cast<int>(error));
  result = error;
  return result;
}
Result Importer::begin(int today, int64_t importedAt, uint64_t account, Fetch source, void* user, Projection* sent) {
  uploaded = sent;
  ownBooks = 0;
  const auto next = start(today, importedAt, account, source, user);
  if (next != Result::Busy) cleanup();
  return next;
}
Result Importer::start(int today, int64_t importedAt, uint64_t account, Fetch source, void* user) {
  cleanup();
  header = {};
  pending = {};
  incoming = {};
  hasPending = false;
  cursor[0] = lastDevice[0] = 0;
  fetch = source;
  context = user;
  requests = 0;
  stage = Stage::Summary;
  // Header codec workspace and prior-slot reader exist only during this manual flow.
  bytes = makeUniqueNoThrow<uint8_t[]>(kHeaderBytes);
  auto previous = makeUniqueNoThrow<Reader>();
  if (!bytes || !previous) return fail(Result::LowMemory);
  const auto old = previous->open();
  if (old == Result::LowMemory || old == Result::NewerCache) return fail(old);
  slot = old == Result::Success ? 1 - previous->activeSlot() : 0;
  const uint32_t generation = old == Result::Success ? previous->data().generation : 0;
  if (generation == UINT32_MAX || !source) return fail(Result::LimitExceeded);
  previous.reset();
  header.generation = generation + 1;
  header.account = account;
  header.importedAt = importedAt;
  if (uploaded) {
    const auto& c = uploaded->checkpoint();
    header.localIdentity = c.identity;
    header.localLength = c.length;
    header.localHash = c.hash;
  }
  header.summary.anchorDay = today;
  if (!Storage.ensureDirectoryExists(kRoot) || !Storage.openFileForWrite("STATS-SYNC", kFinished, completeBooks) ||
      !Storage.openFileForWrite("STATS-SYNC", kUnfinished, unfinishedBooks) ||
      !Storage.openFileForWrite("STATS-SYNC", kDays, days))
    return fail(Result::StorageError);
  result = Result::Busy;
  return result;
}
Result Importer::readSummary() {
  auto status = fetch(context, "/api/v1/stats/summary", kResponse);
  if (status != Result::Success) return fail(status);
  JsonBudget budget;
  JsonDocument doc(&budget), filter(&budget);
  for (const char* key : {"seconds", "pages", "sessions", "completed", "anchor_day", "history_b64", "streak"})
    filter[key] = true;
  filter["daily"][0]["date"] = true;
  filter["daily"][0]["seconds"] = true;
  HalFile file;
  if (!Storage.openFileForRead("STATS-SYNC", kResponse, file)) return fail(Result::StorageError);
  const auto error = parse(file, doc, filter);
  if (error) return fail(error == DeserializationError::NoMemory ? Result::LowMemory : Result::InvalidResponse);
  bool valid = doc.is<JsonObject>();
  auto& s = header.summary;
  s.seconds = number(doc["seconds"], valid);
  s.pages = number(doc["pages"], valid);
  s.sessions = number(doc["sessions"], valid, UINT32_MAX);
  s.completed = number(doc["completed"], valid, UINT32_MAX);
  const int remoteAnchor = optionalNumber(doc["anchor_day"], valid, 36524) + dayIndex({2000, 1, 1});
  if (s.anchorDay < dayIndex({2000, 1, 1}) || s.anchorDay > dayIndex({2099, 12, 31})) s.anchorDay = remoteAnchor;
  s.longest = optionalNumber(doc["streak"], valid, UINT32_MAX);
  if (uploaded && (s.seconds < uploaded->data().summary.seconds || s.pages < uploaded->data().summary.pages ||
                   s.sessions < uploaded->data().summary.sessions))
    return fail(Result::InvalidResponse);
  if (!valid || !history(doc["history_b64"] | "", header, remoteAnchor)) return fail(Result::InvalidResponse);
  if (!doc["daily"].isNull() && !doc["daily"].is<JsonArray>()) return fail(Result::InvalidResponse);
  int previousDay = 0;
  for (JsonVariantConst entry : doc["daily"].as<JsonArrayConst>()) {
    const int day = jsonDay(entry["date"] | "");
    const auto seconds = number(entry["seconds"], valid);
    if (!day || day <= previousDay || !valid || header.days >= kMaxDays) return fail(Result::InvalidResponse);
    previousDay = day;
#ifdef ARDUINO
    if ((header.days & 255) == 0) delay(1);
#endif
    put(bytes.get(), day, 4);
    put(bytes.get() + 4, seconds, 8);
    put(bytes.get() + 12, checksum(bytes.get(), 12), 4);
    if (days.write(bytes.get(), kDayBytes) != kDayBytes) return fail(Result::StorageError);
    ++header.days;
    if (seconds) ++s.totalDays;
    const int offset = s.anchorDay - day;
    if (offset >= 0 && offset < kHistoryDays) {
      if (seconds > UINT32_MAX) return fail(Result::InvalidResponse);
      s.dailySeconds[offset] = seconds;
      setBit(header.known, offset);
      if (seconds) setBit(header.active, offset);
    }
  }
  uint32_t run = 0;
  for (int n = kHistoryDays - 1; n >= 0; --n) {
    if (active(header, n)) {
      if (!(header.known[n / 8] & (1u << (n % 8)))) ++s.totalDays;
      ++run;
      s.lastReadDay = s.anchorDay - n;
      s.run = run;
      s.longest = std::max(s.longest, run);
    } else
      run = 0;
  }
  stage = Stage::Books;
  return result;
}
bool Importer::flushBook() {
  if (!hasPending) return true;
  if (header.books >= kMaxBooks) return false;
  encodeBook(pending, bytes.get());
  auto& destination = pending.completed ? completeBooks : unfinishedBooks;
  if (destination.write(bytes.get(), kBookBytes) != kBookBytes) return false;
  ++header.books;
  if (pending.completed) ++header.finished;
  hasPending = false;
  return true;
}
Result Importer::readBooks() {
  if (++requests > kMaxBooks * 2) return fail(Result::LimitExceeded);
  std::strcpy(path, "/api/v1/stats/books?limit=10");
  if (*cursor) {
    std::strcat(path, "&cursor=");
    static constexpr char hex[] = "0123456789ABCDEF";
    size_t at = std::strlen(path);
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(cursor); *c; ++c) {
      path[at++] = '%';
      path[at++] = hex[*c >> 4];
      path[at++] = hex[*c & 15];
    }
    path[at] = 0;
  }
  const auto status = fetch(context, path, kResponse);
  if (status != Result::Success) return fail(status);
  JsonBudget budget;
  JsonDocument doc(&budget), filter(&budget);
  filter["next"] = true;
  for (const char* key : {"device_id", "document", "title", "author", "filename"}) filter["items"][0][key] = true;
  for (const char* key : {"seconds", "sessions", "pages", "completed", "start_date", "finished_date", "tod"})
    filter["items"][0]["stats"][key] = true;
  HalFile file;
  if (!Storage.openFileForRead("STATS-SYNC", kResponse, file)) return fail(Result::StorageError);
  const auto error = parse(file, doc, filter);
  if (error) return fail(error == DeserializationError::NoMemory ? Result::LowMemory : Result::InvalidResponse);
  if (!doc["items"].is<JsonArray>() || doc["items"].size() > 10) return fail(Result::InvalidResponse);
  for (JsonVariantConst item : doc["items"].as<JsonArrayConst>()) {
    const char* device = item["device_id"] | "";
    if (!*device || std::strlen(device) >= sizeof(lastDevice) || !book(item, incoming))
      return fail(Result::InvalidResponse);
    if (uploaded && !std::strcmp(device, ownDevice)) {
      if (!uploaded->find(incoming.document, pendingOwn) || pendingOwn.seconds != incoming.seconds ||
          pendingOwn.pages != incoming.pages || pendingOwn.sessions != incoming.sessions ||
          pendingOwn.completed != incoming.completed)
        return fail(Result::InvalidResponse);
      if (!*incoming.title || !std::strcmp(incoming.title, incoming.document))
        copyText(incoming.title, sizeof(incoming.title), pendingOwn.title);
      if (!*incoming.author) copyText(incoming.author, sizeof(incoming.author), pendingOwn.author);
      ++ownBooks;
    }
    const int comparison = hasPending ? std::strcmp(incoming.document, pending.document) : 1;
    if (comparison < 0 || (comparison == 0 && std::strcmp(device, lastDevice) <= 0))
      return fail(Result::InvalidResponse);
    if (comparison > 0) {
      if (!flushBook()) return fail(header.books >= kMaxBooks ? Result::LimitExceeded : Result::StorageError);
      pending = incoming;
      hasPending = true;
    } else if (!merge(pending, incoming))
      return fail(Result::InvalidResponse);
    std::strcpy(lastDevice, device);
  }
  if (!doc["next"].isNull() && !doc["next"].is<const char*>()) return fail(Result::InvalidResponse);
  const char* next = doc["next"] | "";
  if (*next && (!doc["items"].size() || std::strlen(next) >= sizeof(cursor) || !std::strcmp(next, cursor)))
    return fail(Result::InvalidResponse);
  std::strcpy(cursor, next);
  if (!*cursor) {
    if (!flushBook()) return fail(header.books >= kMaxBooks ? Result::LimitExceeded : Result::StorageError);
    stage = Stage::Commit;
  }
  return result;
}
Result Importer::commit() {
  if (uploaded && ownBooks != uploaded->count()) return fail(Result::InvalidResponse);
  completeBooks.close();
  unfinishedBooks.close();
  days.close();
  writingSlot = true;
  {
    HalFile output;
    encodeHeader(header, bytes.get());
    if (!Storage.openFileForWrite("STATS-SYNC", slotPath(slot), output) ||
        output.write(bytes.get(), kHeaderBytes) != kHeaderBytes)
      return fail(Result::StorageError);
    for (const char* name : {kDays, kFinished, kUnfinished}) {
      HalFile input;
      if (!Storage.openFileForRead("STATS-SYNC", name, input)) return fail(Result::StorageError);
      for (;;) {
        const int n = input.read(bytes.get(), kHeaderBytes);
        if (n < 0 || (n && output.write(bytes.get(), n) != static_cast<size_t>(n))) return fail(Result::StorageError);
        if (!n) break;
#ifdef ARDUINO
        delay(1);
#endif
      }
    }
  }
  auto verify = makeUniqueNoThrow<Reader>();
  if (!verify) return fail(Result::LowMemory);
  if (verify->open() != Result::Success || verify->activeSlot() != slot ||
      verify->data().generation != header.generation)
    return fail(Result::StorageError);
  result = Result::Success;
  return result;
}
Result Importer::step() {
  if (result != Result::Busy) return result;
  switch (stage) {
    case Stage::Summary:
      readSummary();
      break;
    case Stage::Books:
      readBooks();
      break;
    case Stage::Commit:
      commit();
      break;
  }
  if (result != Result::Busy) cleanup();
  return result;
}
}  // namespace readingstats::cloud
