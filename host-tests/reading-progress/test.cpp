#include <ArduinoJson.h>
#include <Epub.h>
#include <KOReaderCredentialStore.h>
#include <KOReaderDocumentId.h>

#include <cstdio>
#include <cstring>
#include <map>

#include "../../src/apps_local/reading/ReadingProgressSync.h"
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
std::string cache(const std::string& path) {
  const bool xtc = path.ends_with(".xtc") || path.ends_with(".xtch");
  return std::string("/.crosspoint/") + (xtc ? "xtc_" : "epub_") + std::to_string(std::hash<std::string>{}(path)) +
         "/progress.bin";
}
void saved(const std::string& path, int percent) {
  testStorage.files[path] = {1};
  auto& bytes = testStorage.files[cache(path)];
  if (path.ends_with(".xtc") || path.ends_with(".xtch"))
    bytes = {uint8_t(percent - 1), 0, 0, 0};
  else {
    const auto offset = percent * 1000;
    bytes = {0, 0, uint8_t(percent * 10), 0, 100, 0, uint8_t(offset), uint8_t(offset >> 8), 0, 0};
  }
}
std::string remote(int percent, bool fixed = false) {
  return "{\"percentage\":" + std::to_string(percent / 100.0) + ",\"progress\":\"" +
         (fixed ? std::to_string(percent) : "/body/DocFragment[1]/body/p[1]/text()." + std::to_string(percent * 1000)) +
         "\"}";
}
struct Server {
  std::map<std::string, std::string> records;
  int calls = 0, puts = 0, failAt = -1;
  bool lostAck = false;
  static Result exchange(void* user, const char* method, const char* route, const char* body, const char* destination) {
    return static_cast<Server*>(user)->request(method, route, body, destination);
  }
  Result request(const char* method, const char* route, const char* body, const char* destination) {
    const int current = calls++;
    if (current == failAt && !lostAck) return Result::NetworkError;
    std::string response = "{}";
    if (std::strcmp(method, "GET") == 0) {
      CHECK(std::strstr(route, "/syncs/progress/") == route);
      const std::string id(route + 16, 32);
      if (records.count(id)) response = records[id];
    } else {
      CHECK(std::strcmp(route, "/syncs/progress") == 0);
      JsonDocument doc;
      CHECK(!deserializeJson(doc, body));
      CHECK(doc["device"].as<std::string>() == "CrossNext");
      CHECK(!doc["metadata"]["filename"].isNull());
      records[doc["document"].as<std::string>()] = body;
      ++puts;
    }
    testStorage.files[destination] = {response.begin(), response.end()};
    return current == failAt ? Result::NetworkError : Result::Success;
  }
};
Result run(ProgressSync& sync) {
  auto result = Result::Busy;
  for (int n = 0; result == Result::Busy && n < 20000; ++n) result = sync.step();
  return result;
}
void reset() {
  testStorage = {};
  credentials = {};
}
void tests() {
  GfxRenderer renderer;
  const std::string a = "/Books/one.epub", b = "/Manga/two.xtch";
  const auto id = KOReaderDocumentId::calculateFromFilename(a);
  const auto binary = KOReaderDocumentId::calculate(a);
  reset();
  saved(a, 7);
  saved(b, 20);
  Server server;
  server.records[id] = remote(5);
  ProgressSync sync(renderer);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(sync.processed() == 2);
  CHECK(server.puts == 2);
  JsonDocument doc;
  CHECK(!deserializeJson(doc, server.records[id]));
  CHECK(std::fabs(doc["percentage"].as<float>() - .07f) < .00001f);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(server.puts == 2);  // Repeating does not rewrite a synchronized book.
  server.records[id] = remote(9);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(testStorage.files[cache(a)][2] == 90);
  CHECK(server.puts == 2);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(server.puts == 2);

  // A cloud record under the other document-id method is updated in place.
  reset();
  saved(a, 7);
  server = {};
  server.records[binary] = remote(5);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(server.records.count(id) == 0);
  CHECK(server.records.size() == 1);
  CHECK(!deserializeJson(doc, server.records[binary]));
  CHECK(doc["percentage"].as<float>() > .069f);

  // The furthest mapped anchor wins even when percentage metadata disagrees.
  saved(a, 7);
  server.records[id] = remote(5);
  server.records[binary] = "{\"percentage\":0.03,\"progress\":\"/body/DocFragment[2]/body/p[1]/text().100\"}";
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(testStorage.files[cache(a)][0] == 1);

  // Discover more than ten books, nested folders, XTC/XTCH, TXT and Markdown.
  reset();
  server = {};
  for (int n = 0; n < 31; ++n) {
    const char* extension[] = {".epub", ".xtc", ".xtch", ".txt", ".md"};
    saved("/Library/nested/book" + std::to_string(n) + extension[n % 5], 7);
  }
  testStorage.files["/Library/unread.epub"] = {1};
  saved("/.hidden/private.epub", 7);
  saved("/apps/content/unused.epub", 7);
  testStorage.files["/Library/picture.png"] = {1};
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(sync.processed() == 32);
  CHECK(server.records.size() == 31);
  CHECK(server.puts == 31);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(server.puts == 31);

  // A matching cloud book can restore a file that has never been opened here.
  reset();
  server = {};
  testStorage.files[a] = {1};
  server.records[id] = remote(8);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(testStorage.files[cache(a)][2] == 80);
  CHECK(server.puts == 0);
  reset();
  server = {};
  testStorage.files[b] = {1};
  testStorage.enforceParents = true;
  CHECK(testStorage.directories.count(cache(b).substr(0, cache(b).rfind('/'))) == 0);
  server.records[KOReaderDocumentId::calculateFromFilename(b)] = remote(44, true);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(testStorage.files[cache(b)][0] == 43);
  CHECK(testStorage.directories.count(cache(b).substr(0, cache(b).rfind('/'))) == 1);

  // Every request interruption is recoverable; an accepted upload with a lost
  // response is discovered on retry rather than submitted twice.
  for (int fail = 0; fail < 6; ++fail) {
    reset();
    saved(a, 7);
    saved(b, 20);
    server = {};
    server.failAt = fail;
    CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
    CHECK(run(sync) == Result::NetworkError);
    server.failAt = -1;
    CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
    CHECK(run(sync) == Result::Success);
    CHECK(server.records.size() == 2);
    CHECK(server.puts == 2);
  }
  reset();
  saved(a, 7);
  server = {};
  server.failAt = 2;
  server.lostAck = true;
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::NetworkError);
  CHECK(server.puts == 1);
  server.failAt = -1;
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(server.puts == 1);

  for (const char* bad : {"[]", "{\"percentage\":2,\"progress\":\"x\"}", "{\"percentage\":0.5}",
                          "{\"percentage\":\"0.5\",\"progress\":\"x\"}", "{\"error\":\"denied\"}",
                          "{\"percentage\":0.5,\"progress\":\"x\",\"position\":{}}"}) {
    reset();
    saved(a, 7);
    server = {};
    server.records[id] = bad;
    const auto original = testStorage.files[cache(a)];
    CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
    CHECK(run(sync) == Result::InvalidResponse);
    CHECK(testStorage.files[cache(a)] == original);
    CHECK(server.puts == 0);
  }
  reset();
  saved(a, 7);
  server = {};
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  sync.cancel();
  CHECK(sync.step() == Result::Cancelled);
  CHECK(server.calls == 0);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::Success);
  CHECK(server.puts == 1);
  reset();
  saved(a, 7);
  server = {};
  server.records[id] = remote(9);
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  for (int n = 0; n < 7; ++n) CHECK(sync.step() == Result::Busy);
  saved(a, 8);  // The card changed after it was read: do not overwrite it.
  CHECK(run(sync) == Result::LocalChanged);
  CHECK(testStorage.files[cache(a)][2] == 80);
  reset();
  saved(a, 7);
  testStorage.files[cache(a)].resize(3);
  server = {};
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) == Result::StorageError);
  CHECK(server.calls == 0);
  reset();
  saved(a, 7);
  server = {};
  server.records[id] = remote(9);
  testStorage.writeLimit = 2;
  const auto original = testStorage.files[cache(a)];
  CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
  CHECK(run(sync) != Result::Success);
  CHECK(testStorage.files[cache(a)] == original);
  for (const char* directory : {"/", "/Books"}) {
    reset();
    saved(a, 7);
    server = {};
    testStorage.failDirectory = directory;
    CHECK(sync.begin(Server::exchange, &server) == Result::Busy);
    CHECK(run(sync) == Result::StorageError);
    CHECK(server.puts == 0);
  }
}
}  // namespace
int main() {
  tests();
  std::printf("Book progress: %d checks, %d failures\n", checks, failures);
  return failures != 0;
}
