#include <KOReaderCredentialStore.h>

#include <cstdio>
#include <string>

#include "../../src/apps_local/reading/ReadingStatsCloudClient.h"
using namespace readingstats;
using namespace readingstats::cloud;
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string base = argv[1];
  int checks = 0, failed = 0;
  auto check = [&](bool ok) {
    ++checks;
    if (!ok) {
      ++failed;
      std::printf("FAIL HTTP check %d\n", checks);
    }
  };
  testStorage.files["local-journal"] = {2, 4, 6};
  for (const auto* mode : {"ok", "ok", "auth", "unsupported", "redirect", "partial", "invalid", "large"}) {
    credentials.url = base + "/" + mode;
    Client client;
    Importer importer;
    auto status = importer.begin(dayIndex({2026, 10, 26}), 1793000000, Client::accountId(), Client::fetch, &client);
    for (int n = 0; n < 10 && status == Result::Busy; ++n) status = importer.step();
    const std::string m = mode;
    const auto expected = m == "ok"            ? Result::Success
                          : m == "auth"        ? Result::AuthFailed
                          : m == "unsupported" ? Result::Unsupported
                          : m == "invalid"     ? Result::InvalidResponse
                          : m == "large"       ? Result::LimitExceeded
                                               : Result::NetworkError;
    check(status == expected);
    Reader reader;
    check(reader.open() == Result::Success && reader.data().summary.seconds == 600 && reader.data().books == 1);
    check(testStorage.files["local-journal"] == std::vector<uint8_t>({2, 4, 6}));
  }
  credentials.url = base + "/ok";
  credentials.present = false;
  check(!Client::configured());
  Client missing;
  check(Client::fetch(&missing, "/api/v1/stats/summary", kResponse) == Result::NoCredentials);
  credentials.present = true;
  credentials.profile = KOReaderServerType::KOSYNC;
  check(!Client::configured());
  check(Client::fetch(&missing, "/api/v1/stats/summary", kResponse) == Result::Unsupported);
  credentials.profile = KOReaderServerType::CROSSPOINT;
  check(Client::configured());
  for (const char* mode : {"unsupported", "empty"}) {
    credentials.url = base + "/" + mode;
    check(Client::fetch(&missing, "/syncs/progress/00000000000000000000000000000001", kResponse) == Result::Empty);
  }
  for (const char* mode : {"created", "no-content"}) {
    credentials.url = base + "/" + mode;
    check(Client::exchange(&missing, "PUT", "/syncs/progress", "{\"document\":\"00000000000000000000000000000001\"}",
                           kResponse) == Result::Success);
  }
  credentials.url = base + "/ok";
  int calls = 0;
  Client cancelled([](void* p) { return ++*static_cast<int*>(p) > 1; }, &calls);
  check(Client::fetch(&cancelled, "/api/v1/stats/summary", kResponse) == Result::Cancelled);
  std::printf("Cloud HTTP integration: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
