#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/apps_local/flashcards/FlashHistory.h"
#include "../../src/apps_local/game2048/Game2048Result.h"

struct Bytes final : study::ByteSource {
  std::vector<uint8_t> data;
  uint32_t reads = 0, minimum = UINT32_MAX;
  uint32_t size() const override { return uint32_t(data.size()); }
  bool read(uint32_t at, void* out, uint32_t bytes) override {
    ++reads;
    if (at < minimum) minimum = at;
    if (at + bytes > data.size()) return false;
    std::memcpy(out, data.data() + at, bytes);
    return true;
  }
  void add(int64_t id, int64_t at, uint8_t flags) {
    uint8_t record[32]{};
    std::memcpy(record, &id, 8);
    std::memcpy(record + 8, &at, 8);
    record[16] = 3;
    record[28] = flags;
    data.insert(data.end(), record, record + 32);
  }
};
int main() {
  Bytes bytes;
  bytes.data.reserve(4000 * 32);
  for (int n = 0; n < 2000; ++n) {
    bytes.add(n + 1, 1792312800000LL + n * 100000LL, flashcards::kSessionStart);
    bytes.add(n + 1, 1792312801000LL + n * 100000LL, 0);
  }
  flashcards::HistoryWork work;
  std::vector<flashcards::Session> sessions;
  sessions.reserve(2000);
  const auto collect = [](void* ctx, const flashcards::Session& session) {
    static_cast<std::vector<flashcards::Session>*>(ctx)->push_back(session);
  };
  assert(flashcards::scanHistory(bytes, work, collect, &sessions, {1000, 0}));
  assert(sessions.size() == 1000 && sessions.front().first == 3998 * 32 && sessions.back().first == 2000 * 32);
  sessions.clear();
  bytes.minimum = UINT32_MAX;
  const auto original = bytes.data;
  assert(flashcards::scanHistory(bytes, work, collect, &sessions, {0, 5}));
  assert(sessions.size() == 2 && sessions.back().answers == 2);
  assert(bytes.minimum >= 3995 * 32 && bytes.data == original);
  sessions.clear();
  assert(flashcards::scanHistory(bytes, work, collect, &sessions));
  assert(sessions.size() == 2000);
  Bytes legacy;
  for (int n = 0; n < 5; ++n) legacy.add(n + 1, 1792312800000LL + n * 3600000LL, 0);
  sessions.clear();
  assert(flashcards::scanHistory(legacy, work, collect, &sessions, {2, 0}));
  assert(sessions.size() == 2);
  sessions.clear();
  assert(flashcards::scanHistory(legacy, work, collect, &sessions, {0, 3}));
  assert(sessions.size() == 2);

  game2048::State game;
  game.cells[0] = game.cells[1] = 10;
  bool won = false;
  assert(game2048::move(game, game2048::Direction::Left));
  assert(game2048::afterMove(game, won) == game2048::Result::Won && won);
  assert(game2048::afterMove(game, won) == game2048::Result::None);
  for (int n = 0; n < 16; ++n) game.cells[n] = (n % 4 + n / 4) % 2 + 1;
  assert(!game2048::canMove(game));
  assert(game2048::afterMove(game, won) == game2048::Result::Lost);
  assert(!game2048::move(game, game2048::Direction::Left));
  game2048::start(game, 1);
  won = false;
  assert(game2048::afterMove(game, won) == game2048::Result::None && !won);
  std::puts("History limits and 2048 results: passed");
}
