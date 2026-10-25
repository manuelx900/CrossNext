#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>

#include "../../src/apps_local/habits/HabitsCore.h"
#include "../../src/apps_local/minesweeper/MinesweeperHistory.h"
#include "../../src/apps_local/picross/PicrossLegacyMap.h"
#include "../../src/apps_local/picross/PicrossPuzzles.h"
#include "../../src/apps_local/picross/PicrossSave.h"
#include "../../src/apps_local/quest/QuestCore.h"
#include "../../src/apps_local/wordle/LocalWordle.h"
int main() {
  assert(quest::count(quest::Slot::Aura) == 50);
  std::set<int> ids;
  for (int n = 0; n < 50; ++n) {
    int id = quest::catalogItem(quest::Slot::Aura, n);
    assert(id >= 100 && id < 150 && quest::visibleItem(id));
    ids.insert(id);
  }
  assert(ids.size() == 50);
  quest::Profile p, q;
  p.xp = 100000000;
  assert(quest::equip(p, 123));
  assert(quest::equip(p, 160));
  assert(quest::unequip(p, quest::Slot::Aura));
  assert(quest::unequip(p, quest::Slot::Pet));
  assert(!quest::unequip(p, quest::Slot::Body));
  assert(quest::owns(p, 123) && quest::owns(p, 160));
  uint8_t profile[quest::kProfileBytes];
  quest::encode(p, profile);
  assert(quest::decode(profile, q));
  assert(q.equipped[int(quest::Slot::Aura)] == quest::kGearCount &&
         q.equipped[int(quest::Slot::Pet)] == quest::kGearCount);
  assert(quest::equip(p, 123));
  quest::encode(p, profile);
  assert(quest::decode(profile, q) && q.equipped[int(quest::Slot::Aura)] == 123);
  assert(quest::ownedAuraCount(q) == 1 && quest::ownedAuraItem(q, 0) == 123);
  assert(quest::visibleItem(123) && !quest::visibleItem(233));
  const auto balance = quest::coins(q);
  assert(quest::unequip(q, quest::Slot::Aura));
  assert(quest::equip(q, quest::ownedAuraItem(q, 0)) && quest::coins(q) == balance);
  assert(quest::ownedAuraItem(q, 1) == quest::kGearCount);
  habits::Store historic;
  const int historyDay = habits::dayIndex(2026, 10, 17);
  assert(habits::add(historic, "A", historyDay - 1) == habits::NameResult::Ok);
  assert(habits::add(historic, "B", historyDay) == habits::NameResult::Ok);
  assert(habits::countOnDay(historic, historyDay - 1) == 1);
  assert(habits::indexOnDay(historic, historyDay - 1, 0) == 0 && habits::indexOnDay(historic, historyDay - 1, 1) == -1);
  assert(habits::countOnDay(historic, historyDay) == 2 && habits::indexOnDay(historic, historyDay, 1) == 1);
  assert(localwordle::contains(5, "PERRO", 1));
  assert(!localwordle::contains(5, "PERRO", 0));
  assert(localwordle::contains(4, "NI[O", 1));
  assert(!localwordle::contains(5, "XXXXX", 1));
  for (int n = 4; n <= 6; ++n)
    for (int k = 0; k < localwordle::wordCount(n, 1); ++k)
      assert(localwordle::contains(n, localwordle::wordAt(n, k, 1), 1));
  localwordle::State s;
  s.language = 1;
  localwordle::start(s);
  assert(s.gameLanguage == 1);
  assert(localwordle::contains(s.letters, s.game.answer(), 1));
  s.game.start(0, "PERRO");
  for (char c : {'X', 'X', 'X', 'X', 'X'}) assert(s.game.type(c));
  auto spanish = [](void*, const char* word) { return localwordle::contains(5, word, 1); };
  assert(s.game.submit(spanish, nullptr) == wordle::Game::Submit::NotAWord && s.game.guesses() == 0 &&
         s.game.typed() == 5);
  for (int n = 0; n < 5; ++n) assert(s.game.erase());
  for (char c : {'L', 'I', 'B', 'R', 'O'}) assert(s.game.type(c));
  assert(s.game.submit(spanish, nullptr) == wordle::Game::Submit::Scored && s.game.guesses() == 1 &&
         s.game.typed() == 0);
  for (char c : {'P', 'E', 'R', 'R', 'O'}) assert(s.game.type(c));
  assert(s.game.submit(spanish, nullptr) == wordle::Game::Submit::Won && s.game.guesses() == 2);
  localwordle::record(s);
  uint8_t saved[localwordle::kSaveBytes];
  localwordle::encode(s, 77, saved);
  uint32_t generation = 0;
  localwordle::State restored;
  assert(localwordle::decode(saved, restored, generation) && generation == 77 && restored.gameLanguage == 1 &&
         restored.game.guesses() == 2);
  wordle::Game enye;
  enye.start(0, "NI[O", 4);
  for (char c : {'N', 'I', '[', 'O'}) assert(enye.type(c));
  assert(enye.submit([](void*, const char*) { return true; }, nullptr) == wordle::Game::Submit::Won);
  int grids = 0;
  for (int size : {10, 15, 20})
    for (int seed = 1; seed < 100; ++seed) {
      minesweeper::Game g;
      minesweeper::start(g, seed, size, size);
      assert(g.columns == size && g.rows == size);
      assert(minesweeper::dig(g, size / 2, size / 2));
      assert(!minesweeper::neighbouringMines(g, size / 2, size / 2));
      int mines = 0;
      for (int x = 0; x < size; ++x)
        for (int y = 0; y < size; ++y) {
          mines += (g.cell[x][y] & minesweeper::kMine) != 0;
          if ((g.cell[x][y] & minesweeper::kRevealed) && !minesweeper::neighbouringMines(g, x, y))
            for (int dx = -1; dx <= 1; ++dx)
              for (int dy = -1; dy <= 1; ++dy)
                if (minesweeper::inside(g, x + dx, y + dy)) assert(g.cell[x + dx][y + dy] & minesweeper::kRevealed);
        }
      assert(mines == g.mines);
      g.status = minesweeper::Status::Lost;
      uint8_t bytes[minehistory::kBytes];
      minehistory::encode(g, 9, 4, bytes);
      minesweeper::Game copy;
      int wins = 0, losses = 0;
      assert(minehistory::decode(bytes, copy, wins, losses) && wins == 9 && losses == 4 && copy.columns == size &&
             !std::memcmp(g.cell, copy.cell, 400));
      bytes[215] ^= 1;
      assert(!minehistory::decode(bytes, copy, wins, losses));
      ++grids;
    }
  assert(picross::kPuzzleCount == 450);
  int counts[11]{};
  for (auto& puzzle : picross::kPuzzles) ++counts[puzzle.size];
  assert(counts[5] == 100 && counts[8] == 100 && counts[9] == 100 && counts[10] == 150);
  std::set<int> legacy;
  for (auto id : picross::kLegacyV6) {
    assert(id < 450);
    legacy.insert(id);
  }
  assert(legacy.size() == 199);
  picross::Board board;
  picross::Progress progress;
  uint8_t oldSave[picrosssave::kLegacyBytes]{};
  for (int id = 0; id < 199; ++id) {
    std::memset(oldSave, 0, sizeof(oldSave));
    oldSave[0] = 6;
    oldSave[1] = id;
    picrosssave::put(oldSave + 104 + id / 32 * 4, uint32_t(1) << (id % 32));
    assert(picrosssave::decode(oldSave, sizeof(oldSave), board, progress));
    assert(board.index() == picross::kLegacyV6[id] && progress.isSolved(picross::kLegacyV6[id]) &&
           progress.solvedCount() == 1);
  }
  board.load(449);
  progress.markSolved(449);
  uint8_t newSave[picrosssave::kBytes];
  picrosssave::encode(board, progress, newSave);
  assert(picrosssave::decode(newSave, sizeof(newSave), board, progress) && board.index() == 449 &&
         progress.isSolved(449));
  for (size_t n = 0; n < sizeof(newSave); ++n) {
    newSave[n] ^= 128;
    assert(!picrosssave::decode(newSave, sizeof(newSave), board, progress));
    newSave[n] ^= 128;
  }
  assert(!picrosssave::decode(nullptr, 0, board, progress));
  FILE* file = std::fopen("../../tools_local/wasm/sdcard/.crosspoint/habits-a.bin", "rb");
  assert(file);
  uint8_t hb[habits::kFileSize];
  assert(std::fread(hb, 1, sizeof(hb), file) == sizeof(hb));
  std::fclose(file);
  habits::Store habitStore;
  assert(habits::decode(hb, sizeof(hb), habitStore, generation) && habitStore.count == 3);
  file = std::fopen("../../tools_local/wasm/sdcard/.crosspoint/reading-history/sessions.bin", "rb");
  assert(file);
  assert(std::fseek(file, 16, SEEK_SET) == 0);
  readingstats::Summary totals;
  totals.anchorDay = readingstats::dayIndex({2026, 10, 17});
  uint8_t recordBytes[384];
  int sessions = 0;
  while (std::fread(recordBytes, 1, 384, file) == 384) {
    readingstats::Record record;
    assert(readingstats::decode(recordBytes, record));
    totals.add(record);
    ++sessions;
  }
  std::fclose(file);
  assert(sessions == 53 && totals.completed == 3 && totals.onDay(totals.anchorDay) > 0);
  std::printf(
      "Refinement: 50 auras, ownership+unequip, Spanish/Ñ submit+save, %d large minefields, 450 Picross/199 migrated "
      "identities\n",
      grids);
}
