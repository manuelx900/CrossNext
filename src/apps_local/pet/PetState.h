#pragma once

#include <cstddef>
#include <cstdint>

#include "PetCatalog.h"

namespace pet {
enum class Species : uint8_t { Silva, Nox, Ember };
enum class Stage : uint8_t { Egg, Newborn, Baby, Juvenile, Adult };
enum class Mode : uint8_t { Relaxed, Normal, Hard, Extreme };
enum class Illness : uint8_t { Healthy, Cold, Malaise };
enum class LifeStatus : uint8_t { Active, Lost, Retired };
enum class Tab : uint8_t { Pet, Care, Home, Album };
enum class View : uint8_t {
  New,
  Info,
  Rules,
  Dashboard,
  Care,
  Home,
  Album,
  Settings,
  Projects,
  Games,
  Play,
  Reward,
  Evolution,
  Memory,
  Lives,
  Notice,
  Health,
  LifeDetail,
  Retire,
  Name,
  Death,
  Collection,
  Affection,
  Memories,
  ReadingInfo,
  ProjectPreview,
  HealthHelp,
  SettingsConfirm
};
enum class Game : uint8_t { Pairs, Sequence, Puzzle };
enum class GamePhase : uint8_t { Playing, Memorize, RoundComplete, Finished };
inline constexpr unsigned STATE_BYTES = 640, LEGACY_STATE_BYTES = 256, EXTRA_OWNED_BYTES = (OBJECT_COUNT - 16) / 8;
enum class Moment : uint8_t {
  Hatching,
  Home,
  Arrival,
  Meal,
  Affection,
  Health,
  Baby,
  Juvenile,
  Adult,
  Rest,
  Game,
  Reading1h,
  Reading5h,
  Reading10h,
  Reading25h,
  Reading50h,
  Reading100h,
  Games5,
  Games25,
  Games100,
  Treatment,
  Treatments5,
  Treatments20
};
enum class MomentPose : uint8_t { Normal, Happy, Hungry, Tired, Sick, Sleeping };
inline constexpr unsigned MOMENT_COUNT = unsigned(Moment::Treatments20) + 1, LEGACY_MOMENT_COUNT = 11,
                          MOMENTS_VERSION_OFFSET = 205;
inline constexpr uint8_t GOALS[] = {5, 10, 20, 30};
inline constexpr unsigned NAME_LIMIT = 12, NAME_BYTES = NAME_LIMIT * 2 + 1;
unsigned nameLength(const char*);
bool validName(const char*, bool emptyAllowed = true);
bool typeName(char*, char key);
bool eraseName(char*);
bool finishName(char*);
struct Profile {
  char name[NAME_BYTES]{};
  uint32_t sequence = 0, spentMedicine = 0, bonusMedicine = 0, generation = 1;
  uint64_t heritageSeconds = 0;
  // Total reading credit at selection, including inherited seconds.
  uint64_t projectStartedSeconds = 0;
  uint64_t earnedFood = 0, earnedMedicine = 0, noticedFood = 0, noticedMedicine = 0;
  int64_t lastReadAt = 0, endedAt = 0;
  // Legacy encounter fields remain readable for save compatibility.
  int32_t lastStoryDay = 0;
  uint16_t stories = 0;
  uint8_t hygiene = 100;
  Illness illness = Illness::Healthy;
  LifeStatus lifeStatus = LifeStatus::Active;
  uint64_t baselineSeconds = 0, readingSeconds = 0, noticedSeconds = 0;
  int64_t careAt = 0, restAt = 0;
  uint32_t spentFood = 0, birthDaySeconds = 0;
  int32_t bornDay = 0, lastPetDay = 0, gameDay = 0;
  uint16_t owned = 0, memories = 0;
  uint32_t momentMask = 0, gamesWon = 0, treatments = 0;
  uint32_t moments[MOMENT_COUNT]{};
  int32_t momentDays[MOMENT_COUNT]{};
  uint64_t momentReading[MOMENT_COUNT]{};
  uint8_t ownedExtra[EXTRA_OWNED_BYTES]{};
  uint16_t sharedDays = 0, streak = 0;
  Species species = Species::Silva;
  Mode mode = Mode::Relaxed;
  uint8_t goalIndex = 1, saciety = 70, joy = 70, energy = 100;
  uint8_t project = 5, equipped[SLOT_COUNT] = {255, 255, 255, 255};
  uint8_t gamesToday = 0, foodChoice = 0;
  Stage noticedStage = Stage::Egg;
  bool created = false, resting = false, deathAcknowledged = false;
};
uint64_t projectReading(const Profile&);
uint64_t medicine(const Profile&);
bool canCare(const Profile&);
bool clean(Profile&);
bool cure(Profile&);
void captureMoments(Profile&, const Profile& previous, int day = 0);
bool recordGameWin(Profile&);
unsigned momentObject(uint32_t snapshot, unsigned slot);
Stage momentStage(uint32_t snapshot);
MomentPose momentPose(uint32_t snapshot);
bool endLife(Profile&, LifeStatus, int64_t now);
bool deathPending(const Profile&);
bool acknowledgeDeath(Profile&);
int64_t absenceGrace(Mode);
bool absenceExpired(const Profile&, int64_t now);
unsigned homeScene(const Profile&);
bool homeUnlocked(const Profile&);
bool collectionComplete(const Profile&);
Stage stage(uint64_t seconds);
uint64_t stageStart(Stage stage);
uint64_t nextStageSeconds(Stage stage);
bool owns(const Profile&, unsigned item);
void own(Profile&, unsigned item);
unsigned ownedCount(const Profile&);
uint64_t projectThreshold(unsigned owned);
uint64_t projectRequiredSeconds(unsigned owned);
uint64_t projectProgressSeconds(const Profile&);
uint64_t projectRemaining(const Profile&);
uint64_t food(const Profile&);
unsigned dailyFood(uint32_t seconds);
unsigned dailyMedicine(uint32_t seconds);
bool addReading(Profile&, uint64_t totalSeconds);
// Apply a journal checkpoint only after ReadingTimeline has validated its interval.
bool reconcileReading(Profile&, uint64_t totalSeconds, int64_t latestReadingAt, int64_t now);
bool chooseProject(Profile&, unsigned item);
bool equip(Profile&, unsigned item);
bool unequip(Profile&, unsigned item);
bool feed(Profile&);
bool petOnce(Profile&, int day);
bool advanceCare(Profile&, int64_t now);
void encode(const Profile&, uint8_t out[STATE_BYTES]);
bool decode(const uint8_t*, Profile&, size_t bytes = STATE_BYTES);
uint32_t checksum(const uint8_t*, size_t);

struct GameState {
  Game kind = Game::Pairs;
  GamePhase phase = GamePhase::Playing;
  uint8_t cards[6]{}, sequence[7]{}, picked = 255, second = 255;
  uint8_t matched = 0, step = 0, rounds = 0;
  uint8_t puzzle[9]{}, initialPuzzle[9]{};
  uint16_t mistakes = 0, moves = 0;
  uint32_t randomState = 17;
  bool finished() const { return phase == GamePhase::Finished; }
  bool showing() const { return phase == GamePhase::Memorize; }
  unsigned sequenceLength() const { return 5 + (rounds < 3 ? rounds : 2); }
  void begin(Game game, uint32_t seed);
  void select(unsigned value);
  void advance();
  void resetPuzzle();
  void preparePuzzle();
  bool puzzleSolved() const;
};
}  // namespace pet
