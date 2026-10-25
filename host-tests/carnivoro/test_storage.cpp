#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "CarnivoroCore.h"
int checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                               \
    }                                             \
  } while (0)
#define LOG_ERR(...) ((void)0)
struct PowerCut {};
struct FakeStorage;
struct HalFile {
  std::vector<uint8_t>* data = nullptr;
  size_t size() const { return data ? data->size() : 0; }
  int read(uint8_t*, size_t);
  size_t write(const uint8_t*, size_t);
  void flush();
};
struct FakeStorage {
  std::map<std::string, std::vector<uint8_t>> files;
  int calls = 0, failAt = 0;
  bool cut = false, online = true;
  bool io() {
    if (++calls != failAt) return true;
    if (cut) throw PowerCut{};
    return false;
  }
  bool ready() const { return online; }
  bool ensureDirectoryExists(const char*) { return io(); }
  bool exists(const char* name) const { return files.count(name); }
  bool openFileForRead(const char*, const char* name, HalFile& file) {
    if (!io() || !exists(name)) return false;
    file.data = &files.at(name);
    return true;
  }
  bool openFileForWrite(const char*, const char* name, HalFile& file) {
    if (!io()) return false;
    file.data = &files[name];
    file.data->clear();
    return true;
  }
  bool remove(const char* name) {
    if (!io()) return false;
    return files.erase(name);
  }
  bool rename(const char* from, const char* to) {
    if (!io() || !exists(from) || exists(to)) return false;
    files[to] = files.at(from);
    files.erase(from);
    return true;
  }
} Storage;
int HalFile::read(uint8_t* out, size_t n) {
  if (!Storage.io() || !data) return -1;
  n = std::min(n, data->size());
  std::memcpy(out, data->data(), n);
  return n;
}
size_t HalFile::write(const uint8_t* in, size_t n) {
  if (!Storage.io()) {
    data->assign(in, in + n / 2);
    return n / 2;
  }
  data->assign(in, in + n);
  return n;
}
void HalFile::flush() { Storage.io(); }
namespace carnivoroui {
enum class View { Menu, Intro, Verdict };
}
class CarnivoroActivity {
 public:
  carnivoro::State state, pending, verified;
  struct {
    bool saveError = false;
    int page = 0, casePage = 0;
    carnivoroui::View view = carnivoroui::View::Menu;
  } model;
  uint8_t wire[carnivoro::kSaveBytes]{};
  enum class PendingOperation { None, Accusation, Replay, Reset };
  PendingOperation pendingOperation = PendingOperation::None;
  bool writable = true, dirty = false;
  enum class ReadResult : uint8_t { Valid, Invalid, Future, IoError };
  ReadResult read(const char*, carnivoro::State&);
  bool save(const carnivoro::State&);
  void load();
  bool flush();
};
constexpr char live[] = "/.crosspoint/carnivoro.bin", backup[] = "/.crosspoint/carnivoro.bak",
               temp[] = "/.crosspoint/carnivoro.tmp";
#include "activity_storage.inc"
std::vector<uint8_t> encoded(const carnivoro::State& state) {
  std::vector<uint8_t> out(carnivoro::kSaveBytes);
  CHECK(carnivoro::encode(state, out.data(), out.size()) == out.size());
  return out;
}
bool same(const carnivoro::State& a, const carnivoro::State& b) { return !std::memcmp(&a, &b, sizeof(a)); }
int main() {
  carnivoro::State original;
  original.caseId = 0;
  original.innocents = 1;
  original.status[0] = carnivoro::Status::Active;
  const auto baseline = encoded(original);
  for (auto operation : {CarnivoroActivity::PendingOperation::Accusation, CarnivoroActivity::PendingOperation::Replay,
                         CarnivoroActivity::PendingOperation::Reset}) {
    carnivoro::State next = original;
    if (operation == CarnivoroActivity::PendingOperation::Accusation)
      CHECK(carnivoro::accuse(next, 0, 1));
    else if (operation == CarnivoroActivity::PendingOperation::Replay) {
      next.status[0] = carnivoro::Status::Solved;
      CHECK(carnivoro::restartCase(next, 0));
    } else
      carnivoro::resetProgress(next);
    // Every storage-call boundary is tested as both a reported error and abrupt exit.
    for (bool cut : {false, true})
      for (int step = 1; step <= 24; ++step) {
        Storage = FakeStorage{};
        Storage.files[live] = baseline;
        Storage.failAt = step;
        Storage.cut = cut;
        CarnivoroActivity activity;
        activity.state = original;
        activity.pending = next;
        activity.pendingOperation = operation;
        bool committed = false;
        try {
          committed = activity.flush();
        } catch (PowerCut&) {
        }
        if (!cut && !committed) {
          CHECK(same(activity.state, original));
          CHECK(activity.model.saveError);
          CHECK(activity.pendingOperation == operation);
        }
        Storage.failAt = 0;
        Storage.cut = false;
        CarnivoroActivity reopened;
        reopened.load();
        CHECK(!reopened.model.saveError);
        CHECK(same(reopened.state, original) || same(reopened.state, next));
        if (!cut && !committed) {
          if (!activity.flush()) {
            std::printf("retry operation=%d fault=%d writable=%d\n", int(operation), step, activity.writable);
            CHECK(false);
          }
          CHECK(same(activity.state, next));
        }
        if (committed) {
          CHECK(same(activity.state, next));
          CHECK(activity.pendingOperation == CarnivoroActivity::PendingOperation::None);
        }
      }
  }
  Storage = FakeStorage{};
  Storage.files[live] = {1, 2, 3};
  Storage.files[backup] = baseline;
  CarnivoroActivity recovered;
  recovered.load();
  CHECK(same(recovered.state, original));
  CHECK(recovered.save(original));
  CHECK(Storage.exists(live));
  auto newer = baseline;
  newer[4] = carnivoro::kSaveVersion + 1;
  newer.resize(4096);
  for (const char* path : {live, backup})
    for (bool cut : {false, true})
      for (int fault = 0; fault <= 12; ++fault) {
        Storage = FakeStorage{};
        Storage.files[live] = baseline;
        Storage.files[path] = newer;
        CarnivoroActivity protectedSave;
        protectedSave.load();
        Storage.calls = 0;
        Storage.failAt = fault;
        Storage.cut = cut;
        try {
          CHECK(!protectedSave.save(original));
        } catch (PowerCut&) {
        }
        CHECK(Storage.files.at(path) == newer);
      }
  Storage = FakeStorage{};
  Storage.files[live] = {1, 2, 3};
  CarnivoroActivity bad;
  bad.load();
  CHECK(!bad.writable && bad.model.saveError);
  CHECK(!bad.save(original));
  std::printf("%d storage checks, 0 failures; production method bodies with injected HAL failures\n", checks);
}
