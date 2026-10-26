#include <Epub.h>
#include <KOReaderDocumentId.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/reading/ReadingProgressSync.h"
#include "../../src/apps_local/reading/ReadingStatsCloudClient.h"
using namespace readingstats::cloud;
namespace {
int checks = 0, failed = 0, uploadCount = 0;
void check(bool ok) {
  ++checks;
  if (!ok) {
    ++failed;
    std::printf("FAIL official progress check %d\n", checks);
  }
}
std::string cache(const std::string& path, bool fixed = false) {
  return std::string("/.crosspoint/") + (fixed ? "xtc_" : "epub_") + std::to_string(std::hash<std::string>{}(path)) +
         "/progress.bin";
}
void seed(const std::string& path, int percent, bool fixed = false) {
  testStorage.files[path] = {1};
  auto& bytes = testStorage.files[cache(path, fixed)];
  const int offset = percent * 1000;
  bytes =
      fixed ? std::vector<uint8_t>{uint8_t(percent - 1), 0, 0, 0}
            : std::vector<uint8_t>{0, 0, uint8_t(percent * 10), 0, 100, 0, uint8_t(offset), uint8_t(offset >> 8), 0, 0};
}
Result exchange(void* context, const char* method, const char* path, const char* body, const char* destination) {
  if (std::strcmp(method, "PUT") == 0) ++uploadCount;
  return Client::exchange(context, method, path, body, destination);
}
bool sync(Client& client, GfxRenderer& renderer) {
  ProgressSync flow(renderer);
  auto status = flow.begin(exchange, &client);
  for (int n = 0; status == Result::Busy && n < 10000; ++n) status = flow.step();
  return status == Result::Success;
}
JsonDocument response() {
  JsonDocument document;
  auto& bytes = testStorage.files[kResponse];
  check(!deserializeJson(document, bytes.data(), bytes.size()));
  return document;
}
}  // namespace
int testProgressServer() {
  GfxRenderer renderer;
  Client client;
  const std::string book = "/Books/sync-seven-percent.epub";
  const auto id = KOReaderDocumentId::calculateFromFilename(book);
  seed(book, 7);
  // Provision the tester's exact 5% cloud / 7% local case.
  const auto old =
      "{\"document\":\"" + id +
      "\",\"progress\":\"/body/DocFragment[1]/body/p[1]/text().5000\",\"percentage\":0.05,\"device\":\"Previous "
      "reader\",\"device_id\":\"previous-reader\"}";
  check(Client::exchange(&client, "PUT", "/syncs/progress", old.c_str(), kResponse) == Result::Success);
  for (int n = 0; n < 14; ++n) seed("/Manga/book" + std::to_string(n) + (n % 2 ? ".xtch" : ".xtc"), 20, true);
  check(sync(client, renderer));
  check(uploadCount == 15);
  const auto route = "/syncs/progress/" + id;
  check(Client::fetch(&client, route.c_str(), kResponse) == Result::Success);
  auto current = response();
  check(std::fabs(current["percentage"].as<float>() - .07f) < .00001f);
  check(sync(client, renderer));
  check(uploadCount == 15);
  // Read offline, then repeat sync: one changed position, one upload.
  seed(book, 8);
  check(sync(client, renderer));
  check(uploadCount == 16);
  check(sync(client, renderer));
  check(uploadCount == 16);
  // A later cloud advance is restored locally without uploading an older one.
  const auto newer = "{\"document\":\"" + id +
                     "\",\"progress\":\"/body/DocFragment[1]/body/p[1]/"
                     "text().9000\",\"percentage\":0.09,\"device\":\"Phone\",\"device_id\":\"phone\"}";
  check(Client::exchange(&client, "PUT", "/syncs/progress", newer.c_str(), kResponse) == Result::Success);
  check(sync(client, renderer));
  check(testStorage.files[cache(book)][2] == 90);
  check(uploadCount == 16);
  check(Client::fetch(&client, "/api/v1/stats/summary", kResponse) == Result::Success);
  const auto stats = response();
  check(stats["seconds"].as<int>() == 450 && stats["pages"].as<int>() == 16 && stats["sessions"].as<int>() == 4);
  std::printf("Official server book progress: %d checks, %d failed; 5%% -> 7%%, offline 8%%, remote 9%%, 15 books\n",
              checks, failed);
  return failed != 0;
}
