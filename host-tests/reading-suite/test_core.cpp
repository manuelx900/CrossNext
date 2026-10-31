#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/activities/reader/ReaderSession.h"
#include "../../src/activities/reader/XtcPageView.h"
#include "../../src/apps_local/game2048/Game2048Store.h"
#include "../../src/apps_local/home/HomeMenuCore.h"
#include "../../src/apps_local/home/HomeMenuStore.h"
#include "../../src/apps_local/reading/ReadingStatsStore.h"
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
constexpr const char* document = "0123456789abcdef0123456789abcdef";
int main() {
  CHECK(xtcview::fit(0, 800, 480, 800, 0, 0, 0, 0).width == 0);
  CHECK(xtcview::fit(480, 800, 480, 800, 800, 0, 0, 0).height == 0);
  // Four oriented safe areas, both status positions, plus odd bitmap dimensions.
  for (int orientation = 0; orientation < 4; ++orientation) {
    const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
    for (int bar : {0, 1, 2}) {
      const int top = (orientation == 0 ? 10 : 0) + (bar == 1 ? 28 : 0);
      const int right = orientation == 1 ? 10 : 0;
      const int bottom = (orientation == 2 ? 10 : 0) + (bar == 2 ? 28 : 0);
      const int left = orientation == 3 ? 10 : 0;
      const auto view = xtcview::fit(480, 800, width, height, top, right, bottom, left);
      CHECK(view.x >= left && view.y >= top);
      CHECK(view.x + view.width <= width - right && view.y + view.height <= height - bottom);
      CHECK(view.sourceX(0) == 0 && view.sourceY(0) == 0);
      CHECK(view.sourceX(view.width - 1) == 479 && view.sourceY(view.height - 1) == 799);
      CHECK(std::abs(view.width * 800 - view.height * 480) <= 800);
      for (int y = 0; y < view.height; ++y) {
        CHECK(view.sourceY(y) >= 0 && view.sourceY(y) < 800);
      }
    }
  }
  // Independently encode a four-tone source, then sample through each fitted view.
  for (int depth : {1, 2}) {
    constexpr int width = 17, height = 29;
    const size_t plane = width * ((height + 7) / 8);
    std::vector<uint8_t> data(depth == 2 ? plane * 2 : ((width + 7) / 8) * height, depth == 2 ? 0 : 255);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const int tone = (x + y) % (depth == 2 ? 4 : 2);
        if (depth == 2) {
          const size_t offset = (width - 1 - x) * ((height + 7) / 8) + y / 8;
          if (tone & 2) data[offset] |= 1 << (7 - y % 8);
          if (tone & 1) data[plane + offset] |= 1 << (7 - y % 8);
        } else if (tone)
          data[y * ((width + 7) / 8) + x / 8] &= ~(1 << (7 - x % 8));
      }
    for (int scale : {8, 13, 40}) {
      const auto view = xtcview::fit(width, height, scale, 40, 0, 0, 0, 0);
      for (int y = 0; y < view.height; ++y)
        for (int x = 0; x < view.width; ++x) {
          const int sx = view.sourceX(x), sy = view.sourceY(y);
          CHECK(xtcview::pixel(data.data(), width, height, depth, sx, sy) == (sx + sy) % (depth == 2 ? 4 : 2));
        }
    }
  }

  homemenu::Config menu;
  CHECK(homemenu::visibleCount(menu) == 7);
  CHECK(homemenu::at(menu, 6) == homemenu::Id::Settings);
  CHECK(homemenu::at(menu, 2) == homemenu::Id::Transfer);
  CHECK(homemenu::at(menu, 3) == homemenu::Id::Stats);
  CHECK(!homemenu::setVisible(menu, homemenu::Id::Settings, false));
  CHECK(!homemenu::setVisible(menu, homemenu::Id::Services, true));
  CHECK(homemenu::visibleCount(menu) == 7);
  CHECK(homemenu::addShortcut(menu, 0, "CHESS") == homemenu::Id::Count);
  CHECK(homemenu::setVisible(menu, homemenu::Id::Services, false));
  CHECK(homemenu::setVisible(menu, homemenu::Id::Apps, false));
  CHECK(homemenu::setVisible(menu, homemenu::Id::Transfer, true));
  CHECK(homemenu::visibleCount(menu) == 6);
  CHECK(homemenu::setLabel(menu, homemenu::Id::Settings, "Configuraci\xc3\xb3n"));
  CHECK(!homemenu::setLabel(menu, homemenu::Id::Files, "\xf0\x80\x80\x80"));
  CHECK(!homemenu::setLabel(menu, homemenu::Id::Files, "\xe2\x82"));
  CHECK(!homemenu::setLabel(menu, homemenu::Id::Files, "\n"));
  CHECK(homemenu::setLabel(menu, homemenu::Id::Files, ""));
  uint8_t menuBytes[homemenu::kSaveBytes];
  homemenu::encode(menu, 42, menuBytes);
  homemenu::Config restored;
  uint32_t generation = 0;
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(generation == 42 && restored.visible == menu.visible);
  for (size_t n = 0; n < sizeof(menuBytes); ++n) {
    menuBytes[n] ^= 1;
    CHECK(!homemenu::decode(menuBytes, restored, generation));
    menuBytes[n] ^= 1;
  }
  // Eight-entry saves migrate without discarding custom labels or shortcut definitions.
  homemenu::Config eight;
  eight.visible |= 1u << int(homemenu::Id::Services);
  CHECK(homemenu::setLabel(eight, homemenu::Id::Files, "My files"));
  homemenu::encode(eight, 99, menuBytes);
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(homemenu::visibleCount(restored) == 7);
  CHECK(homemenu::indexOf(restored, homemenu::Id::Settings) >= 0);
  CHECK(!std::strcmp(restored.labels[int(homemenu::Id::Files)], "My files"));
  // All entries, including Settings and shortcuts, can move without changing identity.
  CHECK(homemenu::move(menu, homemenu::Id::Settings, -1));
  CHECK(homemenu::at(menu, 4) == homemenu::Id::Settings);
  CHECK(homemenu::move(menu, homemenu::Id::Settings, -1));
  CHECK(homemenu::indexOf(menu, homemenu::Id::Settings) == 3);
  CHECK(!homemenu::move(menu, homemenu::at(menu, 0), -1));
  CHECK(!homemenu::move(menu, homemenu::Id::Settings, 2));
  homemenu::encode(menu, 43, menuBytes);
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(homemenu::at(restored, 3) == homemenu::Id::Settings && generation == 43);
  constexpr size_t orderOffset = 12 + sizeof(menu.labels) + homemenu::kShortcutCount * (homemenu::kItemBytes + 1);
  menuBytes[orderOffset + 1] = menuBytes[orderOffset];
  auto hash = homemenu::checksum(menuBytes, sizeof(menuBytes) - 4);
  for (int n = 0; n < 4; ++n) menuBytes[sizeof(menuBytes) - 4 + n] = hash >> (8 * n);
  CHECK(!homemenu::decode(menuBytes, restored, generation));
  // HMC2 has no order array: retain its default Settings-last layout and labels.
  homemenu::encode(menu, 44, menuBytes);
  menuBytes[3] = '2';
  hash = homemenu::checksum(menuBytes, sizeof(menuBytes) - 4);
  for (int n = 0; n < 4; ++n) menuBytes[sizeof(menuBytes) - 4 + n] = hash >> (8 * n);
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(homemenu::at(restored, 5) == homemenu::Id::Settings && generation == 44);
  // Stock HMC2/HMC3 menus gain Transfer; customized menus keep their choices.
  homemenu::Config stock;
  stock.visible = 0x3f;
  constexpr homemenu::Id oldOrder[] = {homemenu::Id::Files,     homemenu::Id::Recent,    homemenu::Id::Stats,
                                       homemenu::Id::Games,     homemenu::Id::Apps,      homemenu::Id::Transfer,
                                       homemenu::Id::Services,  homemenu::Id::Shortcut0, homemenu::Id::Shortcut1,
                                       homemenu::Id::Shortcut2, homemenu::Id::Shortcut3, homemenu::Id::Shortcut4,
                                       homemenu::Id::Settings};
  std::memcpy(stock.order, oldOrder, sizeof(oldOrder));
  for (char version : {'2', '3'}) {
    homemenu::encode(stock, 45, menuBytes);
    menuBytes[3] = version;
    hash = homemenu::checksum(menuBytes, sizeof(menuBytes) - 4);
    for (int n = 0; n < 4; ++n) menuBytes[sizeof(menuBytes) - 4 + n] = hash >> (8 * n);
    CHECK(homemenu::decode(menuBytes, restored, generation));
    CHECK(homemenu::visibleCount(restored) == 7 && homemenu::at(restored, 2) == homemenu::Id::Transfer);
    CHECK(homemenu::at(restored, 6) == homemenu::Id::Settings);
  }
  CHECK(homemenu::setLabel(stock, homemenu::Id::Files, "My books"));
  homemenu::encode(stock, 46, menuBytes);
  menuBytes[3] = '3';
  hash = homemenu::checksum(menuBytes, sizeof(menuBytes) - 4);
  for (int n = 0; n < 4; ++n) menuBytes[sizeof(menuBytes) - 4 + n] = hash >> (8 * n);
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(homemenu::visibleCount(restored) == 6 && homemenu::at(restored, 2) == homemenu::Id::Stats);
  homemenu::Config hidden;
  CHECK(homemenu::setVisible(hidden, homemenu::Id::Transfer, false));
  homemenu::encode(hidden, 47, menuBytes);
  CHECK(menuBytes[3] == '4');
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(homemenu::visibleCount(restored) == 6 && homemenu::indexOf(restored, homemenu::Id::Transfer) == -1);
  testStorage = {};
  homemenu::Store preferences;
  preferences.load();
  CHECK(preferences.save(menu));
  const int writes = testStorage.writes;
  CHECK(preferences.save(menu));
  CHECK(testStorage.writes == writes);
  homemenu::Store preferencesReload;
  preferencesReload.load();
  CHECK(preferencesReload.get().visible == menu.visible);
  auto changed = menu;
  CHECK(homemenu::setVisible(changed, homemenu::Id::Transfer, false));
  testStorage.writeLimit = 20;
  CHECK(!preferencesReload.save(changed));
  testStorage.writeLimit = -1;
  homemenu::Store recoveredPrefs;
  recoveredPrefs.load();
  CHECK(recoveredPrefs.get().visible == menu.visible);
  testStorage.files["/.crosspoint/home-menu-b.bin"] = {'H', 'M', 'C', '5'};
  homemenu::Store futurePrefs;
  futurePrefs.load();
  CHECK(!futurePrefs.save(changed));
  // A shortcut shares the seven-item budget and is identified by title, not shelf row.
  homemenu::Config shortcuts;
  CHECK(homemenu::setVisible(shortcuts, homemenu::Id::Apps, false));
  const auto chessId = homemenu::addShortcut(shortcuts, 0, "CHESS");
  CHECK(chessId != homemenu::Id::Count && homemenu::visibleCount(shortcuts) == 7);
  CHECK(homemenu::findShortcut(shortcuts, 0, "CHESS") == chessId);
  CHECK(homemenu::at(shortcuts, 6) == homemenu::Id::Settings);
  CHECK(homemenu::at(shortcuts, 5) == chessId);
  CHECK(homemenu::addShortcut(shortcuts, 1, "HABITS") == homemenu::Id::Count);
  CHECK(homemenu::setLabel(shortcuts, chessId, "My chess"));
  homemenu::encode(shortcuts, 17, menuBytes);
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(generation == 17 && !std::strcmp(homemenu::shortcut(restored, chessId)->title, "CHESS"));
  CHECK(!std::strcmp(restored.labels[static_cast<int>(chessId)], "My chess"));
  homemenu::removeShortcut(shortcuts, chessId);
  CHECK(homemenu::visibleCount(shortcuts) == 6);
  CHECK(homemenu::findShortcut(shortcuts, 0, "CHESS") == homemenu::Id::Count);
  CHECK(homemenu::addShortcut(shortcuts, 1, "HABITS") == chessId);
  CHECK(!shortcuts.labels[static_cast<int>(chessId)][0]);
  // Retiring an app removes its saved Home entry without consuming another slot or label.
  homemenu::Config retired;
  CHECK(homemenu::setVisible(retired, homemenu::Id::Games, false));
  CHECK(homemenu::setVisible(retired, homemenu::Id::Apps, false));
  const auto retiredId = homemenu::addShortcut(retired, 1, "BESTIARY");
  const auto questId = homemenu::addShortcut(retired, 1, "READING QUEST");
  CHECK(retiredId != homemenu::Id::Count && questId != homemenu::Id::Count);
  CHECK(homemenu::setLabel(retired, retiredId, "My creatures"));
  CHECK(homemenu::setLabel(retired, questId, "My adventure"));
  homemenu::encode(retired, 18, menuBytes);
  CHECK(homemenu::decode(menuBytes, restored, generation));
  CHECK(generation == 18 && homemenu::visibleCount(restored) == 6);
  CHECK(homemenu::findShortcut(restored, 1, "BESTIARY") == homemenu::Id::Count);
  CHECK(!homemenu::shortcut(restored, retiredId)->title[0]);
  CHECK(!restored.labels[static_cast<int>(retiredId)][0]);
  CHECK(homemenu::findShortcut(restored, 1, "READING QUEST") == questId);
  CHECK(!std::strcmp(restored.labels[static_cast<int>(questId)], "My adventure"));
  CHECK(homemenu::addShortcut(restored, 1, "HABITS") == retiredId);
  // HMC1 preferences survive upgrading to the shortcut-capable codec.
  uint8_t legacy[homemenu::kLegacySaveBytes]{};
  std::memcpy(legacy, "HMC1", 4);
  legacy[4] = 4;
  legacy[8] = 0x3f;
  std::memcpy(legacy + 12 + 3 * homemenu::kLabelBytes, "My settings", 12);
  const uint32_t legacyHash = homemenu::checksum(legacy, sizeof(legacy) - 4);
  for (int n = 0; n < 4; ++n) legacy[sizeof(legacy) - 4 + n] = static_cast<uint8_t>(legacyHash >> (8 * n));
  CHECK(homemenu::decode(legacy, restored, generation, sizeof(legacy)));
  CHECK(generation == 4 && homemenu::visibleCount(restored) == 6);
  CHECK(!std::strcmp(restored.labels[3], "My settings"));
  uint8_t oldDefault[homemenu::kLegacySaveBytes]{};
  std::memcpy(oldDefault, "HMC1", 4);
  oldDefault[8] = 0x3f;
  hash = homemenu::checksum(oldDefault, sizeof(oldDefault) - 4);
  for (int n = 0; n < 4; ++n) oldDefault[sizeof(oldDefault) - 4 + n] = hash >> (8 * n);
  CHECK(homemenu::decode(oldDefault, restored, generation, sizeof(oldDefault)));
  CHECK(homemenu::visibleCount(restored) == 7 && homemenu::at(restored, 2) == homemenu::Id::Transfer);
  testStorage = {};
  testStorage.files["/.crosspoint/home-menu-a.bin"] = {legacy, legacy + sizeof(legacy)};
  homemenu::Store migrated;
  migrated.load();
  CHECK(!std::strcmp(migrated.get().labels[3], "My settings"));
  changed = migrated.get();
  CHECK(homemenu::setVisible(changed, homemenu::Id::Games, false));
  CHECK(homemenu::addShortcut(changed, 0, "2048") != homemenu::Id::Count);
  CHECK(migrated.save(changed));
  homemenu::Store migratedReload;
  migratedReload.load();
  CHECK(homemenu::findShortcut(migratedReload.get(), 0, "2048") != homemenu::Id::Count);
  game2048::State state;
  state.cells[0] = state.cells[1] = state.cells[2] = state.cells[3] = 1;
  CHECK(game2048::slide(state, game2048::Direction::Left));
  CHECK(state.cells[0] == 2 && state.cells[1] == 2 && state.cells[2] == 0 && state.score == 8);
  CHECK(game2048::slide(state, game2048::Direction::Right));
  CHECK(state.cells[3] == 3 && state.score == 16);
  game2048::start(state, 123);
  CHECK(std::count_if(std::begin(state.cells), std::end(state.cells), [](int x) { return x > 0; }) == 2);
  for (int turn = 0; turn < 1000 && game2048::canMove(state); ++turn) {
    uint64_t before = 0;
    for (int n : state.cells)
      if (n) before += uint64_t(1) << n;
    const bool changed = game2048::move(state, static_cast<game2048::Direction>(turn % 4));
    uint64_t after = 0;
    for (int n : state.cells) {
      CHECK(n <= 31);
      if (n) after += uint64_t(1) << n;
    }
    CHECK(changed ? (after == before + 2 || after == before + 4) : after == before);
  }
  uint8_t gameBytes[game2048::kSaveBytes];
  game2048::encode(state, state.score + 10, 73, gameBytes);
  game2048::State other;
  uint64_t best = 0;
  CHECK(game2048::decode(gameBytes, other, best, generation));
  CHECK(generation == 73 && best == state.score + 10 && other.random == state.random);
  for (size_t n = 0; n < sizeof(gameBytes); ++n) {
    gameBytes[n] ^= 1;
    CHECK(!game2048::decode(gameBytes, other, best, generation));
    gameBytes[n] ^= 1;
  }
  game2048::State full;
  for (int n = 0; n < 16; ++n) full.cells[n] = 1 + (n + n / 4) % 2;
  CHECK(!game2048::canMove(full));
  const auto rng = full.random;
  CHECK(!game2048::move(full, game2048::Direction::Left));
  CHECK(full.random == rng);
  for (int day = 1; day <= 84005; ++day) {
    const auto d = readingstats::dateFromDay(day);
    CHECK(readingstats::dayIndex(d) == day);
  }
  CHECK(readingstats::daysInMonth(2000, 2) == 29);
  CHECK(readingstats::daysInMonth(2100, 2) == 28);
  readingstats::Record r;
  r.seconds = 120;
  r.pages = 3;
  r.localDay = readingstats::dayIndex({2026, 10, 14});
  r.secondOfDay = 86340;
  r.epoch = 1792018740;
  readingstats::copyText(r.book.document, sizeof(r.book.document), document);
  r.book.progressBp = 10000;
  readingstats::Book empty;
  readingstats::updateBook(r, empty);
  CHECK(r.firstCompletion && r.book.completed && r.book.readingDays == 2);
  CHECK(r.book.buckets[3] == 60 && r.book.buckets[0] == 60);
  readingstats::Summary summary;
  summary.anchorDay = r.localDay + 1;
  summary.add(r);
  CHECK(summary.onDay(r.localDay) == 60 && summary.onDay(r.localDay + 1) == 60);
  CHECK(summary.currentStreak(r.localDay + 1) == 2 && summary.currentStreak(r.localDay + 2) == 2 &&
        summary.currentStreak(r.localDay + 3) == 0);
  const auto previous = r.book;
  readingstats::updateBook(r, previous);
  CHECK(!r.firstCompletion && r.book.sessions == 2 && r.book.readingDays == 2);
  uint8_t bytes[readingstats::kRecordBytes];
  readingstats::encode(r, bytes);
  readingstats::Record decoded;
  CHECK(readingstats::decode(bytes, decoded));
  CHECK(decoded.book.seconds == 240);
  for (size_t n = 0; n < sizeof(bytes); ++n) {
    bytes[n] ^= 1;
    CHECK(!readingstats::decode(bytes, decoded));
    bytes[n] ^= 1;
  }
  // Period partitions retain every second/page, including midnight and 4-hour boundaries.
  readingstats::PeriodStats left, right, week, month;
  r.localDay = readingstats::dayIndex({2026, 10, 31});
  r.secondOfDay = 86340;
  r.seconds = 120;
  r.pages = 3;
  r.firstCompletion = true;
  readingstats::setPeriod(left, readingstats::Period::Day, r.localDay, false);
  readingstats::setPeriod(right, readingstats::Period::Day, r.localDay + 1, false);
  left.add(r);
  right.add(r);
  CHECK(left.seconds == 60 && right.seconds == 60);
  CHECK(left.pages + right.pages == 3 && left.pages == 1 && right.pages == 2);
  CHECK(left.completed == 0 && right.completed == 1);
  CHECK(left.sessions == 1 && right.sessions == 1);
  CHECK(left.hours[5] == 60 && right.hours[0] == 60);
  CHECK(left.dailySeconds[0] == 60 && right.dailySeconds[0] == 60);
  readingstats::setPeriod(month, readingstats::Period::Month, r.localDay, false);
  month.add(r);
  CHECK(month.seconds == 60 && month.completed == 0 && month.pages == 1);
  readingstats::setPeriod(month, readingstats::Period::Month, r.localDay + 1, false);
  month.add(r);
  CHECK(month.seconds == 60 && month.completed == 1 && month.pages == 2);
  // Finishing exactly at midnight belongs to the finish date, even with no dwell that day.
  r.seconds = 60;
  readingstats::setPeriod(left, readingstats::Period::Day, r.localDay, false);
  readingstats::setPeriod(right, readingstats::Period::Day, r.localDay + 1, false);
  left.add(r);
  right.add(r);
  CHECK(left.seconds == 60 && left.completed == 0);
  CHECK(right.seconds == 0 && right.completed == 1 && right.sessions == 0);
  r.seconds = 120;
  CHECK(month.last - month.first == 29);
  readingstats::setPeriod(month, readingstats::Period::Month, readingstats::dayIndex({2024, 2, 10}), false);
  CHECK(month.last - month.first == 28);
  readingstats::setPeriod(month, readingstats::Period::Month, readingstats::dayIndex({2100, 2, 10}), false);
  CHECK(month.last - month.first == 27);
  readingstats::setPeriod(week, readingstats::Period::Week, r.localDay, false);
  week.add(r);
  CHECK(week.seconds == 120 && week.completed == 1 && week.pages == 3);
  CHECK(readingstats::weekday(week.first) == 0 && week.last - week.first == 6);
  readingstats::setPeriod(week, readingstats::Period::Week, r.localDay, true);
  week.add(r);
  CHECK(week.seconds == 60 && week.completed == 0);
  CHECK(readingstats::weekday(week.first) == 6);
  CHECK(readingstats::isoWeek(readingstats::dayIndex({2021, 1, 1})) == 53);
  CHECK(readingstats::isoWeek(readingstats::dayIndex({2021, 1, 4})) == 1);
  r.localDay = readingstats::dayIndex({2026, 10, 15});
  r.secondOfDay = 14340;
  readingstats::setPeriod(left, readingstats::Period::Day, r.localDay, false);
  left.add(r);
  CHECK(left.hours[0] == 60 && left.hours[1] == 60);
  for (int seconds : {1, 120, 86400, 172801}) {
    r.secondOfDay = 43123;
    r.seconds = seconds;
    r.pages = 127;
    uint64_t partitionTime = 0, partitionPages = 0;
    uint32_t completions = 0;
    for (int d = 0; d < 4; ++d) {
      readingstats::setPeriod(left, readingstats::Period::Day, r.localDay + d, false);
      left.add(r);
      partitionTime += left.seconds;
      partitionPages += left.pages;
      completions += left.completed;
      uint64_t bins = 0;
      for (auto n : left.hours) bins += n;
      CHECK(bins == left.seconds);
    }
    CHECK(partitionTime == r.seconds && partitionPages == r.pages && completions == 1);
  }
  // Consistency is not limited by the 730-day display window or journal date ordering.
  readingstats::Streak streak;
  const int cutoff = readingstats::dayIndex({2026, 10, 17});
  streak.begin(cutoff);
  for (int n = 0; n < 800; ++n) {
    r.localDay = cutoff - n;
    r.secondOfDay = 1000;
    r.seconds = 10;
    streak.add(r);
  }
  CHECK(streak.days() == 800 && streak.first == streak.minimumDay);
  streak.begin(cutoff - 700);
  for (int n = 0; n < 800; ++n) {
    r.localDay = cutoff - n;
    streak.add(r);
  }
  CHECK(streak.days() == 100);
  streak.begin(cutoff);
  const int unordered[] = {1, 0, 2};
  int passes = 0;
  for (;;) {
    const int before = streak.first;
    for (int n : unordered) {
      r.localDay = cutoff - 2 + n;
      streak.add(r);
    }
    ++passes;
    if (before == streak.first || streak.first == streak.minimumDay) break;
    CHECK(passes < 4);
  }
  CHECK(streak.days() == 3 && passes == 2);
  streak.begin(cutoff);
  r.localDay = cutoff + 1;
  streak.add(r);
  CHECK(streak.days() == 0);
  r.localDay = cutoff - 1;
  r.secondOfDay = 86399;
  r.seconds = 2;
  streak.add(r);
  CHECK(streak.days() == 2);
  // Fixed recent window: distinct books, append order, latest cumulative snapshot, eviction and re-entry.
  readingstats::Book recent[readingstats::kRecentBooks]{};
  int recentCount = 0;
  for (int n = 0; n < 20; ++n) {
    readingstats::Book b;
    std::snprintf(b.document, sizeof(b.document), "%032x", n);
    b.sessions = n + 1;
    readingstats::addRecent(recent, recentCount, b);
    CHECK(recentCount == std::min(10, n + 1));
    CHECK(recent[0].sessions == static_cast<uint32_t>(n + 1));
    for (int i = 0; i < recentCount; ++i)
      for (int j = i + 1; j < recentCount; ++j) CHECK(std::strcmp(recent[i].document, recent[j].document));
  }
  auto repeated = recent[5];
  repeated.sessions = 99;
  readingstats::addRecent(recent, recentCount, repeated);
  CHECK(recentCount == 10 && recent[0].sessions == 99 && recent[1].sessions == 20);
  readingstats::copyText(repeated.document, sizeof(repeated.document), "00000000000000000000000000000000");
  readingstats::addRecent(recent, recentCount, repeated);
  CHECK(recentCount == 10 && !std::strcmp(recent[0].document, repeated.document));
  repeated.completed = true;
  readingstats::addRecent(recent, recentCount, repeated);
  CHECK(recentCount == 9);
  for (int n = 0; n < recentCount; ++n) CHECK(std::strcmp(recent[n].document, repeated.document));
  readingstats::addRecent(recent, recentCount, repeated);
  CHECK(recentCount == 9);
  CHECK(readingstats::kBooksPerPage == 5);
  ReaderSession session;
  session.onRenderComplete(0, 1792018700, 100);
  session.noteTurn(true, true);
  session.onRenderComplete(1000, 1792018701, 200);
  CHECK(session.pagesRead() == 1 && session.durationSeconds() == 1 && session.isEmitWorthy());
  session.noteTurn(true, true);
  session.onRenderComplete(11000, 1792018711, 300);
  CHECK(session.pagesRead() == 2 && session.durationSeconds() == 11 && session.isEmitWorthy());
  session.reset();
  CHECK(session.pagesRead() == 0);
  testStorage = {};
  readingstats::Repository journal;
  CHECK(journal.record(document, "Book", "Author", 1792018700, 120, 3, 5000));
  CHECK(journal.record(document, "Book", "Author", 1792018900, 120, 3, 10000));
  readingstats::Book loaded;
  CHECK(journal.loadBook(document, loaded));
  CHECK(loaded.sessions == 2 && loaded.completed && loaded.seconds == 240);
  const auto beforeCachedRead = testStorage.readBytes;
  CHECK(journal.cachedBook(document, loaded) && loaded.seconds == 240);
  CHECK(testStorage.readBytes - beforeCachedRead <= 512);
  const char* cache = "/.crosspoint/reading-history/0123456789abcdef0123456789abcdef.idx";
  const auto validCache = testStorage.files[cache];
  testStorage.files[cache][0] ^= 1;
  CHECK(!journal.cachedBook(document, loaded));
  testStorage.files[cache] = validCache;
  CHECK(!journal.cachedBook("fedcba9876543210fedcba9876543210", loaded));
  testStorage.files.erase("/.crosspoint/reading-history/0123456789abcdef0123456789abcdef.idx");
  CHECK(!journal.cachedBook(document, loaded));
  CHECK(journal.loadBook(document, loaded));
  CHECK(loaded.sessions == 2);
  auto& log = testStorage.files["/.crosspoint/reading-history/sessions.bin"];
  const auto size = log.size();
  log.push_back(1);
  CHECK(journal.record(document, "Book", "Author", 1792019200, 60, 2, 10000));
  CHECK(log.size() == size + readingstats::kRecordBytes);
  CHECK(journal.begin());
  int records = 0, finished = 0;
  for (;;) {
    const auto result = journal.next(decoded);
    if (result == readingstats::ScanResult::Finished) break;
    if (result == readingstats::ScanResult::Record) {
      ++records;
      finished += decoded.firstCompletion;
    }
  }
  CHECK(records == 3 && finished == 1);
  journal.close();
  uint8_t readAhead[8 * readingstats::kRecordBytes];
  for (bool reverse : {false, true})
    for (uint32_t capacity :
         {0u, 1u, uint32_t(readingstats::kRecordBytes), uint32_t(sizeof(readAhead) - 1), uint32_t(sizeof(readAhead))}) {
      CHECK(journal.begin(reverse, readAhead, capacity));
      records = finished = 0;
      for (;;) {
        const auto result = journal.next(decoded);
        if (result == readingstats::ScanResult::Finished) break;
        CHECK(result != readingstats::ScanResult::Error);
        if (result == readingstats::ScanResult::Record) {
          ++records;
          finished += decoded.firstCompletion;
        }
      }
      CHECK(records == 3 && finished == 1);
      journal.close();
    }
  log[16 + 40] ^= 1;
  CHECK(journal.begin());
  records = 0;
  for (;;) {
    const auto result = journal.next(decoded);
    if (result == readingstats::ScanResult::Finished) break;
    if (result == readingstats::ScanResult::Record) ++records;
  }
  CHECK(records == 2 && journal.hadDamage());
  journal.close();
  testStorage.writeLimit = 100;
  CHECK(!journal.record(document, "Book", "Author", 1792019400, 60, 2, 10000));
  testStorage.writeLimit = -1;
  CHECK(journal.record(document, "Book", "Author", 1792019400, 60, 2, 10000));
  log[3] = '2';
  CHECK(!journal.record(document, "Book", "Author", 1792019500, 60, 2, 10000));
  testStorage = {};
  game2048::Repository game;
  CHECK(game.load(other, best));
  CHECK(game.save(state, 20000));
  game2048::Repository gameReload;
  CHECK(gameReload.load(other, best));
  CHECK(best >= 20000 && other.score == state.score);
  testStorage.writeLimit = 20;
  CHECK(!gameReload.save(state, 21000));
  testStorage.writeLimit = -1;
  game2048::Repository recovered;
  CHECK(recovered.load(other, best));
  CHECK(best == 20000);
  testStorage = {};
  testStorage.available = false;
  readingstats::Repository unavailable;
  CHECK(!unavailable.begin());
  game2048::Repository unavailableGame;
  CHECK(!unavailableGame.load(other, best));
  for (int year : {2023, 2024, 2026}) {
    readingstats::PeriodStats annual;
    const int jan = readingstats::dayIndex({year, 1, 1});
    const int end = readingstats::dayIndex({year + 1, 1, 1});
    readingstats::setPeriod(annual, readingstats::Period::Year, jan, false);
    CHECK(annual.first == jan && annual.last == end - 1 && annual.annual);
    for (int day = jan - 1; day <= end; ++day) {
      readingstats::Record r{};
      r.localDay = day;
      r.secondOfDay = 23 * 3600 + 59 * 60;
      r.seconds = 120;
      r.pages = 2;
      r.firstCompletion = true;
      annual.add(r);
    }
    CHECK(annual.seconds == uint64_t(end - jan) * 120);
    CHECK(annual.daysReadThrough(end) == end - jan);
    CHECK(annual.daysReadThrough(jan - 1) == 0 && annual.daysReadThrough(jan) == 1);
    uint64_t total = 0;
    for (int month = 1; month <= 12; ++month) {
      CHECK(annual.annualSeconds[month - 1] == uint64_t(readingstats::daysInMonth(year, month)) * 120);
      CHECK(annual.annualCompleted[month - 1] == uint32_t(readingstats::daysInMonth(year, month)));
      total += annual.annualSeconds[month - 1];
    }
    CHECK(total == annual.seconds && annual.pages == uint64_t(end - jan) * 2);
    CHECK(annual.completed == uint32_t(end - jan));
  }
  std::printf("Reading workspace sizes: Summary=%zu PeriodStats=%zu Book=%zu\n", sizeof(readingstats::Summary),
              sizeof(readingstats::PeriodStats), sizeof(readingstats::Book));
  std::printf("reading suite core/store: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
