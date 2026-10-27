#include <KOReaderCredentialStore.h>

#include <cstdio>
#include <map>
#include <string>

#include "../../src/apps_local/reading/ReadingStatsCloudClient.h"
#include "../../src/apps_local/reading/ReadingStatsUpload.h"
using namespace readingstats;
using namespace readingstats::cloud;
constexpr const char* logPath = "/.crosspoint/reading-history/sessions.bin";
constexpr const char* document = "00000000000000000000000000000001";
Book previous;
void append(uint32_t seconds) {
  Record r;
  r.epoch = 1793000000;
  r.localDay = dayIndex({2026, 10, 26});
  r.secondOfDay = 12 * 3600;
  r.seconds = seconds;
  r.pages = 3;
  copyText(r.book.document, sizeof(r.book.document), document);
  copyText(r.book.title, sizeof(r.book.title), "Test local title");
  r.book.progressBp = 8000;
  updateBook(r, previous);
  previous = r.book;
  auto& bytes = testStorage.files[logPath];
  if (bytes.empty()) bytes = {'C', 'R', 'S', '1', 0x80, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const auto at = bytes.size();
  bytes.resize(at + 384);
  encode(r, bytes.data() + at);
}
Result sync() {
  Client client;
  Uploader upload;
  auto result =
      upload.begin(dayIndex({2026, 10, 26}), Client::deviceTag(), Client::accountId(), Client::exchange, &client);
  for (int n = 0; n < 100 && result == Result::Busy; ++n) result = upload.step();
  if (result != Result::Success) return result;
  Importer download;
  download.setDevice(upload.deviceId());
  result =
      download.begin(dayIndex({2026, 10, 26}), 1793000000, Client::accountId(), Client::fetch, &client, &upload.data());
  for (int n = 0; n < 100 && result == Result::Busy; ++n) result = download.step();
  return result;
}
int testProgressServer();
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  credentials.url = argv[1];
  int checks = 0, failed = 0;
  auto check = [&](bool ok) {
    ++checks;
    if (!ok) {
      ++failed;
      std::printf("FAIL official server check %d\n", checks);
    }
  };
  append(120);
  check(sync() == Result::Success);
  auto total = [&](uint64_t seconds) {
    Reader reader;
    check(reader.open() == Result::Success);
    Projection view;
    check(view.open(&reader, dayIndex({2026, 10, 26}), Client::deviceTag()) == Result::Success);
    check(view.data().summary.seconds == seconds);
    Book b;
    check(view.find(document, b) && b.seconds == seconds);
  };
  total(420);
  check(sync() == Result::Success);
  total(420);
  append(30);
  total(450);
  check(sync() == Result::Success);
  total(450);
  check(sync() == Result::Success);
  total(450);
  // Completion-only books must survive the real server's validation and round trip.
  Record completion;
  completion.kind = RecordKind::Completion;
  completion.epoch = 1793000000;
  completion.localDay = dayIndex({2026, 10, 26});
  copyText(completion.book.document, sizeof(completion.book.document), "00000000000000000000000000000009");
  copyText(completion.book.title, sizeof(completion.book.title), "Manually finished book");
  completion.book.progressBp = 10000;
  updateBook(completion, Book{});
  auto& bytes = testStorage.files[logPath];
  const auto at = bytes.size();
  bytes.resize(at + kRecordBytes);
  encode(completion, bytes.data() + at);
  for (int attempt = 0; attempt < 2; ++attempt) {
    check(sync() == Result::Success);
    total(450);
    Reader reader;
    check(reader.open() == Result::Success);
    Projection view;
    check(view.open(&reader, dayIndex({2026, 10, 26}), Client::deviceTag()) == Result::Success);
    Book manual;
    check(view.find(completion.book.document, manual));
    check(manual.completed && !manual.seconds && !manual.pages && !manual.sessions);
  }
  Record reopened;
  reopened.kind = RecordKind::Reopen;
  reopened.epoch = completion.epoch + 1;
  reopened.localDay = completion.localDay;
  reopened.book.progressBp = 8000;
  copyText(reopened.book.document, sizeof(reopened.book.document), completion.book.document);
  copyText(reopened.book.title, sizeof(reopened.book.title), completion.book.title);
  updateBook(reopened, completion.book);
  const auto reopenAt = bytes.size();
  bytes.resize(reopenAt + kRecordBytes);
  encode(reopened, bytes.data() + reopenAt);
  for (int attempt = 0; attempt < 2; ++attempt) {
    check(sync() == Result::Success);
    total(450);
    Reader reader;
    check(reader.open() == Result::Success);
    Projection view;
    check(view.open(&reader, dayIndex({2026, 10, 26}), Client::deviceTag()) == Result::Success);
    Book book;
    check(view.find(reopened.book.document, book) && !book.completed && book.manuallyUnfinished);
    check(view.data().finished == 0 && view.data().summary.completed == 0);
  }
  std::printf("Official CrossPoint Sync + production client: %d checks, %d failed\n", checks, failed);
  return (failed || testProgressServer()) ? 1 : 0;
}
