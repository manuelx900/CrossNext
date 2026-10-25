#include "ReadingQuestActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RecentBooksStore.h>

#include <algorithm>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "components/SideButtonNavigation.h"
namespace fui = freeink::ui;
std::unique_ptr<Activity> ReadingQuestActivity::create(GfxRenderer& r, MappedInputManager& i) {
  auto app = makeUniqueNoThrow<ReadingQuestActivity>(r, i);
  if (!app) LOG_ERR("QUEST", "OOM: %u bytes", unsigned(sizeof(ReadingQuestActivity)));
  return app;
}
void ReadingQuestActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  if (!assets.open(questassets::PATH, questassets::COUNT)) {
    loading = false;
    requestUpdate();
    return;
  }
  today = readingstats::today();
  summary.anchorDay = today;
  failed = !quest::load(profile, writable, storeWork);
  loading = repository.begin(false, historyReadAhead, sizeof(historyReadAhead));
  if (!loading) failed = true;
  requestUpdate();
}
void ReadingQuestActivity::onExit() {
  repository.close();
  assets.close();
  Activity::onExit();
}
void ReadingQuestActivity::action(fui::ActionId action, int value) {
  if (!assets.ready()) {
    if (action == questui::Back) shelf::leave(renderer, mappedInput);
    return;
  }
  if (loading) return;
  if (quest::hasLevelUp(profile)) {
    if (action != questui::LevelContinue) return;
    draft = profile;
    quest::acknowledgeLevel(draft);
    levelSaveFailed = !quest::save(draft, writable, storeWork);
    if (!levelSaveFailed) profile = draft;
    ready = false;
    requestUpdate();
    return;
  }
  denied = false;
  if (action == questui::Gender && int(profile.gender) == value) return;
  switch (action) {
    case questui::Tab:
      if (value < 0 || value > 2 || value == tab) return;
      tab = value;
      break;
    case questui::Category:
      if (value < 0 || value >= quest::kSlotCount) return;
      category = value;
      page = 0;
      preview = -1;
      break;
    case questui::InventoryView:
      ownedGear = value != 0;
      page = 0;
      preview = -1;
      break;
    case questui::Page:
      if (tab == 1)
        questPage = std::clamp(work.page + value, 0, work.pages - 1);
      else
        page = std::clamp(work.page + value, 0, work.pages - 1);
      break;
    case questui::Preview:
      preview = value;
      break;
    case questui::Back:
      preview = -1;
      break;
    case questui::Gender:
    case questui::Milestone:
    case questui::Outfit:
    case questui::Claim: {
      draft = profile;
      bool allowed = false;
      switch (action) {
        case questui::Gender:
          if (value >= 0 && value <= 1 && int(profile.gender) != value) {
            draft.gender = quest::Gender(value);
            allowed = true;
          }
          break;
        case questui::Milestone:
          allowed = quest::claimMilestone(draft, value, summary);
          break;
        case questui::Outfit:
          allowed = value >= 0 && value < quest::kGearCount && draft.equipped[int(quest::slot(value))] == value &&
                            (quest::slot(value) == quest::Slot::Aura || quest::slot(value) == quest::Slot::Pet)
                        ? quest::unequip(draft, quest::slot(value))
                        : quest::equip(draft, value);
          break;
        case questui::Claim:
          allowed = quest::claim(draft, today, summary.onDay(today));
          break;
      }
      denied = !allowed;
      if (allowed) {
        if (quest::save(draft, writable, storeWork)) {
          const bool purchased = action == questui::Outfit && !quest::owns(profile, value);
          profile = draft;
          if (purchased) {
            ownedGear = true;
            category = int(quest::slot(value));
            const int capacity = renderer.getScreenWidth() > renderer.getScreenHeight() ? 3 : 6;
            int position = 0;
            for (int item = quest::first(quest::Slot(category)); item < value; ++item)
              if (quest::owns(profile, item)) ++position;
            page = position / capacity;
            preview = -1;
            tab = 2;
          }
          if (action == questui::Outfit && value >= 0 && value < quest::kGearCount &&
              (quest::slot(value) == quest::Slot::Aura || quest::slot(value) == quest::Slot::Pet) &&
              profile.equipped[int(quest::slot(value))] == quest::kGearCount) {
            preview = -1;
            tab = 0;
          }
        } else
          failed = true;
      }
      break;
    }
    case questui::Read: {
      const auto& books = RECENT_BOOKS.getBooks();
      if (!books.empty()) {
        activityManager.goToReader(books.front().path);
        return;
      }
      activityManager.goHome();
      return;
    }
    default:
      return;
  }
  ready = false;
  requestUpdate();
}
void ReadingQuestActivity::loop() {
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("APP-NAV", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  sectionnav::Input::Event navCommand{};
  const bool hasNavCommand = ((!loading && ready) || navigation.backPending()) && navigation.pop(navCommand);
  const bool navBack = hasNavCommand && navCommand.kind == sectionnav::Input::Kind::Back;
  const int navStep = hasNavCommand && (navCommand.kind == sectionnav::Input::Kind::Step ||
                                        navCommand.kind == sectionnav::Input::Kind::Section)
                          ? navCommand.amount
                          : 0;
  if (navBack) {
    if (preview >= 0 && !quest::hasLevelUp(profile)) {
      action(questui::Back, 0);
      return;
    }
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (!assets.ready()) {
    int x, y;
    if (ready && sidebuttons::poll(interactions, mappedInput, x, y)) {
      fui::InputSnapshot input;
      input.touchReleased = true;
      input.touchX = x;
      input.touchY = y;
      const auto event = sidebuttons::route(interactions, input, mappedInput, *this);
      action(event.action, event.value);
    }
    return;
  }
  if (loading) {
    for (int n = 0; n < 32; ++n) {
      const auto result = repository.next(record);
      if (result == readingstats::ScanResult::Record) summary.add(record);
      if (result == readingstats::ScanResult::Error || result == readingstats::ScanResult::Finished) {
        loading = false;
        repository.close();
        failed |= result == readingstats::ScanResult::Error;
        if (!failed) {
          const auto previous = profile.xp;
          quest::refresh(profile, summary);
          if (previous != profile.xp) failed = !quest::save(profile, writable, storeWork);
        }
        requestUpdate();
        break;
      }
    }
    return;
  }
  if (quest::hasLevelUp(profile) &&
      (!interactions.hasButtonFocus() && mappedInput.wasReleased(MappedInputManager::Button::Confirm))) {
    action(questui::LevelContinue, 0);
    return;
  }
  if (ready && !quest::hasLevelUp(profile)) {
    if (preview < 0) {
      const auto event = sidebuttons::pageSwipe(interactions, mappedInput);
      if (event.action) {
        action(event.action, event.value);
        return;
      }
    }
    if (const int direction = navStep) {
      interactions.setFocusedIndex(-1);
      action(questui::Tab, (tab + direction % 3 + 3) % 3);
      return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) action(questui::Tab, (tab + 2) % 3);
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) action(questui::Tab, (tab + 1) % 3);
  int x, y;
  if (!ready || !sidebuttons::poll(interactions, mappedInput, x, y)) return;
  fui::InputSnapshot in;
  in.touchReleased = true;
  in.touchX = x;
  in.touchY = y;
  const auto event = sidebuttons::route(interactions, in, mappedInput, *this);
  action(event.action, event.value);
}
void ReadingQuestActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const fui::InputSnapshot in{};
  toybox::Frame frame(target, target.deviceContext(), in, interactions);
  toybox::Screen screen(frame);
  const bool resourcesReady = assets.ready();
  const bool levelUp = resourcesReady && !loading && quest::hasLevelUp(profile);
  questui::build(screen,
                 {&profile, &summary, levelUp ? 0 : tab, today, loading, failed, denied, category, page, preview,
                  questPage, ownedGear, &assets},
                 work);
  if (resourcesReady && !assets.ready()) {
    renderer.clearScreen();
    interactions.clear();
    questui::build(screen,
                   {&profile, &summary, tab, today, loading, failed, denied, category, page, preview, questPage,
                    ownedGear, &assets},
                   work);
  } else if (levelUp) {
    interactions.clear();
    questui::buildLevelUp(screen, profile, levelSaveFailed, work);
  }
  ready = true;
  toybox::reportOverflow(interactions, "ReadingQuest", &renderer);
  renderer.displayBuffer();
}
