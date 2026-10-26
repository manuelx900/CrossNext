#include "ReadingProgressSync.h"

#include <Logging.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace readingstats::cloud {
namespace {
constexpr size_t maxBody = 16384;
constexpr char response[] = "/.crosspoint/reading-history/progress-response.json";
bool uint16Field(JsonVariantConst value) { return value.is<unsigned>() && value.as<unsigned>() <= 65535; }
}  // namespace
Result ProgressSync::begin(Exchange transport, void* user) {
  cancel();
  exchange = transport;
  context = user;
  count = 0;
  stage = Stage::Scan;
  if (!transport) return result = Result::NetworkError;
  // One reusable 16 KiB serialization buffer; too large for the task stack.
  if (!body) body = makeUniqueNoThrow<char[]>(maxBody);
  if (!body) {
    LOG_ERR("PROGRESS-SYNC", "OOM: %u byte request buffer", unsigned(maxBody));
    return result = Result::LowMemory;
  }
  const auto next = books.begin();
  return next == Result::Busy ? result = next : stop(next);
}
Result ProgressSync::stop(Result reason) {
  if (reason != Result::Success && reason != Result::Cancelled)
    LOG_ERR("PROGRESS-SYNC", "Stopped at book %lu: result=%d", static_cast<unsigned long>(count), int(reason));
  books.close();
  json.clear();
  Storage.remove(response);
  return result = reason;
}
void ProgressSync::cancel() { stop(Result::Cancelled); }
Result ProgressSync::fetch(const std::string& document, KOReaderProgress& remote, bool& found) {
  found = false;
  std::snprintf(path, sizeof(path), "/syncs/progress/%s?position_kinds=locator,percentage", document.c_str());
  const auto request = exchange(context, "GET", path, nullptr, response);
  if (request == Result::Empty) return Result::Success;
  if (request != Result::Success) return request;
  HalFile file;
  if (!Storage.openFileForRead("PROGRESS-SYNC", response, file)) return Result::StorageError;
  if (file.fileSize() > maxBody) return Result::LimitExceeded;
  json.clear();
  JsonReader reader(file);
  const auto error = deserializeJson(json, reader);
  if (error) return error == DeserializationError::NoMemory ? Result::LowMemory : Result::InvalidResponse;
  if (!json.is<JsonObject>()) return Result::InvalidResponse;
  if (json.size() == 0) return Result::Success;
  const char* progress = json["progress"].as<const char*>();
  if (!progress || std::strlen(progress) > 4096 || !json["percentage"].is<double>()) return Result::InvalidResponse;
  const float percentage = json["percentage"].as<float>();
  if (!std::isfinite(percentage) || percentage < 0 || percentage > 1) return Result::InvalidResponse;
  std::destroy_at(&remote);
  std::construct_at(&remote);
  remote.document = document;
  remote.progress = progress;
  remote.percentage = percentage;
  if (!json["position"].isNull()) {
    const JsonVariantConst position = json["position"];
    if (!position.is<JsonObjectConst>() || !uint16Field(position["spine"]) || !uint16Field(position["page"]) ||
        !uint16Field(position["pages"]) || position["pages"].as<unsigned>() == 0 ||
        position["page"].as<unsigned>() >= position["pages"].as<unsigned>() || !position["pctQ"].is<unsigned>() ||
        position["pctQ"].as<unsigned>() > 1000000)
      return Result::InvalidResponse;
    KOReaderRichPosition rich;
    rich.spineIndex = position["spine"];
    rich.pageNumber = position["page"];
    rich.totalPages = position["pages"];
    rich.pctQ = position["pctQ"];
    if (!position["xpath"].isNull()) {
      const char* xpath = position["xpath"].as<const char*>();
      if (!xpath || std::strlen(xpath) > 120) return Result::InvalidResponse;
      rich.xpath = xpath;
    }
    if (!position["para"].isNull()) {
      if (!uint16Field(position["para"])) return Result::InvalidResponse;
      rich.paragraphIndex = position["para"].as<uint16_t>();
    }
    remote.position = std::move(rich);
  }
  json.clear();
  found = true;
  return Result::Success;
}
Result ProgressSync::upload() {
  const auto check = books.unchanged(book);
  if (check != Result::Success) return check;
  json.clear();
  json["document"] = book.local.document;
  json["progress"] = book.local.progress;
  json["percentage"] = book.local.percentage;
  json["device"] = "CrossNext";
  json["device_id"] = "crossplay-reader";
  if (book.local.position) {
    const auto& rich = *book.local.position;
    auto position = json["position"].to<JsonObject>();
    position["pctQ"] = rich.pctQ;
    position["spine"] = rich.spineIndex;
    position["page"] = rich.pageNumber;
    position["pages"] = rich.totalPages;
    if (!rich.xpath.empty()) position["xpath"] = rich.xpath;
  }
  if (book.local.metadata) {
    const auto& metadata = *book.local.metadata;
    auto object = json["metadata"].to<JsonObject>();
    object["filename"] = metadata.filename;
    object["title"] = metadata.title;
    object["authors"] = metadata.authors;
    JsonDocument extra(&budget);
    if (!metadata.extraJson.empty() && !deserializeJson(extra, metadata.extraJson)) {
      for (JsonPairConst item : extra.as<JsonObjectConst>()) {
        const auto value = item.value();
        if (object[item.key()].isNull() &&
            (value.is<const char*>() || value.is<bool>() || value.is<long long>() || value.is<double>()))
          object[item.key()] = value;
      }
    }
  }
  if (json.overflowed()) return Result::LowMemory;
  if (measureJson(json) >= maxBody) return Result::LimitExceeded;
  serializeJson(json, body.get(), maxBody);
  json.clear();
  return exchange(context, "PUT", "/syncs/progress", body.get(), response);
}
Result ProgressSync::completeBook() {
  ++count;
  stage = Stage::Scan;
  return Result::Busy;
}
Result ProgressSync::step() {
  if (result != Result::Busy) return result;
  Result next = Result::Success;
  switch (stage) {
    case Stage::Scan:
      next = books.next(book);
      if (next == Result::Empty) return stop(Result::Success);
      if (next == Result::Busy) return result;
      if (next != Result::Success) break;
      hasPrimary = hasAlternate = false;
      stage = Stage::Prepare;
      return result;
    case Stage::Prepare:
      next = books.prepare(book);
      if (next == Result::Success) stage = Stage::Primary;
      break;
    case Stage::Primary:
      next = fetch(book.local.document, primary, hasPrimary);
      if (next == Result::Success) stage = Stage::Alternate;
      break;
    case Stage::Alternate:
      if (book.alternate != book.local.document) next = fetch(book.alternate, alternate, hasAlternate);
      if (next == Result::Success) stage = Stage::Resolve;
      break;
    case Stage::Resolve: {
      if (!hasPrimary && !hasAlternate) {
        if (!book.savedSize) return completeBook();
        stage = Stage::Upload;
        return result;
      }
      ProgressComparison comparison = ProgressComparison::Unknown;
      bool useAlternate = false;
      next = books.resolve(book, hasPrimary ? &primary : nullptr, hasAlternate ? &alternate : nullptr, comparison,
                           useAlternate, destination);
      if (next != Result::Success) break;
      // Continue using the existing remote identity rather than creating a
      // second document when the account used the other matching method.
      if (useAlternate) book.local.document = book.alternate;
      switch (comparison) {
        case ProgressComparison::LocalAhead:
          stage = Stage::Upload;
          return result;
        case ProgressComparison::RemoteAhead:
          stage = Stage::Apply;
          return result;
        case ProgressComparison::Synchronized:
          return completeBook();
        case ProgressComparison::Unknown:
          next = Result::InvalidResponse;
          break;
      }
      break;
    }
    case Stage::Upload:
      next = upload();
      if (next == Result::Success) return completeBook();
      break;
    case Stage::Apply:
      next = books.apply(book, destination);
      if (next == Result::Success) return completeBook();
      break;
  }
  return next == Result::Success ? result : stop(next);
}
}  // namespace readingstats::cloud
