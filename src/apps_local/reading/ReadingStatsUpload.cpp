#include "ReadingStatsUpload.h"

#include <ArduinoJson.h>
#include <BufferedFile.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "ReadingStatsJson.h"
namespace readingstats::cloud {
namespace {
Result jsonFailure(DeserializationError error) {
  if (error == DeserializationError::NoMemory) {
    LOG_ERR("STATS-SYNC", "OOM: response JSON");
    return Result::LowMemory;
  }
  return Result::InvalidResponse;
}
void bitmap(const Summary& s, char* out) {
  static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  uint8_t bits[92]{};
  for (int n = 0; n < kHistoryDays; ++n)
    if (s.dailySeconds[n]) bits[n / 8] |= 1u << (n % 8);
  int at = 0;
  for (int n = 0; n < 92; n += 3) {
    uint32_t value = uint32_t(bits[n]) << 16;
    if (n + 1 < 92) value |= uint32_t(bits[n + 1]) << 8;
    if (n + 2 < 92) value |= bits[n + 2];
    out[at++] = alphabet[value >> 18];
    out[at++] = alphabet[(value >> 12) & 63];
    out[at++] = n + 1 < 92 ? alphabet[(value >> 6) & 63] : '=';
    out[at++] = n + 2 < 92 ? alphabet[value & 63] : '=';
  }
  out[at] = 0;
}

}  // namespace
Result Uploader::begin(int today, uint64_t identity, uint64_t account, Exchange transport, void* user) {
  exchange = transport;
  context = user;
  stage = Stage::Check;
  nextDay = 10957;
  nextBook = 0;
  if (!identity || !transport) return result = Result::StorageError;
  checks = 0;
  cursor[0] = lastDocument[0] = 0;
  auto previous = makeUniqueNoThrow<Reader>();
  if (!previous) {
    LOG_ERR("STATS-SYNC", "OOM: upload checkpoint");
    return result = Result::LowMemory;
  }
  const auto old = previous->open();
  if (old == Result::LowMemory || old == Result::NewerCache) return result = old;
  const Header* acknowledged =
      old == Result::Success && previous->data().account == account ? &previous->data() : nullptr;
  result = projection.open(nullptr, today, identity, acknowledged);
  if (result != Result::Success) return result;
  std::snprintf(device, sizeof(device), "crossnext-%016llx-%016llx", static_cast<unsigned long long>(identity),
                static_cast<unsigned long long>(projection.checkpoint().identity));
  result = Result::Busy;
  return result;
}
Result Uploader::request(const char* route, const char* body, uint32_t books, uint32_t days) {
  const auto status = exchange(context, "PUT", route, body, kResponse);
  if (status != Result::Success) return status;
  JsonDocument ack;
  HalFile file;
  if (!Storage.openFileForRead("STATS-SYNC", kResponse, file) || file.fileSize64() >= sizeof(response))
    return Result::InvalidResponse;
  const int n = file.read(response, sizeof(response) - 1);
  if (n < 0 || uint64_t(n) != file.fileSize64()) return Result::StorageError;
  response[n] = 0;
  const auto error = deserializeJson(ack, response, DeserializationOption::NestingLimit(4));
  if (error) return jsonFailure(error);
  if (!ack["accepted_daily"].is<uint32_t>() || ack["accepted_daily"].as<uint32_t>() != days)
    return Result::InvalidResponse;
  if (books && (!ack["accepted"].is<uint32_t>() || ack["accepted"].as<uint32_t>() != books))
    return Result::InvalidResponse;
  return Result::Busy;
}
Result Uploader::check() {
  // Read back this origin before replacing cumulative counters. Refuse a restored, older journal.
  std::snprintf(path, sizeof(path), "/api/v1/stats/global?device_id=%s", device);
  auto status = exchange(context, "GET", path, nullptr, kResponse);
  if (status != Result::Success) return status;
  JsonBudget budget;
  JsonDocument doc(&budget), filter(&budget);
  filter["devices"][0]["device_id"] = true;
  for (const char* key : {"seconds", "sessions", "pages"}) filter["devices"][0]["stats"][key] = true;
  filter["devices"][0]["stats"]["daily"][0]["date"] = true;
  filter["devices"][0]["stats"]["daily"][0]["seconds"] = true;
  HalFile file;
  if (!Storage.openFileForRead("STATS-SYNC", kResponse, file) || file.fileSize64() > 2 * 1024 * 1024)
    return Result::InvalidResponse;
  // This endpoint is filtered to one origin and carries at most its daily table.
  JsonReader input(file);
  const auto error =
      deserializeJson(doc, input, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(12));
  if (error) return jsonFailure(error);
  if (!doc["devices"].is<JsonArray>()) return Result::InvalidResponse;
  if (doc["devices"].size() > 1) return Result::InvalidResponse;
  for (JsonVariantConst row : doc["devices"].as<JsonArrayConst>()) {
    if (std::strcmp(row["device_id"] | "", device)) return Result::InvalidResponse;
    const auto& local = projection.data().summary;
    for (const char* key : {"seconds", "sessions", "pages"})
      if (!row["stats"][key].is<uint64_t>()) return Result::InvalidResponse;
    if (row["stats"]["seconds"].as<uint64_t>() > local.seconds ||
        row["stats"]["sessions"].as<uint64_t>() > local.sessions || row["stats"]["pages"].as<uint64_t>() > local.pages)
      return Result::LocalChanged;
    if (!row["stats"]["daily"].isNull()) {
      if (!row["stats"]["daily"].is<JsonArrayConst>()) return Result::InvalidResponse;
      int last = 0;
      for (JsonVariantConst day : row["stats"]["daily"].as<JsonArrayConst>()) {
        const int index = jsonDay(day["date"] | "");
        if (!index || index <= last || !day["seconds"].is<uint32_t>()) return Result::InvalidResponse;
        last = index;
        if (projection.daySeconds(index) < day["seconds"].as<uint64_t>()) return Result::LocalChanged;
      }
    }
  }
  stage = Stage::CheckBooks;
  return Result::Busy;
}
Result Uploader::checkBooks() {
  if (++checks > kMaxBooks * 2) return Result::LimitExceeded;
  std::snprintf(path, sizeof(path), "/api/v1/stats/books?device_id=%s&limit=10", device);
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
  const auto status = exchange(context, "GET", path, nullptr, kResponse);
  if (status != Result::Success) return status;
  JsonBudget budget;
  JsonDocument doc(&budget), filter(&budget);
  filter["next"] = true;
  for (const char* key : {"device_id", "document"}) filter["items"][0][key] = true;
  for (const char* key : {"seconds", "pages", "sessions", "completed"}) filter["items"][0]["stats"][key] = true;
  HalFile file;
  if (!Storage.openFileForRead("STATS-SYNC", kResponse, file)) return Result::StorageError;
  JsonReader input(file);
  const auto error =
      deserializeJson(doc, input, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(12));
  if (error) return jsonFailure(error);
  if (!doc["items"].is<JsonArray>() || doc["items"].size() > 10) return Result::InvalidResponse;
  for (JsonVariantConst row : doc["items"].as<JsonArrayConst>()) {
    const char* id = row["document"] | "";
    if (std::strcmp(row["device_id"] | "", device) || !validDocument(id) || std::strcmp(id, lastDocument) <= 0)
      return Result::InvalidResponse;
    std::strcpy(lastDocument, id);
    if (!projection.find(id, book)) return Result::LocalChanged;
    for (const char* key : {"seconds", "pages", "sessions"})
      if (!row["stats"][key].is<uint64_t>()) return Result::InvalidResponse;
    if (!row["stats"]["completed"].is<bool>()) return Result::InvalidResponse;
    const auto stats = row["stats"];
    if (stats["seconds"].as<uint64_t>() > book.seconds || stats["pages"].as<uint64_t>() > book.pages ||
        stats["sessions"].as<uint64_t>() > book.sessions || (stats["completed"].as<bool>() && !book.completed))
      return Result::LocalChanged;
  }
  if (!doc["next"].isNull() && !doc["next"].is<const char*>()) return Result::InvalidResponse;
  const char* next = doc["next"] | "";
  if (*next && (!doc["items"].size() || std::strlen(next) >= sizeof(cursor) || !std::strcmp(next, cursor)))
    return Result::InvalidResponse;
  std::strcpy(cursor, next);
  if (!*cursor) stage = projection.count() ? Stage::Days : Stage::Done;
  return Result::Busy;
}
Result Uploader::global() {
  JsonDocument doc;
  const auto& s = projection.data().summary;
  if (s.seconds > 9007199254740991ULL || s.pages > 9007199254740991ULL) return Result::LimitExceeded;
  doc["device_id"] = device;
  doc["device"] = "CrossNext X4 Pro";
  doc["v"] = 5;
  doc["seconds"] = s.seconds;
  doc["pages"] = s.pages;
  doc["sessions"] = s.sessions;
  doc["completed"] = s.completed;
  for (int n = 0; n < 4; ++n) doc["tod"].add(projection.tod[n]);
  for (int n = 0; n < 7; ++n) doc["dow"].add(projection.dow[n]);
  doc["anchor_day"] = s.anchorDay - 10957;
  doc["streak"] = s.longest;
  bitmap(s, history);
  doc["history_b64"] = history;
  uint32_t days = 0;
  while (nextDay < 47482 && days < 20) {
    const auto seconds = projection.daySeconds(nextDay);
    if (seconds) {
      if (seconds > UINT32_MAX) return Result::LimitExceeded;
      const auto date = dateFromDay(nextDay);
      char text[16];
      std::snprintf(text, sizeof(text), "%04d-%02d-%02d", date.year, date.month, date.day);
      auto row = doc["daily"].add<JsonObject>();
      row["date"] = text;
      row["seconds"] = seconds;
      ++days;
    }
    ++nextDay;
  }
  if (!days) doc["daily"].to<JsonArray>();
  if (doc.overflowed()) return Result::LowMemory;
  const size_t size = measureJson(doc) + 1;
  if (size > 16384) return Result::LimitExceeded;
  // One <=16 KiB checked request buffer, released after this manual exchange.
  auto body = makeUniqueNoThrow<char[]>(size);
  if (!body) {
    LOG_ERR("STATS-SYNC", "OOM: upload JSON");
    return Result::LowMemory;
  }
  serializeJson(doc, body.get(), size);
  const auto status = request("/api/v1/stats/global", body.get(), 0, days);
  if (status != Result::Busy) return status;
  if (nextDay >= 47482) stage = Stage::Books;
  return Result::Busy;
}
Result Uploader::books() {
  JsonDocument doc;
  doc["device_id"] = device;
  uint32_t count = 0;
  while (nextBook < projection.count() && count < 10) {
    if (!projection.bookAt(nextBook++, book)) return Result::StorageError;
    if (book.seconds > 9007199254740991ULL) return Result::LimitExceeded;
    auto row = doc["items"].add<JsonObject>();
    row["document"] = book.document;
    row["v"] = 5;
    row["seconds"] = book.seconds;
    row["sessions"] = book.sessions;
    row["pages"] = book.pages;
    row["completed"] = book.completed;
    row["avg_fwd"] = book.pages ? book.seconds / book.pages : 0;
    row["pace_n"] = book.pages;
    row["eta"] = 0;
    row["start_manual"] = false;
    row["finish_manual"] = false;
    row["start_date"] = book.firstEpoch;
    row["finished_date"] = book.finishedEpoch;
    for (int n = 0; n < 4; ++n) row["tod"].add(book.buckets[(n + 1) % 4]);
    for (int n = 0; n < 7; ++n) row["dow"].add(0);
    ++count;
  }
  if (doc.overflowed()) return Result::LowMemory;
  const size_t size = measureJson(doc) + 1;
  if (size > 16384) return Result::LimitExceeded;
  // One <=16 KiB checked request buffer, released after this manual exchange.
  auto body = makeUniqueNoThrow<char[]>(size);
  if (!body) {
    LOG_ERR("STATS-SYNC", "OOM: upload JSON");
    return Result::LowMemory;
  }
  serializeJson(doc, body.get(), size);
  const auto status = request("/api/v1/stats/books", body.get(), count, 0);
  if (status != Result::Busy) return status;
  if (nextBook >= projection.count()) stage = Stage::Done;
  return Result::Busy;
}
Result Uploader::step() {
  if (result != Result::Busy) return result;
  switch (stage) {
    case Stage::Check:
      result = check();
      break;
    case Stage::CheckBooks:
      result = checkBooks();
      break;
    case Stage::Days:
      result = global();
      break;
    case Stage::Books:
      result = books();
      break;
    case Stage::Done:
      result = Result::Success;
      break;
  }
  return result;
}
}  // namespace readingstats::cloud
