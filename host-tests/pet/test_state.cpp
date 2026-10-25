#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../../src/apps_local/pet/PetNeeds.h"
#include "../../src/apps_local/pet/PetReadingTimeline.h"
#include "../../src/apps_local/pet/PetState.h"

static size_t saveLength(const uint8_t* bytes) { return bytes[3] >= '6' ? pet::STATE_BYTES : pet::LEGACY_STATE_BYTES; }
static bool testDecode(const uint8_t* bytes, pet::Profile& p) { return pet::decode(bytes, p, saveLength(bytes)); }
static void testReadingChronology() {
  constexpr int64_t start = 1700000000;
  for (const auto mode : {pet::Mode::Normal, pet::Mode::Hard, pet::Mode::Extreme}) {
    pet::Profile p;
    p.created = true;
    p.mode = mode;
    p.readingSeconds = 72000;
    p.careAt = p.lastReadAt = start;
    const auto grace = pet::absenceGrace(mode);
    pet::ReadingTimeline timeline;
    timeline.begin(p, start + grace + 100);
    timeline.add(start + grace + 1, 60);
    timeline.finish();
    assert(timeline.valid() && timeline.endedAt() == start + grace);

    timeline.begin(p, start + grace + 100);
    timeline.add(start + grace - 10, 60);
    timeline.finish();
    assert(timeline.valid() && timeline.endedAt() == 0);

    timeline.begin(p, start + grace + 100);
    timeline.add(start + grace, 60);
    timeline.finish();
    assert(timeline.endedAt() == start + grace);

    // A continuous reading session can span the original deadline.
    timeline.begin(p, start + grace + 100);
    timeline.add(start + grace - 10, 90);
    timeline.finish();
    assert(timeline.valid() && timeline.endedAt() == 0);

    timeline.begin(p, start + grace * 3);
    timeline.add(start + grace / 2, 60);
    timeline.add(start + grace + grace / 2 + 61, 60);
    timeline.finish();
    assert(timeline.endedAt() == start + grace + grace / 2 + 60);

    timeline.begin(p, start + grace * 3);
    timeline.add(start + grace + 1, 60);
    timeline.add(start + 1, 60);
    assert(!timeline.valid());
    timeline.begin(p, start + 100);
    timeline.add(start + 101, 1);
    assert(!timeline.valid());
    timeline.begin(p, start + 100);
    timeline.add(start + 90, 20);
    assert(!timeline.valid());

    p.lastReadAt = 0;
    timeline.begin(p, start + grace * 4);
    timeline.add(start, 60);
    timeline.finish();
    assert(timeline.migration() && !timeline.endedAt());
    p.lastReadAt = start;
    p.mode = pet::Mode::Relaxed;
    timeline.begin(p, start + grace * 4);
    timeline.finish();
    assert(!timeline.endedAt());
  }
  pet::Profile a;
  a.created = true;
  a.readingSeconds = 72000;
  a.joy = 20;
  auto b = a;
  pet::addReading(a, 72300);
  for (unsigned n = 1; n <= 30; ++n) pet::addReading(b, 72000 + n * 10);
  assert(a.joy == 30 && b.joy == a.joy);
  assert(!pet::addReading(b, 72300) && b.joy == 30);
  a.careAt = start;
  a.joy = 100;
  b = a;
  pet::advanceCare(a, start + 96 * 3600);
  for (unsigned n = 1; n <= 96; ++n) pet::advanceCare(b, start + n * 3600);
  assert(a.joy == 20 && a.joy == b.joy);  // Relaxed floor is preserved in both paths.
  a.mode = pet::Mode::Normal;
  a.careAt = start;
  a.joy = 100;
  b = a;
  pet::advanceCare(a, start + 96 * 3600);
  for (unsigned n = 1; n <= 96; ++n) pet::advanceCare(b, start + n * 3600);
  assert(a.joy == 4 && a.joy == b.joy);
}

static void testMoments() {
  pet::Profile empty, before, p, loaded;
  p.created = true;
  pet::captureMoments(p, empty);
  assert(p.momentMask == (1u << 2) && pet::momentStage(p.moments[2]) == pet::Stage::Egg);
  for (unsigned slot = 0; slot < 4; ++slot) assert(pet::momentObject(p.moments[2], slot) == 255);
  before = p;
  for (unsigned slot = 0; slot < 4; ++slot) {
    const auto item = pet::objectAt(slot, 59);
    pet::own(p, item);
    p.equipped[slot] = uint8_t(item);
  }
  p.readingSeconds = 72000;
  p.memories = 31;
  p.resting = true;
  p.gameDay = 20000;
  pet::captureMoments(p, before);
  assert((p.momentMask & ((1u << pet::LEGACY_MOMENT_COUNT) - 1)) == (1u << pet::LEGACY_MOMENT_COUNT) - 1);
  for (unsigned slot = 0; slot < 4; ++slot) assert(pet::momentObject(p.moments[0], slot) == pet::objectAt(slot, 59));
  for (unsigned i : {0u, 6u, 7u, 8u}) assert(unsigned(pet::momentStage(p.moments[i])) == (i ? i - 4 : 1));
  const auto firstFood = p.moments[3];
  before = p;
  for (auto& item : p.equipped) item = 255;
  pet::captureMoments(p, before);
  assert(p.moments[3] == firstFood);
  uint8_t bytes[pet::STATE_BYTES];
  pet::encode(p, bytes);
  assert(testDecode(bytes, loaded) && loaded.momentMask == p.momentMask);
  assert(!std::memcmp(loaded.moments, p.moments, sizeof(p.moments)));
  const auto seal = [&] {
    const auto hash = pet::checksum(bytes, saveLength(bytes) - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(hash >> (8 * i));
  };
  bytes[3] = '4';
  bytes[205] = 0;
  seal();
  assert(testDecode(bytes, loaded) && loaded.momentMask == 0);
  pet::encode(p, bytes);
  bytes[217] = 128;
  seal();
  assert(!testDecode(bytes, loaded));
  pet::encode(p, bytes);
  bytes[218] = 60;
  seal();
  assert(!testDecode(bytes, loaded));
  pet::encode(p, bytes);
  bytes[221] = 255;
  seal();
  assert(!testDecode(bytes, loaded));
  pet::encode(p, bytes);
  bytes[205] = 3;
  seal();
  assert(!testDecode(bytes, loaded));
  pet::Profile hungry, old;
  hungry.created = true;
  hungry.readingSeconds = 1200;
  hungry.energy = 22;
  hungry.saciety = 10;
  old = hungry;
  hungry.memories = 4;
  pet::captureMoments(hungry, old);
  assert(pet::momentPose(hungry.moments[3]) == pet::MomentPose::Hungry);
  pet::Profile next;
  next.created = true;
  next.generation = 2;
  pet::captureMoments(next, p);
  assert(next.momentMask == (1u << 2));
}

static void testMilestones() {
  pet::Profile p, before, loaded;
  p.created = true;
  p.bornDay = 20000;
  pet::captureMoments(p, before, 20000);
  assert(p.momentDays[2] == 20000 && p.momentReading[2] == 0);
  before = p;
  p.readingSeconds = 3600;
  pet::captureMoments(p, before, 20001);
  const unsigned reading = unsigned(pet::Moment::Reading1h);
  assert((p.momentMask & (1u << reading)) && p.momentDays[reading] == 20001 && p.momentReading[reading] == 3600);
  p.energy = 20;
  p.saciety = 100;
  for (unsigned i = 0; i < 5; ++i) {
    before = p;
    assert(pet::recordGameWin(p));
    pet::captureMoments(p, before, 20002);
  }
  assert(p.gamesWon == 5 && (p.momentMask & (1u << unsigned(pet::Moment::Games5))));
  for (unsigned i = 0; i < 5; ++i) {
    before = p;
    p.illness = pet::Illness::Cold;
    p.bonusMedicine += 1;
    assert(pet::cure(p));
    pet::captureMoments(p, before, 20003);
  }
  assert(p.treatments == 5 && (p.momentMask & (1u << unsigned(pet::Moment::Treatments5))));
  before = p;
  p.hygiene = 20;
  assert(pet::clean(p));
  pet::captureMoments(p, before, 20004);
  assert(p.treatments == 5 && p.momentDays[reading] == 20001 && p.momentReading[reading] == 3600);
  uint8_t bytes[pet::STATE_BYTES];
  pet::encode(p, bytes);
  assert(pet::decode(bytes, loaded));
  assert(loaded.gamesWon == 5 && loaded.treatments == 5 && loaded.momentDays[reading] == 20001 &&
         loaded.momentReading[reading] == 3600);
  p.illness = pet::Illness::Cold;
  assert(!pet::recordGameWin(p));
  // A PET5 profile migrates scenes without inventing historical dates or times.
  pet::encode(loaded, bytes);
  bytes[3] = '5';
  bytes[205] = 1;
  bytes[206] = 4;
  bytes[207] = 0;
  for (unsigned i = 0; i < pet::LEGACY_MOMENT_COUNT; ++i)
    for (unsigned b = 0; b < 4; ++b) bytes[208 + i * 4 + b] = uint8_t(loaded.moments[i] >> (b * 8));
  const auto hash = pet::checksum(bytes, pet::LEGACY_STATE_BYTES - 4);
  for (unsigned b = 0; b < 4; ++b) bytes[pet::LEGACY_STATE_BYTES - 4 + b] = uint8_t(hash >> (b * 8));
  assert(pet::decode(bytes, p, pet::LEGACY_STATE_BYTES) && p.momentMask == 4 && !p.gamesWon && !p.momentDays[2]);
}

int main() {
  {
    const unsigned increments[] = {10, 15, 20, 30, 45, 60};
    uint64_t previous = 0;
    for (unsigned n = 0; n < pet::OBJECT_COUNT; ++n) {
      const auto threshold = pet::projectThreshold(n);
      assert(threshold - previous == uint64_t(increments[std::min(n, 5u)]) * 60);
      previous = threshold;
    }
    pet::Profile egg;
    egg.created = true;
    assert(pet::addReading(egg, 600));
    assert(pet::ownedCount(egg) == 0 && !pet::homeUnlocked(egg));
    assert(pet::addReading(egg, 1200));
    assert(pet::owns(egg, egg.project) && pet::ownedCount(egg) == 1);
    pet::Profile returning;
    returning.created = true;
    returning.readingSeconds = 72000;
    for (unsigned n = 0; n < 12; ++n) pet::own(returning, n);
    const auto credit = pet::projectReading(returning);
    const auto foodBefore = pet::food(returning);
    for (unsigned n = 12; n < 23; ++n) {
      assert(pet::chooseProject(returning, n));
      assert(pet::owns(returning, n) && pet::ownedCount(returning) == n + 1);
      assert(pet::projectReading(returning) == credit && pet::food(returning) == foodBefore);
    }
    assert(pet::chooseProject(returning, 23));
    assert(!pet::owns(returning, 23) && pet::projectRemaining(returning) == 3600);
    assert(pet::addReading(returning, 75599) && !pet::owns(returning, 23));
    assert(pet::addReading(returning, 75600) && pet::owns(returning, 23));
    for (unsigned n = 0; n < 12; ++n) assert(pet::owns(returning, n));
  }
  testReadingChronology();
  testMoments();
  testMilestones();
  char name[pet::NAME_BYTES]{};
  assert(!pet::finishName(name) && !pet::typeName(name, ' '));
  for (unsigned i = 0; i < pet::NAME_LIMIT; ++i) assert(pet::typeName(name, '['));
  assert(pet::nameLength(name) == 12 && pet::validName(name, false) && !pet::typeName(name, 'A'));
  assert(pet::eraseName(name) && pet::nameLength(name) == 11);
  name[0] = 0;
  for (char c : {'L', 'U', 'N', 'A', ' '}) assert(pet::typeName(name, c));
  assert(!pet::typeName(name, ' ') && pet::finishName(name) && !std::strcmp(name, "LUNA"));
  assert(!pet::validName(" luna") && !pet::validName("LUNA ") && !pet::validName("LUNA  SOL"));
  assert(!pet::validName("ABCDEFGHIJKLM") && !pet::validName("A1") && !pet::validName("A\nB"));
  assert(pet::validName("LUNA SOL", false));
  pet::Profile p;
  p.created = true;
  p.bornDay = 20000;
  p.baselineSeconds = 5000;
  assert(pet::addReading(p, 5240));
  assert(!pet::addReading(p, 5240));
  assert(pet::food(p) == 0);
  assert(pet::addReading(p, 5720));
  assert(pet::food(p) == 0);  // Growth alone never grants supplies.
  assert(pet::stage(p.readingSeconds) == pet::Stage::Egg);
  assert(!pet::feed(p));
  assert(pet::addReading(p, 6200));
  assert(pet::stage(p.readingSeconds) == pet::Stage::Newborn);
  p.earnedFood = 3;
  assert(pet::feed(p) && p.saciety == 95 && pet::food(p) == 2);
  assert(pet::feed(p) && p.saciety == 100 && pet::food(p) == 1);
  assert(!pet::feed(p));
  assert(!pet::addReading(p, 200));
  assert(pet::petOnce(p, 20000));
  assert(!pet::petOnce(p, 20000));
  assert(!pet::petOnce(p, 19999));
  p.careAt = 1700000000;
  p.energy = 20;
  p.resting = true;
  p.restAt = p.careAt;
  assert(pet::advanceCare(p, 1700007200));
  assert(p.energy == 60 && !p.resting);
  const auto unchanged = p.saciety;
  assert(!pet::advanceCare(p, 1700007200) && p.saciety == unchanged);
  assert(pet::advanceCare(p, 1699990000) && p.saciety == unchanged);
  const auto seconds = p.readingSeconds;
  assert(pet::chooseProject(p, 0));
  assert(p.readingSeconds == seconds);
  assert(pet::addReading(p, 6800));
  assert(p.owned & 1);
  assert(p.equipped[0] == 0);
  assert(pet::projectThreshold(0) == 600 && pet::projectThreshold(1) == 1500);
  assert(pet::chooseProject(p, 6));
  assert(pet::projectRemaining(p) == 900);
  assert(!pet::chooseProject(p, pet::OBJECT_COUNT));
  std::strcpy(p.name, "LUNA SOL");
  p.sequence = 49;
  p.mode = pet::Mode::Hard;
  p.goalIndex = 3;
  p.noticedSeconds = 1200;
  uint8_t bytes[pet::STATE_BYTES];
  pet::encode(p, bytes);
  pet::Profile decoded;
  assert(testDecode(bytes, decoded));
  {
    pet::Profile catalog = p, roundTrip;
    bool visited[pet::OBJECT_COUNT]{};
    for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
      for (unsigned n = 0; n < pet::OBJECTS_PER_SLOT; ++n) {
        const auto id = pet::objectAt(slot, n);
        assert(id < pet::OBJECT_COUNT && !visited[id] && pet::objectSlot(id) == slot);
        visited[id] = true;
        pet::own(catalog, id);
        assert(pet::owns(catalog, id));
        if (catalog.equipped[slot] != id) assert(pet::equip(catalog, id));
        assert(catalog.equipped[slot] == id);
      }
    }
    assert(pet::ownedCount(catalog) == 240 && !pet::owns(catalog, 240) && !pet::equip(catalog, 255));
    pet::encode(catalog, bytes);
    assert(!std::memcmp(bytes, "PET7", 4) && testDecode(bytes, roundTrip));
    assert(pet::ownedCount(roundTrip) == 240 && roundTrip.equipped[3] == 239);
    bytes[67] = 239;
    auto hash = pet::checksum(bytes, saveLength(bytes) - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(hash >> (8 * i));
    assert(!testDecode(bytes, roundTrip));
    // Older profiles have no extra ownership bits, even if reserved bytes were nonzero.
    pet::encode(p, bytes);
    bytes[3] = '3';
    std::memset(bytes + 176, 255, pet::EXTRA_OWNED_BYTES);
    hash = pet::checksum(bytes, saveLength(bytes) - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(hash >> (8 * i));
    assert(testDecode(bytes, roundTrip) && roundTrip.owned == p.owned && !pet::owns(roundTrip, 239));
    pet::encode(p, bytes);
  }
  assert(!std::strcmp(decoded.name, "LUNA SOL"));
  assert(decoded.sequence == 49 && decoded.readingSeconds == p.readingSeconds && decoded.mode == p.mode &&
         decoded.equipped[0] == 0 && decoded.spentFood == 2);
  for (unsigned i = 0; i < pet::STATE_BYTES; ++i) {
    bytes[i] ^= 1;
    assert(!testDecode(bytes, decoded));
    bytes[i] ^= 1;
  }
  bytes[3] = '2';
  auto crc = pet::checksum(bytes, saveLength(bytes) - 4);
  for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(crc >> (8 * i));
  assert(testDecode(bytes, decoded) && !decoded.name[0] && decoded.readingSeconds == p.readingSeconds);
  pet::GameState game;
  game.begin(pet::Game::Pairs, 123);
  for (unsigned symbol = 0; symbol < 3; ++symbol)
    for (unsigned i = 0; i < 6; ++i)
      if (game.cards[i] == symbol) game.select(i);
  assert(game.finished() && game.matched == 63);
  game.begin(pet::Game::Sequence, 123);
  for (unsigned round = 0; round < 3; ++round) {
    if (round) game.advance();
    game.advance();
    for (unsigned i = 0; i < game.sequenceLength(); ++i) game.select(game.sequence[i]);
  }
  assert(game.finished());
  game.begin(pet::Game::Sequence, 123);
  game.advance();
  game.select((game.sequence[0] + 1) % 3);
  assert(game.showing() && game.step == 0 && game.mistakes == 1);
  game.begin(pet::Game::Puzzle, 99);
  assert(!game.finished() && !game.puzzleSolved());
  pet::Profile health;
  health.created = true;
  health.readingSeconds = 1200;
  health.careAt = 1700000000;
  health.lastReadAt = 1700000000;
  health.mode = pet::Mode::Relaxed;
  assert(pet::advanceCare(health, 1700000000 + 80 * 3600));
  assert(health.hygiene == 20 && health.illness != pet::Illness::Healthy);
  assert(pet::clean(health) && health.hygiene == 100 && !pet::clean(health));
  assert(health.illness != pet::Illness::Healthy);
  auto remedies = pet::medicine(health);
  assert(pet::cure(health) && pet::medicine(health) == remedies - 1 && !pet::cure(health));
  health.stories = 1;
  health.bonusMedicine = 1;  // Previously earned credit remains valid.
  assert(!pet::absenceExpired(health, 1700000000 + 365 * 86400));
  for (unsigned mode = 1; mode < 4; ++mode) {
    health.mode = pet::Mode(mode);
    const int grace = mode == 1 ? 7 * 86400 : mode == 2 ? 3 * 86400 : 86400;
    assert(!pet::absenceExpired(health, health.lastReadAt + grace - 1));
    assert(pet::absenceExpired(health, health.lastReadAt + grace));
  }
  pet::encode(health, bytes);
  assert(testDecode(bytes, decoded) && decoded.stories == 1 && decoded.hygiene == 100);
  bytes[3] = '1';  // Legacy saves did not define the extension bytes.
  const auto legacyHash = pet::checksum(bytes, saveLength(bytes) - 4);
  for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(legacyHash >> (i * 8));
  assert(testDecode(bytes, decoded) && decoded.hygiene == 100 && decoded.lastReadAt == 0 && decoded.stories == 0);
  for (unsigned offset : {96u, 97u, 98u}) {
    pet::encode(health, bytes);
    bytes[offset] = 255;
    const auto hash = pet::checksum(bytes, saveLength(bytes) - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(hash >> (i * 8));
    assert(!testDecode(bytes, decoded));
  }
  assert(pet::endLife(health, pet::LifeStatus::Lost, 1700500000));
  assert(!pet::canCare(health) && !pet::addReading(health, 99999));
  assert(!pet::endLife(health, pet::LifeStatus::Retired, 1700600000));
  const auto inherited = pet::projectReading(health);
  pet::Profile next;
  next.created = true;
  next.heritageSeconds = inherited;
  next.readingSeconds = 1800;
  next.spentFood = health.spentFood;
  next.earnedFood = health.earnedFood;
  assert(pet::food(next) == pet::food(health));
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned stage = 1; stage < 5; ++stage) {
      next.species = pet::Species(species);
      next.readingSeconds = pet::stageStart(pet::Stage(stage));
      next.resting = false;
      next.illness = pet::Illness::Healthy;
      const auto awake = pet::homeScene(next);
      next.resting = true;
      assert(pet::homeScene(next) == awake + 1);
      next.illness = pet::Illness::Cold;
      assert(pet::homeScene(next) == awake + 2 && pet::homeScene(next) < 39);
    }
  // Actual entry reconciliation helper: stale journals cannot kill PET1 on migration.
  for (unsigned mode = 1; mode < 4; ++mode) {
    pet::Profile legacy;
    legacy.created = true;
    legacy.mode = pet::Mode(mode);
    legacy.readingSeconds = 1200;
    legacy.careAt = 1700000000;
    legacy.lastReadAt = 0;
    assert(pet::reconcileReading(legacy, 1200, 1700000000, 1705000000));
    assert(legacy.lastReadAt == 1705000000 && !pet::absenceExpired(legacy, 1705000000));
    assert(!pet::reconcileReading(legacy, 1200, 1700000000, 1705000000));
  }
  pet::Profile egg;
  egg.created = true;
  egg.baselineSeconds = 5000;
  egg.careAt = 1700000000;
  egg.lastReadAt = 1700000000;
  const auto eggSaciety = egg.saciety, eggJoy = egg.joy, eggEnergy = egg.energy;
  assert(pet::advanceCare(egg, 1700000000 + 3600 * 80));
  assert(egg.saciety == eggSaciety && egg.joy == eggJoy && egg.energy == eggEnergy && egg.hygiene == 100);
  assert(!pet::canCare(egg) && !pet::feed(egg) && !pet::petOnce(egg, 20001) && !pet::clean(egg) && !pet::cure(egg));
  assert(pet::reconcileReading(egg, 6199, 1700288100, 1700288100));
  assert(!pet::canCare(egg));
  assert(pet::reconcileReading(egg, 6200, 1700288200, 1700288200));
  assert(egg.careAt == 1700288200 && pet::canCare(egg));
  assert(egg.saciety == eggSaciety && egg.energy == eggEnergy);
  egg.earnedFood = 3;
  assert(pet::feed(egg));
  const auto newbornSaciety = egg.saciety;
  assert(!pet::advanceCare(egg, 1700288200 + 3599));
  assert(pet::advanceCare(egg, 1700288200 + 3600));
  assert(egg.saciety == newbornSaciety - 2);
  assert(pet::stage(egg.readingSeconds) == pet::Stage::Newborn && egg.hygiene == 99 &&
         egg.illness == pet::Illness::Healthy);
  // A failed profile commit leaves the source intact; replaying the same scan is deterministic.
  auto source = egg;
  source.lastReadAt = 1700000000;
  source.readingSeconds = 1200;
  auto failedDraft = source;
  assert(pet::reconcileReading(failedDraft, 6800, 1700400000, 1700400000));
  auto retryDraft = source;
  assert(pet::reconcileReading(retryDraft, 6800, 1700400000, 1700400000));
  assert(retryDraft.lastReadAt == failedDraft.lastReadAt && retryDraft.readingSeconds == failedDraft.readingSeconds);
  assert(!pet::absenceExpired(retryDraft, 1700400000));
  // Acknowledging a death survives re-entry and only applies to a lost companion.
  pet::Profile lost;
  assert(!pet::deathPending(lost) && !pet::acknowledgeDeath(lost));
  lost.created = true;
  assert(!pet::deathPending(lost) && !pet::acknowledgeDeath(lost));
  assert(pet::endLife(lost, pet::LifeStatus::Lost, 1700600000));
  assert(pet::deathPending(lost));
  pet::encode(lost, bytes);
  assert(testDecode(bytes, decoded) && pet::deathPending(decoded));
  assert(pet::acknowledgeDeath(lost) && !pet::deathPending(lost) && !pet::acknowledgeDeath(lost));
  for (uint32_t generation : {1u, 2u, 3u, UINT32_MAX}) {
    lost.generation = generation;
    pet::encode(lost, bytes);
    assert(testDecode(bytes, decoded) && !pet::deathPending(decoded) && decoded.deathAcknowledged &&
           decoded.generation == generation);
  }
  // Reserved bytes in PET1-3 must retain their legacy meaning (ignored).
  for (char version : {'1', '2', '3'}) {
    pet::encode(lost, bytes);
    bytes[3] = version;
    bytes[204] = 255;
    const auto hash = pet::checksum(bytes, saveLength(bytes) - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(hash >> (i * 8));
    assert(testDecode(bytes, decoded) && !decoded.deathAcknowledged);
    assert(pet::deathPending(decoded) == (version != '1'));
  }
  for (unsigned value : {1u, 2u, 255u}) {
    auto active = lost;
    active.lifeStatus = pet::LifeStatus::Active;
    active.endedAt = 0;
    pet::encode(active, bytes);
    bytes[204] = uint8_t(value);
    const auto hash = pet::checksum(bytes, saveLength(bytes) - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[saveLength(bytes) - 4 + i] = uint8_t(hash >> (i * 8));
    assert(!testDecode(bytes, decoded));
  }
  pet::Profile retired;
  retired.created = true;
  assert(pet::endLife(retired, pet::LifeStatus::Retired, 1700600000));
  assert(!pet::deathPending(retired) && !pet::acknowledgeDeath(retired));
  pet::Profile homeEgg;
  assert(!pet::homeUnlocked(homeEgg));
  homeEgg.created = true;
  assert(!pet::homeUnlocked(homeEgg));
  homeEgg.readingSeconds = 1199;
  assert(!pet::homeUnlocked(homeEgg));
  homeEgg.readingSeconds = 1200;
  assert(pet::homeUnlocked(homeEgg));
  homeEgg.readingSeconds = 0;
  homeEgg.lifeStatus = pet::LifeStatus::Lost;
  assert(pet::homeUnlocked(homeEgg));
  homeEgg.lifeStatus = pet::LifeStatus::Retired;
  assert(pet::homeUnlocked(homeEgg));
  pet::Profile complete;
  assert(!pet::collectionComplete(complete));
  for (unsigned i = 0; i < pet::OBJECT_COUNT; ++i) pet::own(complete, i);
  assert(!pet::collectionComplete(complete));
  complete.created = true;
  assert(pet::collectionComplete(complete));
  complete.ownedExtra[sizeof(complete.ownedExtra) - 1] &= 127;
  assert(pet::ownedCount(complete) == 239 && !pet::collectionComplete(complete));
  pet::own(complete, 239);
  assert(pet::collectionComplete(complete));
  pet::Profile affectionate;
  affectionate.created = true;
  affectionate.readingSeconds = 7200;
  affectionate.joy = 30;
  auto result = pet::affection(affectionate, 20001);
  assert(result.before == 30 && result.after == 45 && result.first && result.reaction == pet::Need::Company);
  result = pet::affection(affectionate, 20001);
  assert(result.before == 45 && result.after == 45 && !result.first);
  affectionate.joy = 95;
  result = pet::affection(affectionate, 20002);
  assert(result.before == 95 && result.after == 100 && result.first);
  result = pet::affection(affectionate, 20003);
  assert(result.before == 100 && result.after == 100 && result.first);
  for (auto block : {pet::PlayBlock::Sleeping, pet::PlayBlock::Sick, pet::PlayBlock::Hungry, pet::PlayBlock::Tired}) {
    auto waiting = affectionate;
    waiting.resting = block == pet::PlayBlock::Sleeping;
    waiting.illness = block == pet::PlayBlock::Sick ? pet::Illness::Cold : pet::Illness::Healthy;
    waiting.saciety = block == pet::PlayBlock::Hungry ? 29 : 70;
    waiting.energy = block == pet::PlayBlock::Tired ? 19 : 100;
    assert(pet::playBlock(waiting) == block && !pet::rewardGame(waiting, 20001));
    assert(waiting.gamesToday == 0 && waiting.joy == 100);
  }
  affectionate.joy = 30;
  assert(!pet::rewardGame(affectionate, 0));
  assert(pet::rewardGame(affectionate, 20001) && affectionate.joy == 45 && affectionate.energy == 80);
  assert(pet::rewardGame(affectionate, 20001) && affectionate.joy == 55 && affectionate.energy == 60);
  assert(pet::rewardGame(affectionate, 20001) && affectionate.gamesToday == 3 && affectionate.energy == 40);
  assert(!pet::rewardGame(affectionate, 20001) && affectionate.gamesToday == 3 && affectionate.energy == 40);
  assert(!pet::rewardGame(affectionate, 20000));
  assert(pet::rewardGame(affectionate, 20002) && affectionate.gamesToday == 1 && affectionate.energy == 20);
  assert(pet::rewardGame(affectionate, 20002) && affectionate.energy == 0);
  assert(!pet::rewardGame(affectionate, 20002));
  for (unsigned kind = 0; kind < 3; ++kind) {
    for (unsigned joy : {30u, 84u, 99u}) {
      auto rewarded = affectionate;
      rewarded.joy = uint8_t(joy);
      rewarded.energy = 100;
      rewarded.gamesToday = 0;
      assert(pet::rewardGame(rewarded, 20003, pet::Game(kind)));
      assert(rewarded.joy == std::min(100u, joy + 10 + kind * 5 + (joy < 40 ? 5 : 0)));
      assert(rewarded.energy == 80 && rewarded.gamesToday == 1);
    }
  }
  assert(pet::absenceGrace(pet::Mode::Relaxed) == 0);
  assert(pet::absenceGrace(pet::Mode::Normal) == 7 * 86400);
  assert(pet::absenceGrace(pet::Mode::Hard) == 3 * 86400);
  assert(pet::absenceGrace(pet::Mode::Extreme) == 86400);
  pet::Profile clocked;
  clocked.created = true;
  clocked.lastReadAt = 1800000000;
  for (auto mode : {pet::Mode::Normal, pet::Mode::Hard, pet::Mode::Extreme}) {
    clocked.mode = mode;
    const auto limit = pet::absenceGrace(mode);
    assert(!pet::absenceExpired(clocked, clocked.lastReadAt + limit - 1));
    assert(pet::absenceExpired(clocked, clocked.lastReadAt + limit));
  }
  std::puts("PET state: all checks passed");
}
