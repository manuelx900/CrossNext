#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../../lib/FsHelpers/ContentVisibility.h"
#include "../../src/apps_local/flashcards/AnkiSqlite.h"
#include "../../src/apps_local/flashcards/FlashText.h"
#include "../../src/apps_local/intercept/InterceptCore.h"
#include "../../src/apps_local/power/PowerButtonPolicy.h"
#include "../../src/apps_local/quest/QuestCore.h"
#include "../../src/apps_local/quotes/ClippingsLine.h"
#include "../../src/apps_local/tools/ToolState.h"
class MemorySource : public ankisql::Source {
 public:
  std::vector<uint8_t> bytes;
  bool read(uint64_t at, void* out, size_t count) override {
    if (at > bytes.size() || count > bytes.size() - at) return false;
    std::memcpy(out, bytes.data() + at, count);
    return true;
  }
  uint64_t size() const override { return bytes.size(); }
};
static unsigned checks = 0;
#undef assert
#define assert(condition)                                 \
  do {                                                    \
    ++checks;                                             \
    if (!(condition)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #condition); \
      std::exit(1);                                       \
    }                                                     \
  } while (0)
int main(int argc, char** argv) {
  assert(argc == 2);
  quotes::ClippingsLine clipping;
  for (int n = 0; n < 2 * 1024 * 1024; ++n) {
    if (clipping.consume('a')) std::exit(1);
  }
  assert(!clipping.consume('\n'));
  for (char c : std::string("==========\r\n")) {
    if (c == '\n')
      assert(clipping.consume(c));
    else
      assert(!clipping.consume(c));
  }
  for (char c : std::string("===========\n")) assert(!clipping.consume(c));

  for (int size : {512, 1024, 4096, 65536}) {
    const auto path = std::string(argv[1]) + "/anki-" + std::to_string(size) + ".sqlite";
    std::ifstream file(path, std::ios::binary);
    MemorySource source;
    source.bytes.assign(std::istreambuf_iterator<char>(file), {});
    assert(!source.bytes.empty());
    ankisql::Database database(source);
    assert(database.open());
    uint32_t notes, cards;
    assert(database.table("notes", notes) && database.table("cards", cards));
    assert(!database.table("missing", cards));
    assert(database.table("cards", cards));
    ankisql::Cursor cursor;
    assert(cursor.begin(database, cards));
    ankisql::Row row;
    int count = 0;
    while (cursor.next(row) == ankisql::Result::Row) {
      int64_t nid, ordinal, usn;
      assert(row.integer(1, nid) && nid == ++count);
      assert(row.id == 1000000000000ull + count);
      assert(row.integer(3, ordinal) && ordinal == count % 2);
      assert(row.integer(5, usn) && usn == -1);
      checks += 4;
    }
    assert(count == 300);
    char text[15000];
    assert(database.find(notes, 173, row));
    assert(row.text(6, text, sizeof(text)));
    assert(std::strlen(text) > 12000);
    assert(!row.text(6, text, 32));
    assert(!database.find(notes, 999, row));
    for (int n = 1; n <= 300; ++n) {
      assert(database.find(notes, n, row));
      assert(row.text(6, text, sizeof(text)));
      if (n != 173) {
        const auto expected = "Front " + std::to_string(n) + "\x1f" + "Back " + std::to_string(n);
        assert(text == expected);
      }
      ++checks;
    }
    source.bytes[0] = '!';
    assert(!database.open());
    source.bytes[0] = 'S';
    source.bytes.pop_back();
    assert(!database.open());
  }
  char out[1024];
  assert(flashtext::plain("<b>Rome</b><br>Italy &amp; Europe", out, sizeof(out)));
  assert(!std::strcmp(out, "Rome\nItaly & Europe"));
  assert(
      flashtext::plain("<div>&lt;ol&gt;</div><div>&lt;li&gt;carbon dating&lt;/li&gt;</div><div>&lt;li&gt;Nuclear "
                       "reactors&lt;/li&gt;</div>&lt;/ol&gt;",
                       out, sizeof(out)));
  assert(!std::strcmp(out, "1. carbon dating\n2. Nuclear reactors"));
  assert(flashtext::plain("$^{99} Tc$ emits $\\gamma$-ray $\\newline$ A $\\rightarrow$ isobar", out, sizeof(out)));
  assert(!std::strcmp(out, "^(99) Tc emits gamma-ray\nA  -> isobar"));
  assert(flashtext::plain("$\\frac{mv^2}{r}$ and $\\frac{a}{\\frac{b}{c}}$", out, sizeof(out)));
  assert(!std::strcmp(out, "(mv^2)/(r) and (a)/((b)/(c))"));
  assert(flashtext::plain("caf&#233; &alpha; &lt;example&gt; $5", out, sizeof(out)));
  assert(!std::strcmp(out, "caf\xc3\xa9 \xce\xb1 <example> $5"));
  assert(flashtext::plain("<DIV title='x > y'>first</DIV><div>second</div>", out, sizeof(out)));
  assert(!std::strcmp(out, "first\nsecond"));
  assert(flashtext::cloze("{{c1::Paris}} is in {{c2::France::country}}", 2, false, out, sizeof(out)));
  assert(!std::strcmp(out, "Paris is in [country]"));
  assert(flashtext::cloze("{{c1::Paris}}", 1, true, out, sizeof(out)));
  assert(!std::strcmp(out, "Paris"));
  assert(flashtext::plain(R"(\(\frac{1}{2}+\sqrt{x}\))", out, sizeof(out)));
  assert(!std::strcmp(out, "(1)/(2)+sqrt(x)"));
  assert(flashtext::plain(R"(\[\begin{pmatrix}1&2\\3&4\end{pmatrix}\])", out, sizeof(out)));
  assert(!std::strcmp(out, "1 | 2\n3 | 4"));
  assert(flashtext::plain(R"(\(\ce{SO4^{2-} + Ba^{2+} -> BaSO4}\))", out, sizeof(out)));
  assert(!std::strcmp(out, "SO4^(2-) + Ba^(2+) -> BaSO4"));
  assert(flashtext::plain("H<sub>2</sub>O x<sup>2</sup><pre>if (x) {\n    y++;\n}</pre>", out, sizeof(out)));
  assert(!std::strcmp(out, "H_(2)O x^(2)\nif (x) {\n    y++;\n}"));
  assert(
      flashtext::plain("<table><tr><th>A</th><td>B</td></tr><tr><td>C</td><td>D</td></tr></table>", out, sizeof(out)));
  assert(!std::strcmp(out, "A | B\nC | D"));
  assert(flashtext::plain("SAFE<script>throw 'no'</script><style>hide</style> YES", out, sizeof(out)));
  assert(!std::strcmp(out, "SAFE YES"));
  for (size_t length : {size_t(4096), size_t(32768), size_t(65535)}) {
    const std::string ampersands(length, '&');
    std::vector<char> converted(length + 1);
    assert(flashtext::plain(ampersands.c_str(), converted.data(), converted.size()));
    assert(ampersands == converted.data());
  }
  assert(!flashtext::plain("12345", out, 3));
  assert(!flashtext::cloze("{{c1::broken", 1, false, out, sizeof(out)));
  assert(content::hiddenContent("/FLASHCARDS/Biology/a.csv"));
  assert(content::hiddenContent("/sleep/a.bmp"));
  assert(content::hiddenContent("/WALLPAPER/x.png"));
  assert(!content::hiddenContent("/Books/sleep.txt"));
  for (const auto* path : {"/fonts/Inter/Regular.ttf", "/LICENSES/OFL.txt", "/apps/reading-adventure/graphics.bin",
                           "/dictionaries/Spanish Wiktionary/es-es.dict"})
    assert(content::hiddenContent(path));
  assert(!content::hiddenContent("/Books/Fonts.epub"));
  assert(!content::hiddenContent("/Books/fonts/Manual.epub"));
  assert(readingstats::enabledFormat("/a.EPUB", 7));
  assert(!readingstats::enabledFormat("/m.xtc", 7));
  assert(!readingstats::enabledFormat("/x.jpg", 7));
  assert(readingstats::enabledFormat("/x.JPG", 32));
  assert(!readingstats::enabledFormat("a.unknown", 63));
  uint8_t record[quest::kProfileBytes];
  tools::State tool;
  tool.count = 999999999;
  tool.analog = true;
  tool.orientation = 3;
  tools::encode(tool, record);
  tools::State recovered;
  assert(tools::decode(record, recovered) && recovered.count == tool.count && recovered.orientation == 3);
  record[20] ^= 1;
  assert(!tools::decode(record, recovered));
  powermenu::ButtonPolicy power;
  using BA = powermenu::ButtonAction;
  assert(power.update(true, false, 800, true) == BA::None);
  assert(power.update(false, true, 800, true) == BA::None);
  assert(power.update(true, false, 0, true) == BA::None);
  assert(power.update(false, true, 100, true) == BA::Sleep);
  assert(power.update(true, false, 0, true) == BA::None);
  assert(power.update(true, false, 700, true) == BA::Menu);
  assert(power.update(true, false, 900, true) == BA::None);
  assert(power.update(false, true, 900, true) == BA::None);
  assert(power.update(true, false, 700, false) == BA::None);
  assert(power.update(false, true, 800, true) == BA::None);
  quest::Profile profile;
  readingstats::Summary summary;
  summary.seconds = 36000;
  summary.completed = 3;
  summary.totalDays = 8;
  quest::refresh(profile, summary);
  const auto xp = profile.xp;
  assert(quest::level(xp) > 1);
  assert(!quest::claim(profile, 20000, 899));
  assert(quest::claim(profile, 20000, 900));
  assert(!quest::claim(profile, 20000, 900));
  assert(!quest::claim(profile, 19999, 900));
  assert(!quest::claim(profile, 20001, 899));
  assert(quest::equip(profile, 1));
  const auto spent = profile.spent;
  assert(quest::equip(profile, 1) && profile.spent == spent);
  assert(!quest::equip(profile, quest::kGearCount));
  quest::encode(profile, record);
  quest::Profile p2;
  assert(quest::decode(record, p2) && p2.equipped[0] == 1);
  record[16] ^= 1;
  assert(!quest::decode(record, p2));
  quest::refresh(profile, {});
  assert(profile.xp >= xp);
  for (uint32_t level = 1; level <= 100; ++level) {
    assert(quest::level(quest::threshold(level)) == level);
    ++checks;
  }
  quest::Profile rich;
  rich.xp = 1000000000;
  assert(quest::count(quest::Slot::Body) == 100 && quest::count(quest::Slot::Aura) == 50 &&
         quest::count(quest::Slot::Pet) == 50);
  for (int slot = 0; slot < quest::kSlotCount; ++slot) {
    for (int n = quest::first(quest::Slot(slot)); n < quest::first(quest::Slot(slot)) + quest::count(quest::Slot(slot));
         ++n) {
      assert(int(quest::slot(n)) == slot);
      const bool wasOwned = quest::owns(rich, n);
      const int shopBefore = quest::shopCount(rich, quest::Slot(slot));
      assert(quest::equip(rich, n));
      assert(quest::shopCount(rich, quest::Slot(slot)) == shopBefore - (wasOwned ? 0 : 1));
      assert(quest::shopCount(rich, quest::Slot(slot)) + quest::ownedCount(rich, quest::Slot(slot)) ==
             quest::count(quest::Slot(slot)));
      for (int index = 0; index < quest::shopCount(rich, quest::Slot(slot)); ++index)
        assert(!quest::owns(rich, quest::shopItem(rich, quest::Slot(slot), index)));
      const auto paid = rich.spent;
      assert(quest::equip(rich, n) && rich.spent == paid && rich.equipped[slot] == n);
      assert(quest::requiredLevel(n) <= 100);
      ++checks;
    }
  }
  for (int slot = 0; slot < quest::kSlotCount; ++slot) {
    assert(quest::shopCount(rich, quest::Slot(slot)) == 0);
    assert(quest::shopItem(rich, quest::Slot(slot), 0) == quest::kGearCount);
    assert(quest::shopItem(rich, quest::Slot(slot), -1) == quest::kGearCount);
  }
  summary.seconds = 1000000000;
  summary.pages = 1000000;
  summary.sessions = 1000000;
  summary.completed = 10000;
  summary.totalDays = 10000;
  summary.longest = 10000;
  for (int q = 0; q < quest::kQuestCount; ++q) {
    const auto beforeReward = rich.xp;
    assert(quest::claimMilestone(rich, q, summary));
    assert(rich.xp - beforeReward == quest::questReward(q));
    const auto earned = rich.xp;
    assert(!quest::claimMilestone(rich, q, summary) && earned == rich.xp);
    ++checks;
  }
  rich.gender = quest::Gender::Feminine;
  quest::encode(rich, record);
  assert(quest::decode(record, p2));
  for (int q = 0; q < quest::kQuestCount; ++q) assert(quest::isClaimed(p2, q));
  for (int slot = 0; slot < quest::kSlotCount; ++slot) assert(p2.equipped[slot] == rich.equipped[slot]);
  assert(p2.gender == quest::Gender::Feminine);
  assert(quest::questReward(0) == 10 && quest::questReward(5) == 10);
  assert(quest::questReward(6) == 10 && quest::questReward(999) == 30);
  assert(quest::questReward(-1) == 0 && quest::questReward(1500) == 0);
  assert(!quest::claimMilestone(rich, quest::kQuestCount, summary));
  assert(!quest::claimMilestone(rich, -1, summary));
  // Level gifts are entitlements, independent of dismissing the notification.
  quest::Profile growing;
  uint64_t gifts = 0;
  for (uint32_t n = 2; n <= quest::MAX_LEVEL; ++n) {
    growing.xp = quest::threshold(n);
    gifts += quest::levelRewardCoins(n);
    assert(quest::coins(growing) == growing.xp / 10 + gifts);
    assert(quest::pendingLevelCoins(growing) == quest::levelRewardCoins(n));
    assert(quest::hasLevelUp(growing));
    const auto balance = quest::coins(growing);
    assert(quest::acknowledgeLevel(growing));
    assert(!quest::acknowledgeLevel(growing) && quest::coins(growing) == balance);
    quest::encode(growing, record);
    assert(quest::decode(record, p2) && !quest::hasLevelUp(p2) && quest::coins(p2) == balance);
    ++checks;
  }
  assert(gifts == 12078 && quest::threshold(100) == 47124);
  assert(quest::levelRewardCoins(1) == 0 && quest::levelRewardCoins(101) == 0);
  growing.noticedLevel = 1;
  assert(quest::pendingLevelCoins(growing) == gifts);
  quest::encode(growing, record);
  assert(quest::decode(record, p2) && quest::pendingLevelCoins(p2) == gifts);
  // RQU5 permits spending earned level gifts; RQU4 retains its old wallet rules.
  growing.spent = growing.xp / 10 + 100;
  quest::encode(growing, record);
  assert(quest::decode(record, p2));
  std::memcpy(record, "RQU4", 4);
  const auto crc4 = readingstats::checksum(record, 316);
  for (int b = 0; b < 4; ++b) record[316 + b] = crc4 >> (8 * b);
  assert(!quest::decodeV4(record, p2));
  growing.noticedLevel = 0;
  quest::encode(growing, record);
  assert(!quest::decode(record, p2));
  growing.noticedLevel = 101;
  quest::encode(growing, record);
  assert(!quest::decode(record, p2));
  // A completed streak survives a break; its bonus cannot be collected twice.
  quest::Profile streakProfile;
  readingstats::Summary streakStats;
  streakStats.longest = 7;
  const int weeklyStreak = (7 - 1) * 6 + 5;
  assert(quest::questReward(weeklyStreak) == 80 && quest::questCoinReward(weeklyStreak) == 33);
  assert(quest::claimMilestone(streakProfile, weeklyStreak, streakStats));
  assert(quest::coins(streakProfile) == 33);
  assert(!quest::claimMilestone(streakProfile, weeklyStreak, streakStats));
  quest::encode(streakProfile, record);
  assert(quest::decode(record, p2) && quest::coins(p2) == 33);
  assert(!quest::claimMilestone(p2, weeklyStreak, streakStats));
  assert(quest::questCoinReward(-1) == 0 && quest::questCoinReward(1500) == 0);
  // Ready rewards lead, incomplete goals follow, and collected rewards move last.
  quest::Profile fresh;
  readingstats::Summary small;
  small.seconds = 1800;
  assert(quest::orderedQuest(fresh, small, 0) == 0);
  assert(quest::claimMilestone(fresh, 0, small));
  assert(quest::orderedQuest(fresh, small, 0) == 1);
  assert(quest::orderedQuest(fresh, small, quest::kQuestCount - 1) == 0);
  bool ordered[quest::kQuestCount]{};
  for (int n = 0; n < quest::kQuestCount; ++n) {
    const int q = quest::orderedQuest(fresh, small, n);
    assert(q >= 0 && q < quest::kQuestCount && !ordered[q]);
    ordered[q] = true;
  }
  assert(quest::orderedQuest(fresh, small, -1) == -1);
  assert(quest::orderedQuest(fresh, small, quest::kQuestCount) == -1);
  // Explore every reachable story state, including conditional choices.
  std::vector<intercept::State> queue(1);
  queue.reserve(4096);
  bool seen[64][64]{};
  seen[0][0] = true;
  unsigned endings[6]{};
  for (size_t at = 0; at < queue.size(); ++at) {
    auto state = queue[at];
    assert(state.node < intercept::nodeCount);
    const auto& node = intercept::nodes[state.node];
    ++endings[unsigned(node.ending)];
    unsigned available = 0;
    for (unsigned c = 0; c < node.count; ++c) {
      if (!intercept::allowed(state, c)) continue;
      ++available;
      auto next = state;
      assert(intercept::choose(next, c));
      assert(next.choices == state.choices + 1);
      intercept::encode(next, record);
      intercept::State restored;
      assert(intercept::decode(record, restored) && restored.node == next.node && restored.flags == next.flags);
      if (!seen[next.node][next.flags]) {
        seen[next.node][next.flags] = true;
        queue.push_back(next);
      }
      ++checks;
    }
    assert(node.ending != intercept::Ending::None || available > 0);
  }
  for (int n = 1; n < 6; ++n) assert(endings[n] > 0);
  std::printf("PASS: %u SQLite/import/scheduling/profile/story checks; %zu reachable story states\n", checks,
              queue.size());
}
