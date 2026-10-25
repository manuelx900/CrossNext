#include "HabitsActivity.h"

#include <HalClock.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "../../activities/util/KeyboardEntryActivity.h"
#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "HabitsScreens.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;

namespace {
constexpr unsigned long kDatePollMs = 30000;

void trimmedName(const std::string& text, char* out, const size_t size) {
  size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) {
    out[0] = '\0';
    return;
  }
  const size_t last = text.find_last_not_of(' ');
  std::snprintf(out, size, "%.*s", static_cast<int>(last - first + 1), text.c_str() + first);
}
}  // namespace

std::unique_ptr<Activity> HabitsActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  auto activity = makeUniqueNoThrow<HabitsActivity>(renderer, input);
  if (!activity) LOG_ERR("HABITS", "OOM: HabitsActivity (%u bytes)", static_cast<unsigned>(sizeof(HabitsActivity)));
  return activity;
}

int HabitsActivity::deviceDay() {
  struct tm local{};
  // The running system clock avoids the RTC read cache around midnight.
  const time_t now = std::time(nullptr);
  if (now > 0 && localtime_r(&now, &local)) {
    const int day = habits::dayIndex(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    if (habits::validDay(day)) return day;
  }
  if (!halClock.localTime(local)) return -1;
  return habits::dayIndex(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

void HabitsActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  load();
}

void HabitsActivity::onExit() { Activity::onExit(); }

void HabitsActivity::load() {
  const auto result = repository_.load(store_, draft_);
  switch (result) {
    case habits::LoadResult::Unavailable:
    case habits::LoadResult::Damaged:
      show(View::StorageError);
      return;
    case habits::LoadResult::Empty:
      store_.count = 0;
      store_.latestDay = -1;
      for (auto& habit : store_.items) habit = habits::Habit{};
      break;
    case habits::LoadResult::Loaded:
    case habits::LoadResult::Recovered:
      break;
  }
  today_ = deviceDay();
  manualDate_ = false;
  page_ = 0;
  if (!habits::validDay(today_) || today_ < store_.latestDay) {
    show(View::Clock);
    return;
  }
  if (result == habits::LoadResult::Recovered)
    notice(tr(STR_UI_THE_LAST_WRITE_WAS_INCOMPLETE_YOUR_9B906F), View::Today);
  else
    show(View::Today);
}

void HabitsActivity::show(const View view) {
  view_ = view;
  interactionsReady_ = false;
  requestUpdate();
}

void HabitsActivity::notice(const char* message, const View returnTo) {
  notice_ = message;
  noticeReturn_ = returnTo;
  show(View::Notice);
}

void HabitsActivity::back() {
  switch (view_) {
    case View::Today:
    case View::Clock:
    case View::StorageError:
      shelf::leave(renderer, mappedInput);
      return;
    case View::Presets:
    case View::Detail:
      show(View::Today);
      return;
    case View::Menu:
      show(View::Detail);
      return;
    case View::Delete:
      show(View::Menu);
      return;
    case View::Notice:
      show(noticeReturn_);
      return;
  }
}

bool HabitsActivity::save() {
  if (!repository_.save(draft_)) {
    notice(tr(STR_UI_COULD_NOT_CONFIRM_THE_SAVE_CHECK_3ED7A6), View::StorageError);
    return false;
  }
  store_ = draft_;
  return true;
}

bool HabitsActivity::acceptName(const habits::NameResult result, const View returnTo) {
  switch (result) {
    case habits::NameResult::Ok:
      return true;
    case habits::NameResult::Empty:
      notice(tr(STR_UI_GIVE_YOUR_HABIT_A_NAME_7B02A7), returnTo);
      return false;
    case habits::NameResult::TooLong:
      notice(tr(STR_UI_USE_A_NAME_OF_32_CHARACTERS_DBAF30), returnTo);
      return false;
    case habits::NameResult::Unsupported:
      notice(tr(STR_UI_USE_ENGLISH_LETTERS_NUMBERS_AND_PUNCTUATION_5035F3), returnTo);
      return false;
    case habits::NameResult::Duplicate:
      notice(tr(STR_UI_YOU_ALREADY_HAVE_A_HABIT_WITH_313AB4), returnTo);
      return false;
    case habits::NameResult::Full:
      notice(tr(STR_UI_YOU_CAN_TRACK_12_HABITS_OPEN_EBD787), returnTo);
      return false;
  }
  return false;
}

void HabitsActivity::addHabit(const char* name) {
  displayedDay_ = 0;
  page_ = 0;
  draft_ = store_;
  if (!acceptName(habits::add(draft_, name, today_), View::Presets) || !save()) return;
  show(View::Today);
  page_ = (store_.count - 1) / pageCapacity();
}

void HabitsActivity::askName(const bool rename) {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(
      renderer, mappedInput, rename ? tr(STR_UI_RENAME_HABIT_3E99C7) : tr(STR_UI_NAME_YOUR_HABIT_727D40),
      rename ? store_.items[selected_].name : "", habits::kNameMax);
  if (!keyboard) {
    LOG_ERR("HABITS", "OOM: KeyboardEntryActivity");
    notice(tr(STR_UI_NOT_ENOUGH_MEMORY_TO_OPEN_THE_7CC7D5), view_);
    return;
  }
  interactionsReady_ = false;
  startActivityForResult(std::move(keyboard), [this, rename](const ActivityResult& result) {
    RenderLock lock(*this);
    if (result.isCancelled) {
      show(rename ? View::Menu : View::Presets);
      return;
    }
    refreshDate();
    if (view_ == View::Clock) return;
    char name[habits::kNameMax + 1];
    trimmedName(std::get<KeyboardResult>(result.data).text, name, sizeof(name));
    if (!rename) {
      addHabit(name);
      return;
    }
    draft_ = store_;
    if (!acceptName(habits::rename(draft_, selected_, name), View::Menu) || !save()) return;
    show(View::Detail);
  });
}

void HabitsActivity::askDate() {
  char initial[16];
  habits::formatDate(habits::validDay(today_) ? today_ : store_.latestDay, initial, sizeof(initial));
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(
      renderer, mappedInput, tr(STR_UI_TODAY_YYYY_MM_DD_A4715E),
      habits::validDay(today_) || habits::validDay(store_.latestDay) ? initial : "", 10);
  if (!keyboard) {
    LOG_ERR("HABITS", "OOM: date keyboard");
    notice(tr(STR_UI_NOT_ENOUGH_MEMORY_TO_OPEN_THE_7CC7D5), view_);
    return;
  }
  const View returnTo = view_;
  interactionsReady_ = false;
  startActivityForResult(std::move(keyboard), [this, returnTo](const ActivityResult& result) {
    RenderLock lock(*this);
    if (result.isCancelled) {
      show(returnTo);
      return;
    }
    int date = -1;
    if (!habits::parseDate(std::get<KeyboardResult>(result.data).text.c_str(), date)) {
      notice(tr(STR_UI_ENTER_A_REAL_DATE_AS_YYYY_A18F8D), returnTo);
      return;
    }
    if (date < store_.latestDay) {
      notice(tr(STR_UI_THAT_DATE_IS_EARLIER_THAN_SAVED_41DE2A), returnTo);
      return;
    }
    today_ = date;
    manualDate_ = date != deviceDay();
    page_ = 0;
    show(View::Today);
  });
}

int HabitsActivity::pageCapacity() const {
  fui::DeviceContext device;
  device.width = renderer.getScreenWidth();
  device.height = renderer.getScreenHeight();
  return view_ == View::Presets ? habitsui::presetCapacity(device) : habitsui::todayCapacity(device);
}

void HabitsActivity::refreshDate() {
  if (manualDate_) return;
  const int date = deviceDay();
  if (!habits::validDay(date) || date < store_.latestDay) {
    show(View::Clock);
    return;
  }
  if (date == today_) return;
  today_ = date;
  selectedDay_ = today_;
  historyStart_ = habits::monthStart(today_);
  interactionsReady_ = false;
  requestUpdate();
}

void HabitsActivity::loop() {
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("APP-NAV", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  sectionnav::Input::Event navCommand{};
  const bool hasNavCommand = navigation.pop(navCommand);
  const bool navBack = hasNavCommand && navCommand.kind == sectionnav::Input::Kind::Back;
  const int navStep = hasNavCommand && (navCommand.kind == sectionnav::Input::Kind::Step ||
                                        navCommand.kind == sectionnav::Input::Kind::Section)
                          ? navCommand.amount
                          : 0;
  if (navBack) {
    back();
    return;
  }
  if (view_ != View::Clock && view_ != View::StorageError && millis() - lastDatePoll_ >= kDatePollMs) {
    lastDatePoll_ = millis();
    refreshDate();
  }
  const int held = navStep;
  const auto swipe = mappedInput.wasSwipe();
  const int horizontal = interactionsReady_ && interactions_.routable() ? sidebuttons::horizontalStep(mappedInput) : 0;
  const bool next = held > 0 || mappedInput.wasReleased(MappedInputManager::Button::Right) ||
                    swipe == MappedInputManager::SwipeDir::Up || horizontal > 0;
  const bool prev = held < 0 || mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                    swipe == MappedInputManager::SwipeDir::Down || horizontal < 0;
  fui::ActionEvent event{};
  if (view_ == View::Detail && horizontal) {
    event.action = habitsui::ActionHistory;
    event.value = static_cast<int16_t>(horizontal);
  } else if ((next || prev) && (view_ == View::Today || view_ == View::Presets)) {
    event.action = habitsui::ActionPage;
    event.value = navStep ? navStep : next ? 1 : -1;
  } else {
    int x, y;
    if (!sidebuttons::poll(interactions_, mappedInput, x, y, false) || !interactionsReady_) return;
    if (view_ != View::Clock && view_ != View::StorageError) {
      const int shownDay = today_;
      const View shownView = view_;
      refreshDate();
      // The date and calendar selection now mean something different. Paint
      // that change before accepting another gesture against the new day.
      if (today_ != shownDay || view_ != shownView) return;
    }
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(x);
    input.touchY = static_cast<int16_t>(y);
    event = sidebuttons::route(interactions_, input, mappedInput, *this, false);
    if (event.action == habitsui::ActionCalendar) {
      const int day = habitsui::calendarDayAt(toybox::makeTarget(renderer).deviceContext(),
                                              {&store_, selected_, today_, historyStart_, selectedDay_}, x, y);
      if (day < 0) return;
      event.action = habitsui::ActionDay;
      event.value = day - habits::kMinDay;
    }
  }
  switch (event.action) {
    case habitsui::ActionBack:
      back();
      return;
    case habitsui::ActionAdd:
      if (store_.count >= habits::kMaxHabits) {
        acceptName(habits::NameResult::Full, View::Today);
        return;
      }
      presetPage_ = 0;
      show(View::Presets);
      return;
    case habitsui::ActionPage: {
      const bool presets = view_ == View::Presets;
      const int count =
          presets ? habitsui::kPresetCount + 1 : habits::countOnDay(store_, displayedDay_ ? displayedDay_ : today_);
      const int capacity = pageCapacity();
      const int pages = std::max(1, (count + capacity - 1) / capacity);
      int& page = presets ? presetPage_ : page_;
      const int to = std::max(0, std::min(pages - 1, page + event.value));
      if (to != page) {
        page = to;
        show(view_);
      }
      return;
    }
    case habitsui::ActionBrowseDay:
      page_ = 0;
      displayedDay_ = event.value == 0
                          ? 0
                          : std::clamp((displayedDay_ ? displayedDay_ : today_) + event.value,
                                       std::max(habits::kMinDay, today_ - habits::kHistoryDays + 1), today_);
      interactionsReady_ = false;
      requestUpdate();
      return;
    case habitsui::ActionToggleToday:
      draft_ = store_;
      if (habits::toggle(draft_, event.value, displayedDay_ ? displayedDay_ : today_, today_) && save())
        show(View::Today);
      return;
    case habitsui::ActionOpen:
      if (event.value < 0 || event.value >= store_.count) return;
      selected_ = event.value;
      selectedDay_ = displayedDay_ ? displayedDay_ : today_;
      historyStart_ = habits::monthStart(selectedDay_);
      show(View::Detail);
      return;
    case habitsui::ActionPreset:
      if (event.value >= 0 && event.value < habitsui::kPresetCount) addHabit(habitsui::kPresets[event.value]);
      return;
    case habitsui::ActionCustom:
      askName(false);
      return;
    case habitsui::ActionDate:
      askDate();
      return;
    case habitsui::ActionMenu:
      show(View::Menu);
      return;
    case habitsui::ActionRename:
      askName(true);
      return;
    case habitsui::ActionDelete:
      show(View::Delete);
      return;
    case habitsui::ActionConfirmDelete:
      draft_ = store_;
      if (habits::remove(draft_, selected_) && save()) {
        page_ = 0;
        show(View::Today);
      }
      return;
    case habitsui::ActionDay:
      selectedDay_ = event.value + habits::kMinDay;
      show(View::Detail);
      return;
    case habitsui::ActionHistory: {
      const int earliest = std::max<int>(store_.items[selected_].createdDay, habits::firstDay(store_));
      historyStart_ = std::clamp(habits::shiftMonth(historyStart_, event.value), habits::monthStart(earliest),
                                 habits::monthStart(today_));
      show(View::Detail);
      return;
    }
    case habitsui::ActionCalendar:
      return;
    case habitsui::ActionToggleDay:
      draft_ = store_;
      if (habits::toggle(draft_, selected_, selectedDay_, today_) && save()) show(View::Detail);
      return;
    case habitsui::ActionRetry:
      load();
      return;
    case habitsui::ActionDismiss:
      show(noticeReturn_);
      return;
    default:
      return;  // No action, or an event belonging to another screen.
  }
}

void HabitsActivity::render(RenderLock&&) {
  interactions_.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  target.setFont(4, toybox::kLibraryFontId);
  pixelui::bindFonts(target, renderer);
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  const auto device = target.deviceContext();
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::Today:
      habitsui::buildToday(screen, {&store_, today_, page_, manualDate_, displayedDay_, -1});
      break;
    case View::Presets:
      habitsui::buildPresets(screen, presetPage_);
      break;
    case View::Detail:
      habitsui::buildDetail(screen, {&store_, selected_, today_, historyStart_, selectedDay_});
      break;
    case View::Menu:
      habitsui::buildMenu(screen, store_.items[selected_].name);
      break;
    case View::Delete:
      habitsui::buildDelete(screen, store_.items[selected_].name);
      break;
    case View::Clock:
      habitsui::buildMessage(screen, tr(STR_UI_CHECK_THE_DATE_9260D0), tr(STR_UI_SET_TODAY_S_DATE_TO_KEEP_D09672),
                             habitsui::ActionDate, tr(STR_UI_SET_TODAY_S_DATE_444EF7));
      break;
    case View::StorageError:
      habitsui::buildMessage(screen, tr(STR_UI_CHECK_THE_SD_CARD_959EC3),
                             tr(STR_UI_HABIT_HISTORY_COULD_NOT_BE_OPENED_7A8B77), habitsui::ActionRetry,
                             tr(STR_UI_TRY_AGAIN_B2CA91));
      break;
    case View::Notice:
      habitsui::buildMessage(screen, tr(STR_UI_HABITS_630758), notice_, habitsui::ActionDismiss, "OK");
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Habits", &renderer);
  renderer.displayBuffer();
}
