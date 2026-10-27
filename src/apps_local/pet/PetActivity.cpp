#include "PetActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RecentBooksStore.h>

#include <algorithm>
#include <cstring>
#include <ctime>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "PetAlbum.h"
#include "PetCatalogView.h"

namespace fui = freeink::ui;
std::unique_ptr<Activity> PetActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  // The fixed profile, history window and UI descriptors outlive individual stack frames.
  auto app = makeUniqueNoThrow<PetActivity>(renderer, input);
  if (!app) LOG_ERR("PET", "OOM: PetActivity (%u bytes)", unsigned(sizeof(PetActivity)));
  return app;
}
void PetActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  contentsAvailable = openResources();
  if (!contentsAvailable) {
    loading = false;
    requestUpdate();
    return;
  }
  loaded = store.load(profile, writable);
  historyStore.load(history, historyWritable);
  chosenSpecies = profile.species;
  selectedMode = profile.mode;
  selectedGoal = profile.goalIndex;
  today = readingstats::today();
  summary.anchorDay = today;
  scanNow = std::time(nullptr);
  draft = profile;
  readingTimeline.begin(profile, scanNow);
  syncFarewellArt();
  view = profile.created ? pet::View::Dashboard : pet::View::New;
  loading = readings.begin(false, readAhead, sizeof(readAhead));
  if (!loading) finishScan(true);
  requestUpdate();
}
bool PetActivity::openResources() {
  if (Storage.exists(petui::ART_PATH)) {
    pettingHandAvailable = art.open(petui::ART_PATH, petui::ART_COUNT, ART_PIXEL_CACHE_BYTES);
    if (!pettingHandAvailable) art.open(petui::ART_PATH, petui::LEGACY_ART_COUNT, ART_PIXEL_CACHE_BYTES);
  }
  portraitBackgroundsAvailable = false;
  if (Storage.exists(petui::HOME_ART_PATH)) {
    portraitBackgroundsAvailable =
        homeArt.open(petui::HOME_ART_PATH, petui::HOME_BACKGROUNDS_COUNT, ART_PIXEL_CACHE_BYTES);
    if (!portraitBackgroundsAvailable) homeArt.open(petui::HOME_ART_PATH, petui::HOME_ART_COUNT, ART_PIXEL_CACHE_BYTES);
  }
  if (Storage.exists(petui::HABITAT_ART_PATH))
    habitatArt.open(petui::HABITAT_ART_PATH, petui::HABITAT_ART_COUNT, ART_PIXEL_CACHE_BYTES);
  if (Storage.exists(petsleep::ART_PATH)) sleepArt.open(petsleep::ART_PATH, petsleep::ART_COUNT, ART_PIXEL_CACHE_BYTES);
  const bool farewellReady = farewellArt.open(petui::FAREWELL_ART_PATH, petui::FAREWELL_ART_COUNT);
  farewellArt.close();
  const bool collectionReady = collectionArt.open(petui::COLLECTION_ART_PATH, petui::COLLECTION_ART_COUNT);
  collectionArt.close();
  return art.ready() && homeArt.ready() && habitatArt.ready() && sleepArt.ready() && farewellReady && collectionReady;
}
bool PetActivity::resourcesReady() const {
  return contentsAvailable && art.ready() && homeArt.ready() && habitatArt.ready() && sleepArt.ready() &&
         (!farewellArtTried || farewellArt.ready()) && (!collectionArtTried || collectionArt.ready());
}
void PetActivity::onExit() {
  readings.close();
  collectionArt.close();
  collectionArtTried = false;
  farewellArt.close();
  farewellArtTried = false;
  sleepArt.close();
  habitatArt.close();
  homeArt.close();
  portraitBackgroundsAvailable = false;
  art.close();
  Activity::onExit();
}
void PetActivity::show(pet::View next) {
  if (next != pet::View::Notice) feedResult = {};
  if (next != pet::View::Notice) {
    careResult = {};
    gameResult = {};
  }
  ++viewRevision;
  pendingTap.clear();
  pendingSlide.clear();
  view = next;
  sleepClock.restart(millis());
  animationRebasePending = true;
  syncCollectionArt();
  ready = false;
  interactions.setFocusedIndex(-1);
  requestUpdate();
}
void PetActivity::notice(const char* text, pet::View next, petui::NoticeKind kind, petui::CareResult result) {
  message = text;
  noticeKind = kind;
  careResult = result;
  if (next != pet::View::Notice) noticeReturn = next;
  show(pet::View::Notice);
}
bool PetActivity::commit() {
  pet::captureMoments(draft, profile, today);
  if (!store.save(draft, writable)) {
    notice(tr(STR_PET_SAVE_FAILED), view);
    return false;
  }
  profile = draft;
  syncFarewellArt();
  return true;
}
void PetActivity::syncCollectionArt() {
  const bool visible =
      view == pet::View::Collection || ((view == pet::View::Home || view == pet::View::Dashboard) &&
                                        pet::collectionComplete(profile) && pet::homeUnlocked(profile));
  if (!visible) {
    return;
  }
  if (collectionArtTried) return;
  collectionArtTried = true;
  if (Storage.exists(petui::COLLECTION_ART_PATH))
    collectionArt.open(petui::COLLECTION_ART_PATH, petui::COLLECTION_ART_COUNT);
}
void PetActivity::syncFarewellArt() {
  if (!profile.created || profile.lifeStatus != pet::LifeStatus::Lost) {
    if (farewellArtTried) farewellArt.close();
    farewellArtTried = false;
    return;
  }
  if (farewellArtTried) return;
  farewellArtTried = true;
  if (Storage.exists(petui::FAREWELL_ART_PATH)) farewellArt.open(petui::FAREWELL_ART_PATH, petui::FAREWELL_ART_COUNT);
}
void PetActivity::acknowledgeDeath(bool openHistory) {
  if (view != pet::View::Death || !pet::deathPending(profile)) return;
  draft = profile;
  if (!pet::acknowledgeDeath(draft) || !commit()) return;
  if (openHistory) {
    tab = pet::Tab::Album;
    page = 0;
    show(pet::View::Lives);
  } else {
    tab = pet::Tab::Pet;
    show(pet::View::Dashboard);
  }
}
void PetActivity::scan() {
  for (unsigned i = 0; i < 32; ++i) {
    const auto result = readings.next(record);
    switch (result) {
      case readingstats::ScanResult::Record: {
        if (record.kind == readingstats::RecordKind::Completion) break;
        const bool alreadyLost = readingTimeline.endedAt() != 0;
        readingTimeline.add(record.epoch, record.seconds, record.localDay, record.secondOfDay);
        if (!alreadyLost && readingTimeline.endedAt()) scanChanged = updateHabits() || scanChanged;
        summary.add(record);
        if (record.localDay <= today) lastReadingAt = std::max(lastReadingAt, record.book.lastEpoch);
        if (!readingTimeline.migration() && readingTimeline.valid() && !readingTimeline.endedAt()) {
          const auto eventAt = std::max(draft.careAt, std::min(scanNow, record.epoch + record.seconds));
          if (pet::reconcileReading(draft, summary.seconds, record.epoch + record.seconds, eventAt)) {
            pet::captureMoments(draft, profile, record.localDay);
            scanChanged = true;
          }
        }
      } break;
      case readingstats::ScanResult::More:
        break;
      case readingstats::ScanResult::Finished:
        finishScan(readings.hadDamage());
        return;
      case readingstats::ScanResult::Error:
        finishScan(true);
        return;
    }
  }
}
void PetActivity::finishScan(bool failed) {
  loading = false;
  readings.close();
  historyChanged = failed || !readingTimeline.valid() ||
                   (profile.created && summary.seconds < profile.baselineSeconds + profile.readingSeconds);
  if (!loaded) {
    notice(tr(STR_PET_LOAD_FAILED), pet::View::New);
    return;
  }
  if (historyChanged) {
    notice(tr(STR_PET_HISTORY_FAILED), view);
    return;
  }
  if (!today) {
    notice(tr(STR_PET_CLOCK_REQUIRED), view);
    return;
  }
  todaySeconds = int(std::min<uint32_t>(summary.onDay(today), INT32_MAX));
  if (!profile.created) {
    reconciled = true;
    show(pet::View::New);
    return;
  }
  const int64_t now = std::time(nullptr);
  bool changed = scanChanged;
  readingTimeline.applyProvisions(draft, profile);
  changed = changed || draft.earnedFood != profile.earnedFood || draft.earnedMedicine != profile.earnedMedicine;
  if (readingTimeline.migration())
    changed = pet::reconcileReading(draft, summary.seconds, lastReadingAt, now) || changed;
  const bool gapDuringScan = readingTimeline.endedAt() != 0;
  readingTimeline.finish(now);
  if (draft.lifeStatus == pet::LifeStatus::Active && readingTimeline.endedAt()) {
    if (!gapDuringScan) changed = updateHabits() || changed;
    changed = pet::advanceCare(draft, readingTimeline.endedAt()) || changed;
    changed = pet::endLife(draft, pet::LifeStatus::Lost, readingTimeline.endedAt()) || changed;
  } else {
    changed = pet::advanceCare(draft, now) || changed;
  }
  scanChanged = changed;
  if (draft.lifeStatus != pet::LifeStatus::Active && !archiveDraft()) return;
  changed = updateHabits() || changed;
  scanChanged = changed;
  if (changed && !commit()) return;
  reconciled = true;
  if (profile.lifeStatus != pet::LifeStatus::Active) {
    show(pet::deathPending(profile) ? pet::View::Death : pet::View::Dashboard);
    return;
  }
  if (pet::stage(profile.readingSeconds) > profile.noticedStage)
    show(pet::View::Evolution);
  else if (profile.readingSeconds / 60 > profile.noticedSeconds / 60)
    show(pet::View::Reward);
  else
    show(pet::View::Dashboard);
}
bool PetActivity::updateHabits() {
  todaySeconds = int(std::min<uint32_t>(summary.onDay(today), INT32_MAX));
  if (today == profile.bornDay) todaySeconds = std::max(0, todaySeconds - int(profile.birthDaySeconds));
  weekDays = 0;
  const int first = readingstats::weekStart(today, SETTINGS.weekStartsSunday);
  for (int day = first; day <= today; ++day) {
    uint32_t seconds = summary.onDay(day);
    if (day < profile.bornDay) seconds = 0;
    if (day == profile.bornDay) seconds = seconds > profile.birthDaySeconds ? seconds - profile.birthDaySeconds : 0;
    if (seconds >= 300) ++weekDays;
  }
  if (draft.lifeStatus != pet::LifeStatus::Active) return false;
  unsigned days = 0, consecutive = 0, best = profile.streak;
  const int earliest = std::max(int(profile.bornDay), today - readingstats::kHistoryDays + 1);
  for (int day = earliest; day <= today; ++day) {
    uint32_t seconds = summary.onDay(day);
    if (day == profile.bornDay) seconds = seconds > profile.birthDaySeconds ? seconds - profile.birthDaySeconds : 0;
    if (seconds >= 300) {
      ++days;
      best = std::max(best, ++consecutive);
    } else
      consecutive = 0;
  }
  const auto shared = uint16_t(std::min(65535u, std::max(unsigned(profile.sharedDays), days)));
  if (draft.sharedDays != shared || draft.streak != best) {
    draft.sharedDays = shared;
    draft.streak = uint16_t(std::min(65535u, best));
    return true;
  }
  return false;
}
bool PetActivity::archiveDraft() {
  historyDraft = history;
  if (pet::addLife(historyDraft, draft) && !historyStore.save(historyDraft, historyWritable)) {
    notice(tr(STR_PET_ARCHIVE_FAILED), view);
    return false;
  }
  if (!historyWritable) {
    notice(tr(STR_PET_ARCHIVE_FAILED), view);
    return false;
  }
  history = historyDraft;
  return true;
}
bool PetActivity::updateCare() {
  if (loading) return true;
  if (!loaded || historyChanged || !today) return true;
  if (!reconciled) {
    // Replay the completed scan after a failed write before evaluating absence.
    finishScan(false);
    return false;
  }
  if (!profile.created || profile.lifeStatus != pet::LifeStatus::Active || !writable) return true;
  draft = profile;
  const int64_t now = std::time(nullptr);
  const bool expired = pet::absenceExpired(draft, now);
  const auto careTime = expired ? draft.lastReadAt + pet::absenceGrace(draft.mode) : now;
  bool changed = pet::advanceCare(draft, careTime);
  if (expired) {
    pet::endLife(draft, pet::LifeStatus::Lost, careTime);
    if (!archiveDraft()) return false;
    changed = true;
  }
  if (changed && !commit()) return false;
  if (pet::deathPending(profile)) {
    show(pet::View::Death);
    return false;
  }
  if (changed) {
    ready = false;
    requestUpdate();
  }
  return true;
}
bool PetActivity::refreshDay() {
  if (!loaded || historyChanged) return true;
  if (!reconciled) return updateCare();
  const int current = readingstats::today();
  if (current && current != today) {
    const int previous = today;
    today = current;
    draft = profile;
    if (updateHabits() && !commit()) {
      today = previous;
      return false;
    }
  }
  return updateCare();
}
void PetActivity::changeTab(int value) {
  if (!profile.created || value < 0 || value >= 4) return;
  tab = pet::Tab(value);
  static constexpr pet::View pages[] = {pet::View::Dashboard, pet::View::Care, pet::View::Home, pet::View::Album};
  if (view == pages[value]) return;
  show(pages[value]);
}
void PetActivity::acknowledge() {
  if (view == pet::View::Evolution) {
    show(pet::View::Reward);
    return;
  }
  draft = profile;
  draft.noticedSeconds = profile.readingSeconds;
  draft.noticedFood = profile.earnedFood;
  draft.noticedMedicine = profile.earnedMedicine;
  draft.noticedStage = pet::stage(profile.readingSeconds);
  if (commit()) {
    changeTab(0);
  }
}
void PetActivity::back() {
  if (!resourcesReady()) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  switch (view) {
    case pet::View::ReadingInfo:
    case pet::View::Collection:
      show(returnView);
      return;
    case pet::View::Death:
      shelf::leave(renderer, mappedInput);
      return;
    case pet::View::Name:
      show(pet::View::New);
      return;
    case pet::View::New:
      if (profile.created) {
        changeTab(0);
        return;
      }
      shelf::leave(renderer, mappedInput);
      return;
    case pet::View::Dashboard:
    case pet::View::Care:
    case pet::View::Home:
    case pet::View::Album:
      shelf::leave(renderer, mappedInput);
      return;
    case pet::View::Info:
    case pet::View::Rules:
      show(pet::View::New);
      return;
    case pet::View::Settings:
      selectedMode = profile.mode;
      selectedGoal = profile.goalIndex;
      show(returnView);
      return;
    case pet::View::SettingsConfirm:
      show(pet::View::Settings);
      return;
    case pet::View::Projects:
      changeTab(2);
      return;
    case pet::View::ProjectPreview:
      show(pet::View::Projects);
      return;
    case pet::View::Health:
    case pet::View::Games:
      changeTab(1);
      return;
    case pet::View::HealthHelp:
      show(pet::View::Health);
      return;
    case pet::View::Play:
      show(pet::View::Games);
      return;
    case pet::View::Reward:
    case pet::View::Evolution:
      acknowledge();
      return;
    case pet::View::LifeDetail:
      show(lifeReturn);
      return;
    case pet::View::Retire:
      show(pet::View::Lives);
      return;
    case pet::View::Memory:
      show(memoryReturn);
      return;
    case pet::View::Memories:
    case pet::View::Lives:
      changeTab(3);
      return;
    case pet::View::Affection:
      show(affectionReturn);
      return;
    case pet::View::Notice:
      show(noticeReturn);
      return;
  }
}
void PetActivity::saveSettings(bool acceptExpiry) {
  if (!writable) return;
  draft = profile;
  draft.mode = selectedMode;
  draft.goalIndex = selectedGoal;
  const auto now = std::time(nullptr);
  if (pet::absenceExpired(draft, now)) {
    if (!acceptExpiry) {
      show(pet::View::SettingsConfirm);
      return;
    }
    if (!pet::endLife(draft, pet::LifeStatus::Lost, now) || !archiveDraft() || !commit()) return;
    show(pet::View::Death);
    return;
  }
  if (commit()) show(returnView);
}
void PetActivity::createCompanion() {
  if ((profile.created && profile.lifeStatus == pet::LifeStatus::Active) || !writable || !today || historyChanged)
    return;
  if (profile.created && (!archiveDraft() || profile.generation == UINT32_MAX)) return;
  draft = {};
  draft.sequence = profile.sequence;
  draft.species = chosenSpecies;
  draft.generation = profile.created ? profile.generation + 1 : 1;
  draft.heritageSeconds = pet::projectReading(profile);
  draft.project = profile.project;
  draft.projectStartedSeconds = profile.projectStartedSeconds;
  draft.earnedFood = draft.noticedFood = profile.earnedFood;
  draft.earnedMedicine = draft.noticedMedicine = profile.earnedMedicine;
  draft.spentFood = profile.spentFood;
  draft.spentMedicine = profile.spentMedicine;
  draft.bonusMedicine = profile.bonusMedicine;
  draft.stories = profile.stories;
  draft.lastStoryDay = profile.lastStoryDay;
  draft.owned = profile.owned;
  std::memcpy(draft.ownedExtra, profile.ownedExtra, sizeof(draft.ownedExtra));
  std::memcpy(draft.equipped, profile.equipped, sizeof(draft.equipped));
  std::memcpy(draft.name, nameDraft, pet::NAME_BYTES);
  draft.created = true;
  draft.baselineSeconds = summary.seconds;
  draft.birthDaySeconds = summary.onDay(today);
  draft.bornDay = today;
  draft.mode = selectedMode;
  draft.goalIndex = selectedGoal;
  draft.careAt = draft.lastReadAt = std::time(nullptr);
  if (commit()) {
    todaySeconds = 0;
    weekDays = 0;
    changeTab(0);
  }
}
void PetActivity::action(fui::ActionId action, int value) {
  if (action == fui::NO_ACTION) return;
  if (!resourcesReady()) {
    if (action == petui::Back) back();
    return;
  }
  if (profile.created && action != petui::Back && action != petui::Read && !refreshDay()) return;
  if (view == pet::View::Death && action != petui::AcknowledgeDeath && action != petui::Back) return;
  if (action == petui::Back) {
    back();
    return;
  }
  draft = profile;
  switch (action) {
    case petui::OpenReadingInfo:
      returnView = view;
      show(pet::View::ReadingInfo);
      return;
    case petui::OpenMemories:
      tab = pet::Tab::Album;
      page = 0;
      show(pet::View::Memories);
      return;
    case petui::OpenCollection:
      if (!pet::collectionComplete(profile)) return;
      returnView = view;
      show(pet::View::Collection);
      return;
    case petui::AcknowledgeDeath:
      acknowledgeDeath(value == 1);
      return;
    case petui::ChangeTab:
      changeTab(value);
      return;
    case petui::SetupTab:
      if (value >= 0 && value < 3) {
        static constexpr pet::View views[] = {pet::View::New, pet::View::Info, pet::View::Rules};
        show(views[value]);
      }
      return;
    case petui::SelectSpecies:
      if (value >= 0 && value < 3 && (!profile.created || profile.lifeStatus != pet::LifeStatus::Active)) {
        chosenSpecies = pet::Species(value);
        show(pet::View::New);
      }
      return;
    case petui::Start:
      if ((profile.created && profile.lifeStatus == pet::LifeStatus::Active) || !writable || !today || historyChanged)
        return;
      std::memset(nameDraft, 0, sizeof(nameDraft));
      show(pet::View::Name);
      return;
    case petui::NameKey:
      if (view != pet::View::Name) return;
      if (value == wordlekeys::kEnter) {
        if (pet::finishName(nameDraft)) createCompanion();
        return;
      }
      if (value == wordlekeys::kErase ? pet::eraseName(nameDraft) : pet::typeName(nameDraft, char(value)))
        requestUpdate();
      return;
    case petui::ConfirmName:
      if (view == pet::View::Name && pet::finishName(nameDraft)) createCompanion();
      return;
    case petui::OpenSettings:
      returnView = view;
      selectedMode = profile.mode;
      selectedGoal = profile.goalIndex;
      show(pet::View::Settings);
      return;
    case petui::SelectMode:
      if (value >= 0 && value < 4) selectedMode = pet::Mode(value);
      show(pet::View::Settings);
      return;
    case petui::SelectGoal:
      if (value >= 0 && value < 4) selectedGoal = uint8_t(value);
      show(pet::View::Settings);
      return;
    case petui::SaveSettings:
      saveSettings(false);
      return;
    case petui::ConfirmSettings:
      if (view == pet::View::SettingsConfirm) saveSettings(true);
      return;
    case petui::OpenStatsHelp:
      notice(tr(STR_PET_READING_DAYS_HELP), view);
      return;
    case petui::OpenAccumulatedHelp:
      notice(tr(STR_PET_READING_ACCUMULATED_HELP), view);
      return;
    case petui::Feed:
      feedResult = {};
      if (!pet::feed(draft)) {
        notice(tr(STR_PET_FEED_HINT), view);
        return;
      }
      if (value >= 0 && value < 3) draft.foodChoice = uint8_t(value);
      {
        const uint8_t before = profile.saciety;
        if (commit()) {
          feedResult = {before, profile.saciety, true};
          notice(tr(STR_PET_FED_MESSAGE), pet::View::Care, petui::NoticeKind::Fed);
        }
      }
      return;
    case petui::Stroke:
      if (!pet::canCare(draft)) return;
      affection = pet::affection(draft, today);
      if (affection.first && !commit()) return;
      affectionReturn = view;
      show(pet::View::Affection);
      return;
    case petui::Rest:
      if (!pet::canCare(draft)) return;
      draft.resting = !draft.resting;
      draft.restAt = draft.resting ? std::time(nullptr) : 0;
      if (commit()) show(pet::View::Care);
      return;
    case petui::OpenGames:
      if (!pet::canCare(profile)) return;
      tab = pet::Tab::Care;
      show(pet::View::Games);
      return;
    case petui::SelectGame:
      if (!pet::canCare(profile)) return;
      if (value >= 0 && value < 3) selectedGame = pet::Game(value);
      show(pet::View::Games);
      return;
    case petui::BeginGame:
      if (!pet::canCare(profile)) return;
      if (pet::playBlock(profile) != pet::PlayBlock::None) {
        notice(petui::playBlockText(pet::playBlock(profile)), pet::View::Games);
        return;
      }
      game.begin(selectedGame, uint32_t(millis()) ^ uint32_t(profile.readingSeconds));
      show(pet::View::Play);
      return;
    case petui::GamePick:
      if (view != pet::View::Play || value < 0) return;
      game.select(unsigned(value));
      show(pet::View::Play);
      return;
    case petui::GameNext:
      if (view != pet::View::Play) return;
      game.advance();
      show(pet::View::Play);
      return;
    case petui::GameReset:
      if (view != pet::View::Play) return;
      game.resetPuzzle();
      show(pet::View::Play);
      return;
    case petui::GameDone:
      if (view != pet::View::Play || !game.finished()) return;
      {
        const bool won = writable && pet::recordGameWin(draft);
        const bool rewarded = writable && pet::rewardGame(draft, today, game.kind);
        const petui::GameResult result{profile.joy, draft.joy, profile.energy, draft.energy,
                                       rewarded ? petui::GameOutcome::Rewarded
                                       : pet::playBlock(profile) != pet::PlayBlock::None
                                           ? petui::GameOutcome::CareNeeded
                                           : petui::GameOutcome::Practice};
        if ((rewarded || won) && !commit()) return;
        gameResult = result;
        notice(rewarded                                           ? tr(STR_PET_GAME_REWARDED)
               : result.outcome == petui::GameOutcome::CareNeeded ? tr(STR_PET_GAME_RESULT_CARE)
                                                                  : tr(STR_PET_GAME_RESULT_PRACTICE),
               pet::View::Games, petui::NoticeKind::Game);
      }
      return;
    case petui::OpenHealth:
      if (pet::stage(profile.readingSeconds) == pet::Stage::Egg) return;
      tab = pet::Tab::Care;
      show(pet::View::Health);
      return;
    case petui::OpenHealthHelp:
      if (view != pet::View::Health) return;
      show(pet::View::HealthHelp);
      return;
    case petui::Clean: {
      if (!pet::clean(draft)) return;
      const petui::CareResult result{profile.hygiene, draft.hygiene, profile.joy, draft.joy, 0, true};
      if (commit()) notice(tr(STR_PET_CLEANED), pet::View::Health, petui::NoticeKind::Cleaned, result);
      return;
    }
    case petui::Cure: {
      if (!pet::cure(draft)) return;
      const petui::CareResult result{profile.hygiene,
                                     draft.hygiene,
                                     profile.joy,
                                     draft.joy,
                                     uint8_t(draft.spentMedicine - profile.spentMedicine),
                                     true};
      if (commit()) notice(tr(STR_PET_CURED), pet::View::Health, petui::NoticeKind::Cured, result);
      return;
    }
    case petui::OpenLife:
      if (value >= 0 && value < history.count) {
        lifeReturn = view == pet::View::Album ? pet::View::Album : pet::View::Lives;
        memory = value;
        show(pet::View::LifeDetail);
      }
      return;
    case petui::OpenRetire:
      if (pet::stage(profile.readingSeconds) == pet::Stage::Adult && profile.lifeStatus == pet::LifeStatus::Active)
        show(pet::View::Retire);
      return;
    case petui::ConfirmRetire:
      if (view == pet::View::Retire && pet::stage(profile.readingSeconds) == pet::Stage::Adult &&
          pet::endLife(draft, pet::LifeStatus::Retired, std::time(nullptr)) && archiveDraft() && commit())
        changeTab(0);
      return;
    case petui::NewCompanion:
      if (profile.lifeStatus != pet::LifeStatus::Active) {
        chosenSpecies = profile.species;
        show(pet::View::New);
      }
      return;
    case petui::Read: {
      const auto& books = RECENT_BOOKS.getBooks();
      if (!books.empty())
        activityManager.goToReader(books.front().path);
      else
        activityManager.goHome();
      return;
    }
    case petui::OpenProjects:
      if (!pet::homeUnlocked(profile)) return;
      tab = pet::Tab::Home;
      projectSlot = value >= 0 && value < 4 ? value : int(pet::objectSlot(profile.project));
      catalogFilter = value >= 0 && value < 4 ? petui::CatalogFilter::Owned : petui::CatalogFilter::Unowned;
      page = 0;
      show(pet::View::Projects);
      return;
    case petui::SelectCategory:
      if (!pet::homeUnlocked(profile) || value < 0 || value >= int(pet::SLOT_COUNT) || value == projectSlot) return;
      projectSlot = value;
      page = 0;
      show(pet::View::Projects);
      return;
    case petui::SelectCatalogFilter:
      if (view != pet::View::Projects || !pet::homeUnlocked(profile) || value < 0 || value > 1) return;
      catalogFilter = petui::CatalogFilter(value);
      page = 0;
      show(pet::View::Projects);
      return;
    case petui::OpenProjectPreview:
      if (view != pet::View::Projects || !pet::homeUnlocked(profile) || value < 0 ||
          pet::objectSlot(unsigned(value)) != unsigned(projectSlot) ||
          !petui::catalogMatches(profile, unsigned(value), catalogFilter))
        return;
      previewItem = uint8_t(value);
      show(pet::View::ProjectPreview);
      return;
    case petui::PreviewStep: {
      if (view != pet::View::ProjectPreview || !pet::homeUnlocked(profile)) return;
      const auto next = petui::catalogAdjacent(profile, unsigned(projectSlot), catalogFilter, previewItem, value);
      if (next >= pet::OBJECT_COUNT) return;
      previewItem = uint8_t(next);
      const unsigned perPage = renderer.getScreenWidth() > renderer.getScreenHeight() ? 4 : 6;
      page = int(petui::catalogOrdinal(profile, unsigned(projectSlot), catalogFilter, next) / perPage);
      show(pet::View::ProjectPreview);
      return;
    }
    case petui::SelectProject:
      if (view != pet::View::ProjectPreview || !writable || value != previewItem || !pet::homeUnlocked(profile) ||
          value < 0 || pet::objectSlot(unsigned(value)) != unsigned(projectSlot) ||
          !petui::catalogMatches(profile, unsigned(value), catalogFilter) ||
          !pet::chooseProject(draft, unsigned(value)))
        return;
      if (commit()) {
        if (pet::owns(profile, previewItem)) catalogFilter = petui::CatalogFilter::Owned;
        const unsigned perPage = renderer.getScreenWidth() > renderer.getScreenHeight() ? 4 : 6;
        page = int(petui::catalogOrdinal(profile, unsigned(projectSlot), catalogFilter, previewItem) / perPage);
        show(pet::View::ProjectPreview);
      }
      return;
    case petui::RemoveProject:
      if (view != pet::View::ProjectPreview || !writable || value != previewItem || !pet::homeUnlocked(profile) ||
          value < 0 || pet::objectSlot(unsigned(value)) != unsigned(projectSlot) ||
          !pet::unequip(draft, unsigned(value)))
        return;
      if (commit()) changeTab(int(pet::Tab::Home));
      return;
    case petui::Page:
      page = std::clamp(page, 0, std::max(0, work.pages - 1));
      page = std::clamp(page + value, 0, std::max(0, work.pages - 1));
      show(view);
      return;
    case petui::OpenMemory:
      if (value < 0 || value >= int(petui::MEMORY_COUNT)) return;
      memoryReturn = view == pet::View::Album ? pet::View::Album : pet::View::Memories;
      memory = value;
      show(pet::View::Memory);
      return;
    case petui::OpenLives:
      tab = pet::Tab::Album;
      page = 0;
      show(pet::View::Lives);
      return;
    case petui::Continue:
      acknowledge();
      return;
    case petui::Retry:
      return;
    case petui::OpenRules:
      show(pet::View::Rules);
      return;
    case fui::NO_ACTION:
      return;
    default:
      return;
  }
}
void PetActivity::loop() {
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("PET", "Navigation queue full");
  if (view == pet::View::Play && game.kind == pet::Game::Puzzle) {
    int x = 0, y = 0;
    const auto shown = (paintedRevision.load(std::memory_order_acquire) << 2) ^ uint32_t(renderer.getOrientation());
    if (mappedInput.wasScreenTouchPressed(x, y)) pendingSlide.begin(x, y, shown);
    const auto swipe = mappedInput.wasSwipe();
    int dx = 0, dy = 0;
    switch (swipe) {
      case MappedInputManager::SwipeDir::None:
        break;
      case MappedInputManager::SwipeDir::Left:
        dx = -1;
        break;
      case MappedInputManager::SwipeDir::Right:
        dx = 1;
        break;
      case MappedInputManager::SwipeDir::Up:
        dy = -1;
        break;
      case MappedInputManager::SwipeDir::Down:
        dy = 1;
        break;
    }
    if (swipe != MappedInputManager::SwipeDir::None || mappedInput.wasScreenTouchReleased())
      pendingSlide.end(dx, dy, shown);
  }
  int tapX = 0, tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY))
    pendingTap.capture(tapX, tapY, paintedRevision.load(std::memory_order_acquire));
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (!resourcesReady()) {
    sectionnav::Input::Event command{};
    fui::InputSnapshot input{};
    if (navigation.pop(command, 1)) {
      if (command.kind == sectionnav::Input::Kind::Back) {
        back();
        return;
      }
      if (command.kind == sectionnav::Input::Kind::Confirm && ready && interactions.routable()) {
        back();
        return;
      }
    }
    if (!ready || !interactions.routable()) return;
    int x = 0, y = 0;
    if (pendingTap.take(viewRevision, x, y)) {
      input.touchReleased = true;
      input.touchX = int16_t(x);
      input.touchY = int16_t(y);
    }
    const auto event = interactions.route(input);
    action(event.action, event.value);
    return;
  }
  if (loading && !navigation.backPending()) {
    scan();
    return;
  }
  if (uint32_t(millis() - careCheckedAt) >= 60000) {
    careCheckedAt = millis();
    if (!refreshDay()) return;
    if (view == pet::View::Care && profile.resting && today && profile.restAt > 0) requestUpdate();
    if (profile.created && profile.lifeStatus == pet::LifeStatus::Active && profile.mode != pet::Mode::Relaxed &&
        (view == pet::View::Dashboard || view == pet::View::Home || view == pet::View::ReadingInfo))
      requestUpdate();
  }
  const bool breathing =
      petsleep::breathing(profile, view) && (sleepArt.ready() || petsleep::moodVisible(profile, view));
  if (sleepClock.update(millis(), !loading && (breathing || petsleep::enabled(profile, view)))) requestUpdate();
  pet::View before = view;
  sectionnav::Input::Event command{};
  fui::InputSnapshot input{};
  if ((!ready || !interactions.routable()) && !navigation.backPending()) return;
  if (navigation.pop(command, 1)) {
    switch (command.kind) {
      case sectionnav::Input::Kind::Back:
        back();
        return;
      case sectionnav::Input::Kind::Step:
      case sectionnav::Input::Kind::Section:
        if (profile.created && view != pet::View::Name && view != pet::View::Death)
          action(petui::ChangeTab, (int(tab) + command.amount % 4 + 4) % 4);
        return;
      case sectionnav::Input::Kind::Confirm:
        input.confirm = true;
        break;
    }
  }
  if (!ready || !interactions.routable()) return;
  int slideX = 0, slideY = 0, dx = 0, dy = 0;
  const uint32_t gestureRevision = (viewRevision << 2) ^ uint32_t(renderer.getOrientation());
  if (pendingSlide.take(gestureRevision, slideX, slideY, dx, dy)) {
    if (view == pet::View::Play && game.kind == pet::Game::Puzzle && game.phase == pet::GamePhase::Playing) {
      const int tile = work.puzzleLayout.slide(game.puzzle, slideX, slideY, dx, dy);
      if (tile >= 0) action(petui::GamePick, tile);
    }
    return;
  }
  int x = 0, y = 0;
  if (pendingTap.take(viewRevision, x, y)) {
    interactions.setFocusedIndex(-1);
    input = {};
    input.touchReleased = true;
    input.touchX = int16_t(x);
    input.touchY = int16_t(y);
  }
  const int focus = interactions.focusPosition();
  const auto event =
      view == pet::View::Name ? petui::routeNaming(interactions, work.keys, input) : interactions.route(input);
  if (focus != interactions.focusPosition()) {
    ready = false;
    requestUpdate();
  }
  if (view == before) action(event.action, event.value);
}
bool PetActivity::drawPetArt(void* context, fui::DrawTarget& target, fui::Rect box, uint32_t id, bool inverse) {
  auto& app = *static_cast<PetActivity*>(context);
  if (id == petui::PETTING_HAND_ART && !app.pettingHandAvailable) return false;
  return drawArt(&app.art, target, box, id, inverse);
}
bool PetActivity::drawArt(void* context, fui::DrawTarget& target, fui::Rect box, uint32_t id, bool inverse) {
  auto& pack = *static_cast<sdart::Pack*>(context);
  if (!pack.ready()) return false;
  const auto image = pack.image(id);
  if (image.width <= 1 || image.height <= 1) return false;
  pack.draw(target, box, id, fui::BitmapMode::Contain, inverse ? fui::Color::White : fui::Color::Black);
  return pack.ready();
}
bool PetActivity::drawHomeArt(void* context, fui::DrawTarget& target, fui::Rect box, uint32_t id, bool inverse) {
  if (id < petui::HOME_ART_COUNT) return drawArt(context, target, box, id, inverse);
  auto& pack = *static_cast<sdart::Pack*>(context);
  if (!pack.ready()) return false;
  const auto image = pack.image(id);
  if (image.width != 224 || image.height != 144) return false;
  pack.draw(target, box, id, fui::BitmapMode::Stretch, inverse ? fui::Color::White : fui::Color::Black);
  return pack.ready();
}
void PetActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const fui::InputSnapshot input{};
  model = {};
  model.resourcesReady = resourcesReady();
  model.profile = &profile;
  model.game = &game;
  model.history = &history;
  model.historyWritable = historyWritable;
  model.choice = chosenSpecies;
  model.view = view;
  model.tab = tab;
  model.memoryReturn = memoryReturn;
  model.lifeReturn = lifeReturn;
  model.selectedMode = selectedMode;
  model.selectedGoal = selectedGoal;
  model.dateOrder = dateformat::fromStored(SETTINGS.dateFormat);
  model.selectedGame = selectedGame;
  model.now = std::time(nullptr);
  model.today = today;
  model.todaySeconds = todaySeconds;
  model.weekDays = weekDays;
  model.page = page;
  model.projectSlot = projectSlot;
  model.catalogFilter = catalogFilter;
  model.previewItem = previewItem;
  model.memory = memory;
  if (view == pet::View::LifeDetail && memory >= 0 && memory < history.count) {
    const auto ended = time_t(history.records[memory].endedAt);
    tm date{};
    if (ended >= 1577836800 && localtime_r(&ended, &date))
      model.lifeEndedDay = readingstats::dayIndex({date.tm_year + 1900, date.tm_mon + 1, date.tm_mday});
  }
  model.loading = loading;
  model.writable = writable;
  model.clockKnown = today != 0;
  model.historyChanged = historyChanged;
  model.notice = message;
  model.noticeKind = noticeKind;
  model.nameDraft = nameDraft;
  model.art = {this, drawPetArt};
  model.scenes = {&homeArt, drawHomeArt};
  model.portraitBackgrounds = portraitBackgroundsAvailable;
  model.habitat = {&habitatArt, drawArt};
  model.sleepArt = {&sleepArt, drawArt};
  model.farewellArt = {&farewellArt, drawArt};
  model.collectionArt = {&collectionArt, drawArt};
  model.sleepStep = uint8_t(sleepClock.step());
  model.affection = affection;
  model.feedResult = feedResult;
  model.careResult = careResult;
  model.gameResult = gameResult;
  petui::render(target, target.deviceContext(), input, interactions, toybox::headerStatus(renderer), model, work);
  if (model.resourcesReady && !resourcesReady()) {
    renderer.clearScreen();
    model.resourcesReady = false;
    petui::render(target, target.deviceContext(), input, interactions, toybox::headerStatus(renderer), model, work);
  }
  ready = true;
  toybox::reportOverflow(interactions, "Pet", &renderer);
  renderer.displayBuffer();
  if (animationRebasePending) {
    // Let the newly visible section settle before its first animation refresh.
    sleepClock.restart(millis());
    animationRebasePending = false;
  }
  paintedRevision.store(viewRevision, std::memory_order_release);
}
