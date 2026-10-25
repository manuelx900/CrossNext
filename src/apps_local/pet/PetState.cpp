#include "PetState.h"

#include <algorithm>
#include <cstring>

#include "PetNeeds.h"

namespace pet {
namespace {
constexpr unsigned NAME_OFFSET = 144, OWNED_OFFSET = 176, DEATH_NOTICE_OFFSET = 204;
constexpr unsigned MOMENT_MASK_OFFSET = 214, MOMENT_RECORDS_OFFSET = 218, MOMENT_RECORD_BYTES = 16;
constexpr unsigned PROVISIONS_OFFSET = MOMENT_RECORDS_OFFSET + MOMENT_COUNT * MOMENT_RECORD_BYTES;
static_assert(PROVISIONS_OFFSET + 32 <= STATE_BYTES - 4);
static_assert(OWNED_OFFSET + EXTRA_OWNED_BYTES <= DEATH_NOTICE_OFFSET);
static_assert(DEATH_NOTICE_OFFSET < STATE_BYTES - 4);
static_assert(MOMENT_RECORDS_OFFSET + MOMENT_COUNT * MOMENT_RECORD_BYTES <= STATE_BYTES - 4);
static_assert(NAME_OFFSET + NAME_BYTES <= STATE_BYTES - 4);
constexpr uint64_t STAGES[] = {0, 1200, 7200, 28800, 72000};
void put(uint8_t* out, uint64_t value, unsigned n) {
  for (unsigned i = 0; i < n; ++i) out[i] = uint8_t(value >> (i * 8));
}
uint64_t get(const uint8_t* in, unsigned n) {
  uint64_t value = 0;
  for (unsigned i = 0; i < n; ++i) value |= uint64_t(in[i]) << (i * 8);
  return value;
}
uint32_t random(uint32_t& seed) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}
bool unlockProject(Profile& p) {
  if (!homeUnlocked(p) || p.project >= OBJECT_COUNT || owns(p, p.project) ||
      projectReading(p) < projectThreshold(ownedCount(p)))
    return false;
  own(p, p.project);
  p.equipped[objectSlot(p.project)] = p.project;
  p.memories |= 2;
  return true;
}
}  // namespace
Stage stage(uint64_t seconds) {
  for (unsigned i = 4; i > 0; --i)
    if (seconds >= STAGES[i]) return Stage(i);
  return Stage::Egg;
}
uint64_t stageStart(Stage value) { return STAGES[unsigned(value)]; }
uint64_t nextStageSeconds(Stage value) { return value == Stage::Adult ? STAGES[4] : STAGES[unsigned(value) + 1]; }
bool owns(const Profile& p, unsigned item) {
  if (item >= OBJECT_COUNT) return false;
  return item < 16 ? (p.owned & (1u << item)) != 0 : (p.ownedExtra[(item - 16) / 8] & (1u << ((item - 16) % 8))) != 0;
}
void own(Profile& p, unsigned item) {
  if (item >= OBJECT_COUNT) return;
  if (item < 16)
    p.owned |= uint16_t(1u << item);
  else
    p.ownedExtra[(item - 16) / 8] |= uint8_t(1u << ((item - 16) % 8));
}
unsigned ownedCount(const Profile& p) {
  unsigned count = 0;
  for (unsigned i = 0; i < OBJECT_COUNT; ++i) count += owns(p, i);
  return count;
}
uint64_t projectThreshold(unsigned owned) {
  if (owned >= OBJECT_COUNT) return UINT64_MAX;
  uint64_t minutes = 0;
  static constexpr uint8_t FIRST_MINUTES[] = {10, 15, 20, 30, 45};
  for (unsigned i = 0; i <= owned; ++i) minutes += i < sizeof(FIRST_MINUTES) ? FIRST_MINUTES[i] : 60;
  return minutes * 60;
}
uint64_t projectRemaining(const Profile& p) {
  if (owns(p, p.project)) return 0;
  const auto target = projectThreshold(ownedCount(p));
  return target > projectReading(p) ? target - projectReading(p) : 0;
}
uint64_t food(const Profile& p) { return p.earnedFood > p.spentFood ? p.earnedFood - p.spentFood : 0; }
unsigned dailyFood(uint32_t seconds) {
  static constexpr uint16_t MINUTES[] = {5, 10, 15, 30, 60, 120};
  unsigned count = 0;
  for (const auto minute : MINUTES) count += seconds / 60 >= minute;
  return count;
}
unsigned dailyMedicine(uint32_t seconds) { return unsigned(seconds >= 1800) + unsigned(seconds >= 7200); }
bool addReading(Profile& p, uint64_t total) {
  if (!p.created || p.lifeStatus != LifeStatus::Active || total < p.baselineSeconds ||
      total - p.baselineSeconds <= p.readingSeconds)
    return false;
  const auto before = stage(p.readingSeconds);
  const auto previous = p.readingSeconds;
  p.readingSeconds = total - p.baselineSeconds;
  const auto intervals = p.readingSeconds / 300 - previous / 300;
  p.joy = uint8_t(std::min(100u, unsigned(p.joy) + unsigned(std::min<uint64_t>(10, intervals)) * 10));
  if (stage(p.readingSeconds) != before) p.memories |= 1;
  unlockProject(p);
  return true;
}
bool reconcileReading(Profile& p, uint64_t totalSeconds, int64_t latestReadingAt, int64_t now) {
  if (!p.created || p.lifeStatus != LifeStatus::Active) return false;
  const bool egg = stage(p.readingSeconds) == Stage::Egg;
  bool changed = advanceCare(p, now);
  const bool read = addReading(p, totalSeconds);
  if (egg && stage(p.readingSeconds) != Stage::Egg && now >= 1577836800) p.careAt = now;
  if (!p.lastReadAt && now >= 1577836800) {
    // PET1 profiles begin their absence grace when first opened after migration.
    p.lastReadAt = now;
    changed = true;
  } else if (read && now >= 1577836800) {
    p.lastReadAt = std::max(p.lastReadAt, latestReadingAt > 0 ? std::min(now, latestReadingAt) : now);
  }
  return changed || read;
}
bool collectionComplete(const Profile& p) { return p.created && ownedCount(p) == OBJECT_COUNT; }
bool homeUnlocked(const Profile& p) {
  return p.created && (p.lifeStatus != LifeStatus::Active || stage(p.readingSeconds) != Stage::Egg);
}
bool chooseProject(Profile& p, unsigned item) {
  if (item >= OBJECT_COUNT) return false;
  if (owns(p, item)) return equip(p, item);
  const bool changed = p.project != item;
  p.project = uint8_t(item);
  return unlockProject(p) || changed;
}
bool equip(Profile& p, unsigned item) {
  if (item >= OBJECT_COUNT || !owns(p, item) || p.equipped[objectSlot(item)] == item) return false;
  p.equipped[objectSlot(item)] = uint8_t(item);
  return true;
}
bool unequip(Profile& p, unsigned item) {
  if (item >= OBJECT_COUNT || !owns(p, item) || p.equipped[objectSlot(item)] != item) return false;
  p.equipped[objectSlot(item)] = 255;
  return true;
}
bool feed(Profile& p) {
  if (!canCare(p) || !food(p) || p.saciety == 100 || p.spentFood == UINT32_MAX) return false;
  ++p.spentFood;
  p.saciety = uint8_t(std::min(100u, unsigned(p.saciety) + 25));
  p.memories |= 4;
  return true;
}
bool petOnce(Profile& p, int day) {
  if (!canCare(p) || day <= 0 || day <= p.lastPetDay) return false;
  p.lastPetDay = day;
  p.joy = uint8_t(std::min(100u, unsigned(p.joy) + moodGain(p)));
  p.memories |= 8;
  return true;
}
bool rewardGame(Profile& p, int day, Game game) {
  if (!gameRewardAvailable(p, day)) return false;
  if (day > p.gameDay) {
    p.gameDay = day;
    p.gamesToday = 0;
  }
  ++p.gamesToday;
  p.joy = uint8_t(std::min(100u, unsigned(p.joy) + gameMoodGain(p, game)));
  p.energy -= 20;
  return true;
}
bool advanceCare(Profile& p, int64_t now) {
  if (!p.created || p.lifeStatus != LifeStatus::Active || now < 1577836800) return false;
  if (!p.careAt || now < p.careAt) {
    p.careAt = now;
    if (p.resting) p.restAt = now;
    return true;
  }
  bool changed = false;
  const auto hours = uint64_t(now - p.careAt) / 3600;
  if (stage(p.readingSeconds) == Stage::Egg) {
    if (!hours) return false;
    p.careAt += int64_t(hours * 3600);
    return true;
  }
  if (hours) {
    const unsigned elapsed = unsigned(std::min<uint64_t>(hours, 100));
    const unsigned floor = p.mode == Mode::Relaxed ? 20 : 0;
    p.saciety = uint8_t(std::max(floor, unsigned(p.saciety) > elapsed * 2 ? unsigned(p.saciety) - elapsed * 2 : 0));
    p.joy = uint8_t(std::max(floor, unsigned(p.joy) > elapsed ? unsigned(p.joy) - elapsed : 0));
    p.energy = uint8_t(std::min(100u, unsigned(p.energy) + std::min(elapsed, 20u) * 5));
    if (stage(p.readingSeconds) != Stage::Egg) {
      const unsigned hygieneFloor = p.mode == Mode::Relaxed ? 20 : 0;
      const unsigned dirt = unsigned(std::min<uint64_t>(hours, 100));
      p.hygiene = uint8_t(std::max(hygieneFloor, unsigned(p.hygiene) > dirt ? unsigned(p.hygiene) - dirt : 0));
      if (p.hygiene <= 20 && p.illness == Illness::Healthy)
        p.illness = p.saciety < 25 ? Illness::Malaise : Illness::Cold;
    }
    p.careAt += int64_t(hours * 3600);
    changed = true;
  }
  if (p.resting && p.restAt > 0 && now >= p.restAt && now - p.restAt >= 7200) {
    p.resting = false;
    p.restAt = 0;
    p.energy = uint8_t(std::min(100u, unsigned(p.energy) + 30));
    changed = true;
  }
  return changed;
}
uint64_t projectReading(const Profile& p) {
  return p.heritageSeconds > UINT64_MAX - p.readingSeconds ? UINT64_MAX : p.heritageSeconds + p.readingSeconds;
}
uint64_t medicine(const Profile& p) {
  const auto earned = 1 + p.earnedMedicine + p.bonusMedicine;
  return earned > p.spentMedicine ? earned - p.spentMedicine : 0;
}
bool canCare(const Profile& p) {
  return p.created && p.lifeStatus == LifeStatus::Active && stage(p.readingSeconds) != Stage::Egg;
}
bool clean(Profile& p) {
  if (!canCare(p) || p.hygiene == 100) return false;
  p.hygiene = 100;
  p.joy = uint8_t(std::min(100u, unsigned(p.joy) + 5));
  p.memories |= 16;
  return true;
}
bool cure(Profile& p) {
  if (!canCare(p) || p.illness == Illness::Healthy || !medicine(p) || p.spentMedicine == UINT32_MAX) return false;
  ++p.spentMedicine;
  if (p.treatments < UINT32_MAX) ++p.treatments;
  p.illness = Illness::Healthy;
  p.hygiene = std::max<uint8_t>(60, p.hygiene);
  p.memories |= 16;
  return true;
}
unsigned momentObject(uint32_t snapshot, unsigned slot) {
  if (slot >= SLOT_COUNT) return 255;
  const unsigned index = (snapshot >> (slot * 6)) & 63;
  return index < OBJECTS_PER_SLOT ? objectAt(slot, index) : 255;
}
Stage momentStage(uint32_t snapshot) { return Stage((snapshot >> 24) & 7); }
MomentPose momentPose(uint32_t snapshot) { return MomentPose((snapshot >> 27) & 7); }
bool recordGameWin(Profile& p) {
  if (playBlock(p) != PlayBlock::None || p.gamesWon == UINT32_MAX) return false;
  ++p.gamesWon;
  return true;
}
void captureMoments(Profile& p, const Profile& previous, int day) {
  if (!p.created || p.lifeStatus != LifeStatus::Active) return;
  const auto record = [&](Moment moment, Stage age) {
    const unsigned id = unsigned(moment);
    if (p.momentMask & (1u << id)) return;
    MomentPose pose = MomentPose::Normal;
    switch (need(p)) {
      case Need::Sick:
        pose = MomentPose::Sick;
        break;
      case Need::Sleeping:
        pose = MomentPose::Sleeping;
        break;
      case Need::Hungry:
        pose = MomentPose::Hungry;
        break;
      case Need::Tired:
        pose = MomentPose::Tired;
        break;
      case Need::None:
        pose = p.joy >= 75 ? MomentPose::Happy : MomentPose::Normal;
        break;
      case Need::Dirty:
      case Need::Company:
        break;
    }
    uint32_t snapshot = (unsigned(age) << 24) | (unsigned(pose) << 27);
    for (unsigned slot = 0; slot < SLOT_COUNT; ++slot) {
      const unsigned item = p.equipped[slot];
      const unsigned index = item >= OBJECT_COUNT ? 63 : item < 12 ? item % 3 : 3 + (item - 12) % 57;
      snapshot |= index << (slot * 6);
    }
    p.moments[id] = snapshot;
    p.momentDays[id] = day > 0 ? day : 0;
    p.momentReading[id] = p.readingSeconds;
    p.momentMask |= 1u << id;
  };
  if (!previous.created || previous.generation != p.generation) record(Moment::Arrival, stage(p.readingSeconds));
  const auto age = stage(p.readingSeconds), before = stage(previous.readingSeconds);
  static constexpr Moment ids[] = {Moment::Arrival, Moment::Hatching, Moment::Baby, Moment::Juvenile, Moment::Adult};
  for (unsigned i = unsigned(before) + 1; i <= unsigned(age); ++i) record(ids[i], Stage(i));
  if (!(previous.memories & 2) && (p.memories & 2)) record(Moment::Home, age);
  if (!(previous.memories & 4) && (p.memories & 4)) record(Moment::Meal, age);
  if (!(previous.memories & 8) && (p.memories & 8)) record(Moment::Affection, age);
  if (!(previous.memories & 16) && (p.memories & 16)) record(Moment::Health, age);
  if (!previous.resting && p.resting) record(Moment::Rest, age);
  if (!previous.gameDay && p.gameDay) record(Moment::Game, age);
  static constexpr uint32_t readingHours[] = {1, 5, 10, 25, 50, 100};
  for (unsigned i = 0; i < 6; ++i) {
    const uint64_t threshold = uint64_t(readingHours[i]) * 3600;
    if (previous.readingSeconds < threshold && p.readingSeconds >= threshold)
      record(Moment(unsigned(Moment::Reading1h) + i), age);
  }
  static constexpr uint32_t wins[] = {5, 25, 100}, treatments[] = {1, 5, 20};
  for (unsigned i = 0; i < 3; ++i) {
    if (previous.gamesWon < wins[i] && p.gamesWon >= wins[i]) record(Moment(unsigned(Moment::Games5) + i), age);
    if (previous.treatments < treatments[i] && p.treatments >= treatments[i])
      record(Moment(unsigned(Moment::Treatment) + i), age);
  }
}
bool endLife(Profile& p, LifeStatus reason, int64_t now) {
  if (!p.created || p.lifeStatus != LifeStatus::Active || reason == LifeStatus::Active || now < 1577836800)
    return false;
  p.lifeStatus = reason;
  p.endedAt = now;
  p.resting = false;
  p.restAt = 0;
  p.deathAcknowledged = false;
  return true;
}
bool deathPending(const Profile& p) { return p.created && p.lifeStatus == LifeStatus::Lost && !p.deathAcknowledged; }
bool acknowledgeDeath(Profile& p) {
  if (!deathPending(p)) return false;
  p.deathAcknowledged = true;
  return true;
}
int64_t absenceGrace(Mode mode) {
  static constexpr int64_t grace[] = {0, 7 * 86400, 3 * 86400, 86400};
  return grace[unsigned(mode)];
}
bool absenceExpired(const Profile& p, int64_t now) {
  if (!p.created || p.lifeStatus != LifeStatus::Active || p.mode == Mode::Relaxed || p.lastReadAt < 1577836800 ||
      now < p.lastReadAt)
    return false;
  return now - p.lastReadAt >= absenceGrace(p.mode);
}
unsigned homeScene(const Profile& p) {
  const auto life = stage(p.readingSeconds);
  if (life == Stage::Egg) return unsigned(p.species);
  const unsigned pose = p.illness != Illness::Healthy ? 2 : p.resting || p.lifeStatus != LifeStatus::Active ? 1 : 0;
  return 3 + unsigned(p.species) * 12 + (unsigned(life) - 1) * 3 + pose;
}
unsigned nameLength(const char* text) {
  unsigned count = 0;
  for (unsigned i = 0; i < NAME_BYTES && text[i]; ++i)
    if ((uint8_t(text[i]) & 0xc0) != 0x80) ++count;
  return count;
}
bool validName(const char* text, bool emptyAllowed) {
  unsigned i = 0, count = 0;
  bool space = true;
  while (i < NAME_BYTES && text[i]) {
    const auto c = uint8_t(text[i++]);
    if (++count > NAME_LIMIT) return false;
    if (c == ' ') {
      if (space) return false;
      space = true;
    } else if (c >= 'A' && c <= 'Z') {
      space = false;
    } else if (c == 0xc3 && i < NAME_BYTES && uint8_t(text[i]) == 0x91) {
      ++i;
      space = false;
    } else
      return false;
  }
  return i < NAME_BYTES && (count ? !space : emptyAllowed);
}
bool typeName(char* text, char key) {
  if (nameLength(text) >= NAME_LIMIT) return false;
  const unsigned n = unsigned(std::strlen(text));
  if (key == ' ') {
    if (!n || text[n - 1] == ' ') return false;
    text[n] = ' ';
    text[n + 1] = 0;
  } else if (key == '[') {
    if (n + 2 >= NAME_BYTES) return false;
    text[n] = char(0xc3);
    text[n + 1] = char(0x91);
    text[n + 2] = 0;
  } else if (key >= 'A' && key <= 'Z') {
    if (n + 1 >= NAME_BYTES) return false;
    text[n] = key;
    text[n + 1] = 0;
  } else
    return false;
  return true;
}
bool eraseName(char* text) {
  unsigned n = unsigned(std::strlen(text));
  if (!n) return false;
  do {
    --n;
  } while (n && (uint8_t(text[n]) & 0xc0) == 0x80);
  text[n] = 0;
  return true;
}
bool finishName(char* text) {
  unsigned n = unsigned(std::strlen(text));
  while (n && text[n - 1] == ' ') text[--n] = 0;
  return validName(text, false);
}
uint32_t checksum(const uint8_t* data, size_t length) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; ++i) hash = (hash ^ data[i]) * 16777619u;
  return hash;
}
void encode(const Profile& p, uint8_t* out) {
  std::memset(out, 0, STATE_BYTES);
  std::memcpy(out, "PET7", 4);
  put(out + PROVISIONS_OFFSET, p.earnedFood, 8);
  put(out + PROVISIONS_OFFSET + 8, p.earnedMedicine, 8);
  put(out + PROVISIONS_OFFSET + 16, p.noticedFood, 8);
  put(out + PROVISIONS_OFFSET + 24, p.noticedMedicine, 8);
  std::memcpy(out + OWNED_OFFSET, p.ownedExtra, EXTRA_OWNED_BYTES);
  put(out + 4, p.sequence, 4);
  put(out + 8, p.baselineSeconds, 8);
  put(out + 16, p.readingSeconds, 8);
  put(out + 24, p.noticedSeconds, 8);
  put(out + 32, p.spentFood, 4);
  put(out + 36, p.birthDaySeconds, 4);
  put(out + 40, uint32_t(p.bornDay), 4);
  put(out + 44, uint32_t(p.lastPetDay), 4);
  put(out + 48, uint32_t(p.gameDay), 4);
  put(out + 52, p.owned, 2);
  put(out + 54, p.memories, 2);
  put(out + 56, p.sharedDays, 2);
  put(out + 58, p.streak, 2);
  out[60] = uint8_t(p.species);
  out[61] = uint8_t(p.mode);
  out[62] = p.goalIndex;
  out[63] = p.saciety;
  out[64] = p.joy;
  out[65] = p.energy;
  out[66] = p.project;
  std::memcpy(out + 67, p.equipped, SLOT_COUNT);
  out[71] = p.gamesToday;
  out[72] = p.foodChoice;
  out[73] = uint8_t(p.noticedStage);
  out[74] = p.created;
  out[75] = p.resting;
  out[DEATH_NOTICE_OFFSET] = p.deathAcknowledged;
  out[MOMENTS_VERSION_OFFSET] = 2;
  put(out + 206, p.gamesWon, 4);
  put(out + 210, p.treatments, 4);
  put(out + MOMENT_MASK_OFFSET, p.momentMask, 4);
  for (unsigned i = 0; i < MOMENT_COUNT; ++i) {
    auto* entry = out + MOMENT_RECORDS_OFFSET + i * MOMENT_RECORD_BYTES;
    put(entry, p.moments[i], 4);
    put(entry + 4, uint32_t(p.momentDays[i]), 4);
    put(entry + 8, p.momentReading[i], 8);
  }
  put(out + 80, uint64_t(p.careAt), 8);
  put(out + 88, uint64_t(p.restAt), 8);
  out[96] = p.hygiene;
  out[97] = uint8_t(p.illness);
  out[98] = uint8_t(p.lifeStatus);
  put(out + 100, p.spentMedicine, 4);
  put(out + 104, p.bonusMedicine, 4);
  put(out + 108, p.generation, 4);
  put(out + 112, p.stories, 2);
  put(out + 116, uint32_t(p.lastStoryDay), 4);
  put(out + 120, uint64_t(p.lastReadAt), 8);
  put(out + 128, uint64_t(p.endedAt), 8);
  put(out + 136, p.heritageSeconds, 8);
  std::memcpy(out + NAME_OFFSET, p.name, NAME_BYTES);
  put(out + STATE_BYTES - 4, checksum(out, STATE_BYTES - 4), 4);
}
bool decode(const uint8_t* in, Profile& p, size_t bytes) {
  const bool provisions = bytes == STATE_BYTES && !std::memcmp(in, "PET7", 4);
  const bool detailed = provisions || (bytes == STATE_BYTES && !std::memcmp(in, "PET6", 4));
  if (!detailed && bytes != LEGACY_STATE_BYTES) return false;
  const bool legacy = !std::memcmp(in, "PET1", 4);
  const bool current = detailed || !std::memcmp(in, "PET5", 4);
  const bool expanded = current || !std::memcmp(in, "PET4", 4);
  const bool named = expanded || !std::memcmp(in, "PET3", 4);
  if ((!legacy && !named && std::memcmp(in, "PET2", 4)) || get(in + bytes - 4, 4) != checksum(in, bytes - 4) ||
      in[60] > 2 || in[61] > 3 || in[62] > 3 || in[63] > 100 || in[64] > 100 || in[65] > 100 ||
      in[66] >= OBJECT_COUNT || in[71] > 3 || in[72] > 2 || in[73] > 4 || in[74] > 1 || in[75] > 1 ||
      (expanded && (in[DEATH_NOTICE_OFFSET] > 1 || in[MOMENTS_VERSION_OFFSET] > (detailed ? 2 : 1))) ||
      (!expanded && (get(in + 52, 2) > 4095 || in[66] >= 12)))
    return false;
  if (get(in + 8, 8) > UINT64_MAX - get(in + 16, 8) || get(in + 40, 4) > INT32_MAX || get(in + 44, 4) > INT32_MAX ||
      get(in + 48, 4) > INT32_MAX || get(in + 80, 8) > INT64_MAX || get(in + 88, 8) > INT64_MAX)
    return false;
  if (!legacy && (in[96] > 100 || in[97] > 2 || in[98] > 2 || !get(in + 108, 4) || get(in + 112, 2) > 255 ||
                  get(in + 116, 4) > INT32_MAX || get(in + 120, 8) > INT64_MAX || get(in + 128, 8) > INT64_MAX ||
                  get(in + 136, 8) > UINT64_MAX - get(in + 16, 8)))
    return false;
  if (named && !validName(reinterpret_cast<const char*>(in + NAME_OFFSET))) return false;
  p = {};
  if (named) std::memcpy(p.name, in + NAME_OFFSET, NAME_BYTES);
  p.sequence = uint32_t(get(in + 4, 4));
  p.baselineSeconds = get(in + 8, 8);
  p.readingSeconds = get(in + 16, 8);
  p.noticedSeconds = get(in + 24, 8);
  p.spentFood = uint32_t(get(in + 32, 4));
  p.birthDaySeconds = uint32_t(get(in + 36, 4));
  p.bornDay = int32_t(get(in + 40, 4));
  p.lastPetDay = int32_t(get(in + 44, 4));
  p.gameDay = int32_t(get(in + 48, 4));
  p.owned = uint16_t(get(in + 52, 2));
  if (expanded) std::memcpy(p.ownedExtra, in + OWNED_OFFSET, EXTRA_OWNED_BYTES);
  for (unsigned i = 0; i < SLOT_COUNT; ++i) {
    const unsigned item = in[67 + i];
    if (item != 255 && ((!expanded && item >= 12) || !owns(p, item) || objectSlot(item) != i)) return false;
  }
  p.memories = uint16_t(get(in + 54, 2));
  p.sharedDays = uint16_t(get(in + 56, 2));
  p.streak = uint16_t(get(in + 58, 2));
  p.species = Species(in[60]);
  p.mode = Mode(in[61]);
  p.goalIndex = in[62];
  p.saciety = in[63];
  p.joy = in[64];
  p.energy = in[65];
  p.project = in[66];
  std::memcpy(p.equipped, in + 67, SLOT_COUNT);
  p.gamesToday = in[71];
  p.foodChoice = in[72];
  p.noticedStage = Stage(in[73]);
  p.created = in[74];
  p.resting = in[75];
  p.deathAcknowledged = expanded && in[DEATH_NOTICE_OFFSET];
  if (detailed || (current && in[MOMENTS_VERSION_OFFSET] == 1)) {
    p.momentMask = uint32_t(get(in + (detailed ? MOMENT_MASK_OFFSET : MOMENTS_VERSION_OFFSET + 1), detailed ? 4 : 2));
    if (p.momentMask >= (1u << MOMENT_COUNT)) return false;
    if (!detailed && p.momentMask >= (1u << LEGACY_MOMENT_COUNT)) return false;
    for (unsigned i = 0; i < (detailed ? MOMENT_COUNT : LEGACY_MOMENT_COUNT); ++i) {
      const auto* entry =
          in + (detailed ? MOMENT_RECORDS_OFFSET + i * MOMENT_RECORD_BYTES : MOMENTS_VERSION_OFFSET + 3 + i * 4);
      p.moments[i] = uint32_t(get(entry, 4));
      if (detailed) {
        if (get(entry + 4, 4) > INT32_MAX || get(entry + 8, 8) > p.readingSeconds) return false;
        p.momentDays[i] = int32_t(get(entry + 4, 4));
        p.momentReading[i] = get(entry + 8, 8);
      }
      if (!(p.momentMask & (1u << i))) continue;
      if (unsigned(momentStage(p.moments[i])) > unsigned(stage(p.readingSeconds)) ||
          unsigned(momentPose(p.moments[i])) > unsigned(MomentPose::Sleeping) || (p.moments[i] >> 30))
        return false;
      for (unsigned slot = 0; slot < SLOT_COUNT; ++slot)
        if (((p.moments[i] >> (slot * 6)) & 63) >= 60 && ((p.moments[i] >> (slot * 6)) & 63) != 63) return false;
    }
  }
  if (detailed) {
    if (in[MOMENTS_VERSION_OFFSET] != 2) return false;
    p.gamesWon = uint32_t(get(in + 206, 4));
    p.treatments = uint32_t(get(in + 210, 4));
  }
  p.careAt = int64_t(get(in + 80, 8));
  p.restAt = int64_t(get(in + 88, 8));
  if (!legacy) {
    p.hygiene = in[96];
    p.illness = Illness(in[97]);
    p.lifeStatus = LifeStatus(in[98]);
    p.spentMedicine = uint32_t(get(in + 100, 4));
    p.bonusMedicine = uint32_t(get(in + 104, 4));
    p.generation = uint32_t(get(in + 108, 4));
    p.stories = uint16_t(get(in + 112, 2));
    p.lastStoryDay = int32_t(get(in + 116, 4));
    p.lastReadAt = int64_t(get(in + 120, 8));
    p.endedAt = int64_t(get(in + 128, 8));
    p.heritageSeconds = get(in + 136, 8);
    if ((p.lifeStatus == LifeStatus::Active && p.endedAt != 0) ||
        (p.lifeStatus != LifeStatus::Active && (p.endedAt < 1577836800 || p.resting)))
      return false;
  }
  if (provisions) {
    p.earnedFood = get(in + PROVISIONS_OFFSET, 8);
    p.earnedMedicine = get(in + PROVISIONS_OFFSET + 8, 8);
    p.noticedFood = get(in + PROVISIONS_OFFSET + 16, 8);
    p.noticedMedicine = get(in + PROVISIONS_OFFSET + 24, 8);
  } else {
    p.earnedFood = projectReading(p) / 300;
    p.earnedMedicine = projectReading(p) / 900;
    const auto previous = p.heritageSeconds + std::min(p.noticedSeconds, p.readingSeconds);
    p.noticedFood = previous / 300;
    p.noticedMedicine = previous / 900;
  }
  return (!p.deathAcknowledged || p.lifeStatus == LifeStatus::Lost) && p.noticedSeconds <= p.readingSeconds &&
         p.spentFood <= p.earnedFood && p.spentMedicine <= 1 + p.earnedMedicine + p.bonusMedicine &&
         p.earnedFood <= UINT64_MAX / 300 && p.earnedMedicine <= UINT64_MAX / 900 && p.noticedFood <= p.earnedFood &&
         p.noticedMedicine <= p.earnedMedicine;
}
void GameState::begin(Game game, uint32_t seed) {
  *this = {};
  kind = game;
  randomState = seed ? seed : 17;
  for (unsigned i = 0; i < 6; ++i) cards[i] = uint8_t(i / 2);
  for (unsigned i = 5; i > 0; --i) std::swap(cards[i], cards[random(randomState) % (i + 1)]);
  for (auto& symbol : sequence) symbol = uint8_t(random(randomState) % 3);
  switch (kind) {
    case Game::Pairs:
      break;
    case Game::Sequence:
      phase = GamePhase::Memorize;
      break;
    case Game::Puzzle:
      preparePuzzle();
      break;
  }
}
void GameState::select(unsigned value) {
  if (phase != GamePhase::Playing) return;
  switch (kind) {
    case Game::Pairs:
      if (value >= 6 || second != 255 || (matched & (1u << value)) || picked == value) return;
      if (picked == 255) {
        picked = uint8_t(value);
      } else {
        second = uint8_t(value);
        if (cards[picked] == cards[second]) {
          matched |= uint8_t((1u << picked) | (1u << second));
          picked = second = 255;
          if (matched == 63) phase = GamePhase::Finished;
        } else {
          if (mistakes < UINT16_MAX) ++mistakes;
        }
      }
      break;
    case Game::Sequence:
      if (value >= 3) return;
      if (value != sequence[step]) {
        if (mistakes < UINT16_MAX) ++mistakes;
        step = 0;
        phase = GamePhase::Memorize;
      } else if (++step == sequenceLength()) {
        ++rounds;
        phase = rounds == 3 ? GamePhase::Finished : GamePhase::RoundComplete;
      }
      break;
    case Game::Puzzle: {
      if (value >= 9 || !puzzle[value]) return;
      unsigned blank = 0;
      while (blank < 9 && puzzle[blank]) ++blank;
      if (blank == 9) return;
      const unsigned row = value / 3, col = value % 3, br = blank / 3, bc = blank % 3;
      if ((row > br ? row - br : br - row) + (col > bc ? col - bc : bc - col) != 1) return;
      std::swap(puzzle[value], puzzle[blank]);
      if (moves < UINT16_MAX) ++moves;
      if (puzzleSolved()) phase = GamePhase::Finished;
      break;
    }
  }
}
void GameState::advance() {
  if (finished()) return;
  switch (kind) {
    case Game::Pairs:
      if (second != 255) picked = second = 255;
      break;
    case Game::Sequence:
      if (phase == GamePhase::Memorize)
        phase = GamePhase::Playing;
      else if (phase == GamePhase::RoundComplete) {
        step = 0;
        phase = GamePhase::Memorize;
      }
      break;
    case Game::Puzzle:
      break;
  }
}
bool GameState::puzzleSolved() const {
  for (unsigned i = 0; i < 9; ++i)
    if (puzzle[i] != (i + 1) % 9) return false;
  return true;
}
void GameState::preparePuzzle() {
  bool ready = false;
  for (unsigned attempt = 0; attempt < 16 && !ready; ++attempt) {
    for (unsigned i = 0; i < 9; ++i) puzzle[i] = uint8_t(i);
    for (unsigned i = 8; i > 0; --i) std::swap(puzzle[i], puzzle[random(randomState) % (i + 1)]);
    unsigned inversions = 0, distance = 0;
    for (unsigned i = 0; i < 9; ++i) {
      if (!puzzle[i]) continue;
      for (unsigned j = i + 1; j < 9; ++j) inversions += puzzle[j] && puzzle[i] > puzzle[j];
    }
    if (inversions % 2) {
      unsigned a = puzzle[0] ? 0 : 1, b = a + 1;
      if (!puzzle[b]) ++b;
      std::swap(puzzle[a], puzzle[b]);
    }
    for (unsigned i = 0; i < 9; ++i) {
      if (!puzzle[i]) continue;
      const unsigned goal = puzzle[i] - 1, row = i / 3, col = i % 3;
      distance +=
          (row > goal / 3 ? row - goal / 3 : goal / 3 - row) + (col > goal % 3 ? col - goal % 3 : goal % 3 - col);
    }
    ready = distance >= 12;
  }
  if (!ready) {
    static constexpr uint8_t fallback[] = {8, 6, 7, 2, 5, 4, 3, 0, 1};
    std::memcpy(puzzle, fallback, sizeof(puzzle));
  }
  std::memcpy(initialPuzzle, puzzle, sizeof(puzzle));
  moves = 0;
}
void GameState::resetPuzzle() {
  if (kind != Game::Puzzle || phase != GamePhase::Playing) return;
  std::memcpy(puzzle, initialPuzzle, sizeof(puzzle));
  moves = 0;
}
}  // namespace pet
