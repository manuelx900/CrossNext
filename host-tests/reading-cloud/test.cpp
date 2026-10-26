#include <cstdio>
#include <string>
#include <vector>

#include "../../src/apps_local/reading/ReadingStatsCloud.h"
#include "../../src/apps_local/reading/ReadingStatsJson.h"

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
constexpr const char* one = "00000000000000000000000000000001";
constexpr const char* two = "00000000000000000000000000000002";
constexpr const char* local = "/.crosspoint/reading-history/sessions.bin";
const std::string summary =
    R"({"seconds":600,"sessions":3,"pages":20,"completed":1,"anchor_day":9795,"streak":3,"daily":[{"date":"2020-01-02","seconds":120},{"date":"2026-10-25","seconds":300},{"date":"2026-10-26","seconds":180}]})";
std::string entry(const char* id, const char* device, int seconds, bool finished) {
  return std::string("{\"device_id\":\"") + device + "\",\"document\":\"" + id +
         "\",\"title\":\"A book\",\"author\":\"An author\",\"stats\":{\"seconds\":" + std::to_string(seconds) +
         ",\"sessions\":1,\"pages\":10,\"completed\":" + (finished ? "true" : "false") +
         ",\"start_date\":1600000000,\"finished_date\":" + (finished ? "1600000600" : "0") + "}}";
}
struct Server {
  std::vector<std::string> bodies;
  std::vector<std::string> paths;
  Result error = Result::Success;
  int failAt = -1;
  Server() {
    bodies.reserve(8);
    paths.reserve(8);
  }
  static Result fetch(void* context, const char* path, const char* destination) {
    auto& s = *static_cast<Server*>(context);
    const int n = s.paths.size();
    s.paths.emplace_back(path);
    if (n == s.failAt) return s.error;
    if (n >= static_cast<int>(s.bodies.size())) return Result::NetworkError;
    HalFile out;
    if (!Storage.openFileForWrite("TEST", destination, out) ||
        out.write(s.bodies[n].data(), s.bodies[n].size()) != s.bodies[n].size())
      return Result::StorageError;
    return Result::Success;
  }
};
Result run(Server& server) {
  Importer importer;
  auto result = importer.begin(dayIndex({2026, 10, 26}), 1793000000, 123, Server::fetch, &server);
  for (int n = 0; n < 20 && result == Result::Busy; ++n) result = importer.step();
  return result;
}
Server normal() {
  Server server;
  server.bodies = {
      summary, "{\"items\":[" + entry(one, "A", 200, true) + "],\"next\":\"a:b &c\"}",
      "{\"items\":[" + entry(one, "B", 100, true) + "," + entry(two, "A", 300, false) + "],\"next\":null}"};
  return server;
}
void preserved(const std::vector<uint8_t>& before) {
  Reader reader;
  CHECK(reader.open() == Result::Success);
  CHECK(reader.data().summary.seconds == 600 && reader.data().books == 2);
  CHECK(testStorage.files[local] == before);
}
}  // namespace
int main() {
  {
    JsonBudget allocator;
    void* block = allocator.allocate(1);
    CHECK(block && reinterpret_cast<uintptr_t>(block) % alignof(std::max_align_t) == 0);
    *static_cast<uint8_t*>(block) = 42;
    block = allocator.reallocate(block, 100);
    CHECK(block && reinterpret_cast<uintptr_t>(block) % alignof(std::max_align_t) == 0 &&
          *static_cast<uint8_t*>(block) == 42);
    allocator.deallocate(block);
    CHECK(!allocator.allocate(2 * 1024 * 1024 + 1));
    block = allocator.allocate(32);
    CHECK(block);
    allocator.deallocate(block);
  }

  testStorage = {};
  testStorage.files[local] = {1, 2, 3, 4, 5};
  const auto before = testStorage.files[local];
  auto server = normal();
  CHECK(run(server) == Result::Success);
  CHECK(server.paths.size() == 3);
  CHECK(server.paths[2] == "/api/v1/stats/books?limit=10&cursor=%61%3A%62%20%26%63");
  {
    Reader reader;
    CHECK(reader.open() == Result::Success);
    CHECK(reader.data().generation == 1 && reader.data().books == 2 && reader.data().finished == 1 &&
          reader.data().days == 3);
    CHECK(reader.data().summary.seconds == 600 && reader.data().summary.sessions == 3 &&
          reader.data().summary.pages == 20);
    CHECK(reader.data().summary.onDay(dayIndex({2026, 10, 26})) == 180);
    CHECK(reader.knownOn(dayIndex({2026, 10, 25})) && reader.activeOn(dayIndex({2026, 10, 25})));
    Book books[5];
    int count = 0;
    CHECK(reader.page(BookFilter::Finished, 0, books, 5, count) && count == 1);
    CHECK(books[0].seconds == 300 && books[0].sessions == 2 && books[0].pages == 20 && books[0].completed);
    CHECK(reader.page(BookFilter::Recent, 0, books, 5, count) && count == 1 && !books[0].completed);
    CHECK(reader.find(one, books[0]) && books[0].seconds == 300);
    PeriodStats period;
    setPeriod(period, Period::Year, dayIndex({2020, 1, 1}), false);
    CHECK(reader.project(period) && period.seconds == 120 && period.annualSeconds[0] == 120);
  }
  preserved(before);
  auto again = normal();
  CHECK(run(again) == Result::Success);
  {
    Reader reader;
    CHECK(reader.open() == Result::Success);
    CHECK(reader.data().generation == 2 && reader.data().summary.seconds == 600 && reader.data().books == 2);
  }
  for (Result failure :
       {Result::AuthFailed, Result::Unsupported, Result::NetworkError, Result::Cancelled, Result::LowMemory}) {
    auto bad = normal();
    bad.failAt = 1;
    bad.error = failure;
    CHECK(run(bad) == failure);
    preserved(before);
  }
  for (const char* invalid : {"{", "{\"seconds\":-1}", "{\"seconds\":false}",
                              "{\"seconds\":600,\"pages\":1,\"sessions\":1,\"completed\":0}garbage",
                              "{\"seconds\":1,\"pages\":1,\"sessions\":1,\"completed\":0,\"daily\":[{\"date\":\"2026-"
                              "02-30\",\"seconds\":1}]}"}) {
    auto bad = normal();
    bad.bodies[0] = invalid;
    CHECK(run(bad) == Result::InvalidResponse);
    preserved(before);
  }
  {
    auto bad = normal();
    bad.bodies[2] = "{\"items\":[" + entry(one, "A", 200, true) + "],\"next\":null}";
    CHECK(run(bad) == Result::InvalidResponse);
    preserved(before);
  }
  {
    auto bad = normal();
    bad.bodies[2] = "{\"items\":[" + entry(one, "B", 200, true) + "],\"next\":\"a:b &c\"}";
    CHECK(run(bad) == Result::InvalidResponse);
    preserved(before);
  }
  {
    Importer importer;
    auto ok = normal();
    CHECK(importer.begin(dayIndex({2026, 10, 26}), 1793000000, 123, Server::fetch, &ok) == Result::Busy);
    CHECK(importer.step() == Result::Busy);
    importer.cancel();
    CHECK(importer.status() == Result::Cancelled);
    preserved(before);
  }
  {
    auto good = normal();
    testStorage.writeLimit = 100;
    CHECK(run(good) == Result::StorageError);
    testStorage.writeLimit = -1;
    preserved(before);
  }
  // A partial new slot cannot supersede the previous complete import.
  testStorage.files[slotPath(0)] = {1, 2, 3};
  preserved(before);
  for (const char* temp : {"sync-finished.tmp", "sync-unfinished.tmp", "sync-days.tmp", "sync-response.json"})
    CHECK(!testStorage.files.count(std::string(kRoot) + "/" + temp));
  {
    Server empty;
    empty.bodies = {"{\"seconds\":0,\"pages\":0,\"sessions\":0,\"completed\":0}", "{\"items\":[],\"next\":null}"};
    CHECK(run(empty) == Result::Success);
    Reader reader;
    CHECK(reader.open() == Result::Success && reader.data().books == 0 && reader.data().summary.seconds == 0);
  }
  {
    const auto previous = testStorage.files[slotPath(1)];
    testStorage.files[slotPath(1)] = {'C', 'S', 'I', '2'};
    Reader reader;
    CHECK(reader.open() == Result::NewerCache);
    auto good = normal();
    CHECK(run(good) == Result::NewerCache);
    CHECK(testStorage.files[slotPath(1)] == std::vector<uint8_t>({'C', 'S', 'I', '2'}));
    testStorage.files[slotPath(1)] = previous;
  }
  CHECK(testStorage.files[local] == before);
  std::printf("Cloud statistics: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
