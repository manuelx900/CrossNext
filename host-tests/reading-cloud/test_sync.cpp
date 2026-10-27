#include <ArduinoJson.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../../src/apps_local/reading/ReadingStatsUpload.h"
using namespace readingstats;
using namespace readingstats::cloud;
namespace {
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (false)
constexpr const char* logPath = "/.crosspoint/reading-history/sessions.bin";
constexpr const char* a = "00000000000000000000000000000001";
constexpr const char* b = "00000000000000000000000000000002";
constexpr uint64_t device = 777, account = 123;
constexpr int today = 20752;  // 2026-10-26.
struct Server {
  std::map<std::string, std::string> globals, books;
  std::map<std::string, uint64_t> days;
  int requests = 0, failAt = -1;
  bool loseAck = false, rejectAck = false;
  static std::string dump(const JsonDocument& doc) {
    std::string out;
    serializeJson(doc, out);
    return out;
  }
  static Result fetch(void* p, const char* path, const char* destination) {
    return exchange(p, "GET", path, nullptr, destination);
  }
  static Result exchange(void* p, const char* method, const char* path, const char* body, const char* destination) {
    return static_cast<Server*>(p)->request(method, path, body, destination);
  }
  Result request(const char* method, const char* path, const char* body, const char* destination) {
    const int request = requests++;
    if (request == failAt && !loseAck) return Result::NetworkError;
    JsonDocument out, doc;
    if (body && deserializeJson(doc, body)) return Result::InvalidResponse;
    std::string route = path;
    if (std::string(method) == "PUT") {
      const std::string id = doc["device_id"] | "";
      CHECK(!id.empty());
      if (route == "/api/v1/stats/global") {
        CHECK(doc["v"] == 5 && doc["tod"].size() == 4 && doc["dow"].size() == 7 &&
              doc["history_b64"].is<const char*>());
        globals[id] = dump(doc);
        const auto daily = doc["daily"].as<JsonArrayConst>();
        CHECK(daily.size() <= 20);
        for (auto row : daily) {
          auto& value = days[id + ":" + (row["date"] | "")];
          value = std::max(value, row["seconds"].as<uint64_t>());
        }
        out["accepted_daily"] = daily.size();
      } else if (route == "/api/v1/stats/books") {
        auto items = doc["items"].as<JsonArrayConst>();
        CHECK(items.size() && items.size() <= 10);
        for (auto row : items) {
          CHECK(row["v"] == 5 && row["completed"].is<bool>() && row["start_manual"].is<bool>() &&
                row["tod"].size() == 4 && row["dow"].size() == 7);
          JsonDocument copy;
          copy.set(row);
          books[std::string(row["document"] | "") + ":" + id] = dump(copy);
        }
        out["accepted"] = items.size();
        out["accepted_daily"] = 0;
      } else
        return Result::Unsupported;
      out["until"] = 1793000000;
      if (rejectAck) out.clear();
    } else if (route.find("/api/v1/stats/global?device_id=") == 0) {
      const auto id = route.substr(route.find('=') + 1);
      out["devices"].to<JsonArray>();
      auto found = globals.find(id);
      if (found != globals.end()) {
        JsonDocument stats;
        deserializeJson(stats, found->second);
        auto row = out["devices"].add<JsonObject>();
        row["device_id"] = id;
        row["stats"] = stats;
      }
    } else if (route == "/api/v1/stats/summary") {
      uint64_t seconds = 0, pages = 0, sessions = 0, completed = 0;
      for (const auto& [id, text] : globals) {
        JsonDocument stats;
        deserializeJson(stats, text);
        seconds += stats["seconds"].as<uint64_t>();
        pages += stats["pages"].as<uint64_t>();
        sessions += stats["sessions"].as<uint64_t>();
        completed += stats["completed"].as<uint64_t>();
      }
      out["seconds"] = seconds;
      out["pages"] = pages;
      out["sessions"] = sessions;
      out["completed"] = completed;
      std::map<std::string, uint64_t> dates;
      for (const auto& [key, value] : days) dates[key.substr(key.size() - 10)] += value;
      out["daily"].to<JsonArray>();
      for (const auto& [date, value] : dates) {
        auto row = out["daily"].add<JsonObject>();
        row["date"] = date;
        row["seconds"] = value;
      }
    } else if (route.find("/api/v1/stats/books?") == 0) {
      std::string cursor;
      const auto at = route.find("&cursor=");
      if (at != std::string::npos) {
        const auto encoded = route.substr(at + 8);
        for (size_t n = 0; n < encoded.size(); n += 3)
          cursor.push_back(std::stoi(encoded.substr(n + 1, 2), nullptr, 16));
      }
      std::string onlyDevice;
      const auto filtered = route.find("device_id=");
      if (filtered != std::string::npos) {
        const auto end = route.find('&', filtered);
        onlyDevice = route.substr(filtered + 10, end - filtered - 10);
      }
      out["items"].to<JsonArray>();
      int count = 0;
      std::string last;
      for (auto it = books.upper_bound(cursor); it != books.end(); ++it) {
        if (!onlyDevice.empty() && it->first.substr(it->first.find(':') + 1) != onlyDevice) continue;
        if (count == 10) {
          out["next"] = last;
          break;
        }
        const auto& [key, text] = *it;
        const auto colon = key.find(':');
        JsonDocument stats;
        deserializeJson(stats, text);
        auto row = out["items"].add<JsonObject>();
        row["device_id"] = key.substr(colon + 1);
        row["document"] = key.substr(0, colon);
        row["stats"] = stats;
        last = key;
        ++count;
      }
    } else
      return Result::Unsupported;
    if (request == failAt && loseAck) return Result::NetworkError;
    const auto response = dump(out);
    HalFile output;
    if (!Storage.openFileForWrite("TEST", destination, output) ||
        output.write(response.data(), response.size()) != response.size())
      return Result::StorageError;
    return Result::Success;
  }
  void remote() {
    globals["old-reader"] = R"({"v":5,"seconds":300,"pages":10,"sessions":2,"completed":0})";
    books[std::string(a) + ":old-reader"] =
        R"({"v":5,"seconds":300,"pages":10,"sessions":2,"completed":false,"start_date":1700000000,"finished_date":0})";
    days["old-reader:2026-10-26"] = 300;
  }
};
std::map<std::string, Book> localBooks;
void append(const char* document, uint32_t seconds = 120, uint16_t progress = 7000, int day = today) {
  Record record;
  record.epoch = 1793000000;
  record.seconds = seconds;
  record.pages = 3;
  record.localDay = day;
  record.secondOfDay = 12 * 3600;
  copyText(record.book.document, sizeof(record.book.document), document);
  copyText(record.book.title, sizeof(record.book.title), "Local title");
  record.book.progressBp = progress;
  updateBook(record, localBooks[document]);
  localBooks[document] = record.book;
  auto& file = testStorage.files[logPath];
  if (file.empty()) file = {'C', 'R', 'S', '1', 0x80, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const auto at = file.size();
  file.resize(at + kRecordBytes);
  encode(record, file.data() + at);
}
void appendCompletion(const char* document) {
  Record record;
  record.kind = RecordKind::Completion;
  record.epoch = 1793000000;
  record.localDay = today;
  record.secondOfDay = 0;
  copyText(record.book.document, sizeof(record.book.document), document);
  copyText(record.book.title, sizeof(record.book.title), "Finished manually");
  record.book.progressBp = 10000;
  updateBook(record, localBooks[document]);
  localBooks[document] = record.book;
  auto& file = testStorage.files[logPath];
  if (file.empty()) file = {'C', 'R', 'S', '1', 0x80, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const auto at = file.size();
  file.resize(at + kRecordBytes);
  encode(record, file.data() + at);
}
Result sync(Server& server) {
  Uploader upload;
  auto result = upload.begin(today, device, account, Server::exchange, &server);
  for (int n = 0; n < 10000 && result == Result::Busy; ++n) result = upload.step();
  if (result != Result::Success) return result;
  Importer download;
  download.setDevice(upload.deviceId());
  result = download.begin(today, 1793000000, account, Server::fetch, &server, &upload.data());
  for (int n = 0; n < 10000 && result == Result::Busy; ++n) result = download.step();
  return result;
}
void total(uint64_t expected, int pages, int sessions) {
  Reader reader;
  CHECK(reader.open() == Result::Success);
  Projection view;
  CHECK(view.open(&reader, today, device) == Result::Success);
  CHECK(view.data().summary.seconds == expected);
  CHECK(view.data().summary.pages == uint64_t(pages));
  CHECK(view.data().summary.sessions == uint32_t(sessions));
  Book book;
  CHECK(view.find(a, book));
  CHECK(book.seconds == expected);
  PeriodStats stats;
  setPeriod(stats, Period::Day, today, false);
  bool known = false;
  CHECK(view.project(stats, &known) && known && stats.seconds == expected);
}
}  // namespace
int main() {
  {
    testStorage = {};
    localBooks.clear();
    Server finished;
    finished.remote();
    append(a);
    CHECK(sync(finished) == Result::Success);
    appendCompletion(a);
    appendCompletion("00000000000000000000000000000009");
    appendCompletion(a);  // Duplicate valid marker cannot count a second completion.
    for (int attempt = 0; attempt < 3; ++attempt) {
      total(420, 13, 3);
      Reader reader;
      CHECK(reader.open() == Result::Success);
      Projection view;
      CHECK(view.open(&reader, today, device) == Result::Success);
      Book manual;
      CHECK(view.find("00000000000000000000000000000009", manual));
      CHECK(manual.completed && !manual.seconds && !manual.pages && !manual.sessions);
      CHECK(view.data().summary.completed == 2);
      CHECK(sync(finished) == Result::Success);
    }
    localBooks.clear();
  }
  testStorage = {};
  Server server;
  server.remote();
  append(a);
  const auto journal = testStorage.files[logPath];
  CHECK(sync(server) == Result::Success);
  CHECK(testStorage.files[logPath] == journal);
  total(420, 13, 3);
  CHECK(server.globals.size() == 2);
  CHECK(sync(server) == Result::Success);
  total(420, 13, 3);
  append(a, 30);
  total(450, 16, 4);
  CHECK(sync(server) == Result::Success);
  total(450, 16, 4);
  CHECK(server.globals.size() == 2);
  append(a, 20);
  server.failAt = server.requests + 2;
  server.loseAck = true;
  CHECK(sync(server) == Result::NetworkError);
  total(470, 19, 5);
  server.failAt = -1;
  CHECK(sync(server) == Result::Success);
  total(470, 19, 5);
  append(a, 5);
  server.failAt = server.requests + 4;
  server.loseAck = false;
  CHECK(sync(server) == Result::NetworkError);
  total(475, 22, 6);
  server.failAt = -1;
  CHECK(sync(server) == Result::Success);
  total(475, 22, 6);
  CHECK(sync(server) == Result::Success);
  total(475, 22, 6);
  {
    const auto full = testStorage.files[logPath];
    testStorage.files[logPath] = journal;
    CHECK(sync(server) == Result::LocalChanged);
    Reader reader;
    CHECK(reader.open() == Result::Success);
    Projection view;
    CHECK(view.open(&reader, today, device) == Result::LocalChanged);
    testStorage.files[logPath] = full;
  }
  {
    auto saved = testStorage.files[logPath];
    testStorage.files[logPath][140] ^= 1;
    CHECK(sync(server) == Result::StorageError);
    testStorage.files[logPath] = saved;
  }
  {
    failPsram = true;
    Projection view;
    CHECK(view.open(nullptr, today, device) == Result::LowMemory);
    failPsram = false;
  }
  // All local books, including an item completed offline, survive paging and repeated synchronization.
  append(b, 60, 10000);
  CHECK(sync(server) == Result::Success);
  {
    Reader reader;
    CHECK(reader.open() == Result::Success);
    Projection view;
    CHECK(view.open(&reader, today, device) == Result::Success);
    CHECK(view.data().books == 2 && view.data().finished == 1);
    Book books[5];
    int count = 0;
    CHECK(view.page(BookFilter::Finished, 0, books, 5, count) && count == 1 && std::string(books[0].document) == b);
    CHECK(view.page(BookFilter::Recent, 0, books, 5, count) && count == 1 && std::string(books[0].document) == a);
  }
  // More than twenty dates must be uploaded as bounded batches, with cumulative snapshots unchanged.
  for (int n = 1; n <= 25; ++n) append(a, 1, 7000, today - n);
  CHECK(sync(server) == Result::Success);
  CHECK(sync(server) == Result::Success);
  {
    Reader reader;
    CHECK(reader.open() == Result::Success);
    Projection view;
    CHECK(view.open(&reader, today, device) == Result::Success);
    CHECK(view.data().summary.seconds == 560 && view.data().summary.sessions == 32 &&
          view.data().summary.totalDays == 26);
  }
  // Starting a new journal has another origin; old uploaded history remains visible, exactly once.
  testStorage.files[logPath].clear();
  localBooks.clear();
  append(a, 17);
  CHECK(sync(server) == Result::Success);
  CHECK(server.globals.size() == 3);
  {
    Reader reader;
    CHECK(reader.open() == Result::Success);
    Projection view;
    CHECK(view.open(&reader, today, device) == Result::Success);
    CHECK(view.data().summary.seconds == 577);
  }
  {
    // Restoring a divergent prefix with recovered totals must never reach a PUT.
    testStorage = {};
    localBooks.clear();
    Server restored;
    restored.remote();
    append(a);
    const auto backup = testStorage.files[logPath];
    const auto bookBackup = localBooks;
    append(b);
    CHECK(sync(restored) == Result::Success);
    const auto globals = restored.globals, books = restored.books;
    testStorage.files[logPath] = backup;
    localBooks = bookBackup;
    append("00000000000000000000000000000003");
    CHECK(sync(restored) == Result::LocalChanged);
    CHECK(restored.globals == globals && restored.books == books);
    testStorage.files.erase(slotPath(0));
    testStorage.files.erase(slotPath(1));
    CHECK(sync(restored) == Result::LocalChanged);
    CHECK(restored.globals == globals && restored.books == books);
  }
  {
    testStorage = {};
    localBooks.clear();
    Server interrupted;
    interrupted.remote();
    append(a);
    CHECK(sync(interrupted) == Result::Success);
    const auto valid = testStorage.files[logPath];
    for (size_t tail = 1; tail < kRecordBytes; ++tail) {
      testStorage.files[logPath] = valid;
      testStorage.files[logPath].resize(valid.size() + tail, 0xab);
      Reader reader;
      CHECK(reader.open() == Result::Success);
      Projection view;
      CHECK(view.open(&reader, 0, device) == Result::Success);
      CHECK(view.data().summary.seconds == 420 && view.data().summary.anchorDay == today);
      CHECK(view.checkpoint().length == valid.size());
    }
    CHECK(sync(interrupted) == Result::Success);
    CHECK(testStorage.files[logPath].size() == valid.size() + kRecordBytes - 1);
    total(420, 13, 3);
    testStorage.files.erase(slotPath(0));
    testStorage.files.erase(slotPath(1));
    CHECK(sync(interrupted) == Result::Success);
    total(420, 13, 3);
  }
  std::printf("Bidirectional sync: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
