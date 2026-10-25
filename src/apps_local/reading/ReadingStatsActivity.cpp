#include "ReadingStatsActivity.h"

#include <CrossPointSettings.h>
#include <I18n.h>
#include <RecentBooksStore.h>

#include <algorithm>
#include <cstring>

#include "../ui/PixelMenuTheme.h"
#include "ReadingStatsScreens.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;
bool ReadingStatsActivity::beginScan(bool reverse) { return repository.begin(reverse, readAhead, sizeof(readAhead)); }
void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  currentDay = readingstats::today();
  clockKnown = currentDay != 0;
  displayedDay = currentDay;
  readingstats::setPeriod(dayStats, readingstats::Period::Day, currentDay, SETTINGS.weekStartsSunday);
  auto target = toybox::makeTarget(renderer);
  pageSize = readingui::capacity(target.deviceContext());
  const auto date = readingstats::dateFromDay(currentDay ? currentDay : readingstats::dayIndex({2026, 1, 1}));
  month = date.year * 12 + date.month - 1;
  pickerYear = date.year;
  summary.anchorDay = currentDay;
  finishedCapacity = pageSize;
  loading = beginScan(currentDay == 0) ? (currentDay ? Load::Summary : Load::Anchor) : Load::Failed;
  if (*initialBook) {
    std::snprintf(detail.document, sizeof(detail.document), "%s", initialBook);
    tab = 3;
    details = true;
    repository.close();
    loading = beginScan(true) ? Load::Detail : Load::Failed;
  }
  requestUpdate();
}
void ReadingStatsActivity::onExit() {
  repository.close();
  Activity::onExit();
}
void ReadingStatsActivity::back() {
  if (pickerVisible) {
    pickerVisible = false;
    periodFromPicker = false;
    tab = 1;
    ready = false;
    requestUpdate();
    return;
  }
  if (periodFromPicker && period != readingstats::Period::Calendar) {
    period = readingstats::Period::Calendar;
    pickerVisible = true;
    ready = false;
    repository.close();
    loading = Load::Ready;
    requestUpdate();
    return;
  }
  if (*initialBook) {
    finish();
    return;
  }
  if (tab == 2 && period != readingstats::Period::Calendar) {
    period = readingstats::Period::Calendar;
    loading = Load::Ready;
    repository.close();
    ready = false;
    requestUpdate();
    return;
  }
  if (details) {
    details = false;
    loading = Load::Ready;
    repository.close();
    ready = false;
    requestUpdate();
    return;
  }
  activityManager.goHome(HomeMenuItem::READING_STATS);
}
void ReadingStatsActivity::loadPage(int next) {
  page = next;
  count = 0;
  skipped = 0;
  selected = 0;
  ready = false;
  repository.close();
  if (filter == readingstats::BookFilter::Recent || (finishedPage == page && finishedCapacity == pageSize)) {
    if (filter == readingstats::BookFilter::Recent) {
      page = std::clamp(page, 0, std::max(0, (recentCount - 1) / pageSize));
      count = std::min(pageSize, recentCount - page * pageSize);
    } else {
      count = finishedCount;
    }
    loading = Load::Ready;
  } else {
    finishedPage = -1;
    finishedCapacity = pageSize;
    loading = beginScan(true) ? Load::Books : Load::Failed;
  }
  requestUpdate();
}
void ReadingStatsActivity::loadPeriod(readingstats::Period next, int day) {
  if (loading == Load::Ready && next == period && day >= periodStats.first && day <= periodStats.last) return;
  const int earliest = std::max(readingstats::dayIndex({2020, 1, 1}), currentDay - readingstats::kHistoryDays + 1);
  if (day < (next == readingstats::Period::Year || next == readingstats::Period::Month
                 ? readingstats::dayIndex({2020, 1, 1})
                 : earliest) ||
      day > currentDay)
    return;
  period = next;
  readingstats::setPeriod(periodStats, period, day, SETTINGS.weekStartsSunday);
  if (period == readingstats::Period::Month) {
    int through = std::min(periodStats.last, currentDay);
    if (through == currentDay && !summary.onDay(through)) --through;
    periodStats.consistency.begin(through);
    streakPassFirst = periodStats.consistency.first;
  }
  ready = false;
  loading = beginScan(true) ? Load::Period : Load::Failed;
  requestUpdate();
}
void ReadingStatsActivity::scan() {
  for (int n = 0; n < 32; ++n) {
    const auto status = repository.next(record);
    if (status == readingstats::ScanResult::Error) {
      loading = Load::Failed;
      repository.close();
      requestUpdate();
      return;
    }
    if (status == readingstats::ScanResult::Finished) {
      damaged |= repository.hadDamage();
      repository.close();
      const auto& streak = periodStats.consistency;
      if ((loading == Load::Period || loading == Load::Streak) && period == readingstats::Period::Month &&
          streak.first < streakPassFirst && streak.first > streak.minimumDay) {
        // Repeat only the interval projection; aggregates are counted once.
        // A second pass can join dates recorded out of order after a clock correction.
        streakPassFirst = streak.first;
        loading = beginScan(true) ? Load::Streak : Load::Failed;
        if (loading == Load::Failed) requestUpdate();
        return;
      }
      if (loading == Load::Anchor || loading == Load::Summary) {
        summaryReady = true;
        finishedPage = 0;
        dayReady = clockKnown;
      } else if (loading == Load::Day)
        dayReady = true;
      else if (loading == Load::Books) {
        finishedPage = page;
        finishedCount = count;
      }
      loading = Load::Ready;
      requestUpdate();
      return;
    }
    if (status != readingstats::ScanResult::Record) continue;
    switch (loading) {
      case Load::Anchor: {
        currentDay = record.localDay + static_cast<int>((uint64_t(record.secondOfDay) + record.seconds - 1) / 86400);
        summary.anchorDay = currentDay;
        const auto d = readingstats::dateFromDay(currentDay);
        month = d.year * 12 + d.month - 1;
        repository.close();
        loading = beginScan() ? Load::Summary : Load::Failed;
        return;
      }
      case Load::Summary:
        summary.add(record);
        if (clockKnown) dayStats.add(record);
        readingstats::addRecent(books, recentCount, record.book);
        if (record.firstCompletion) {
          const int last = std::min(finishedCount, pageSize - 1);
          for (int at = last; at > 0; --at) finishedBooks[at] = finishedBooks[at - 1];
          finishedBooks[0] = record.book;
          finishedCount = std::min(pageSize, finishedCount + 1);
        }
        break;
      case Load::Day:
        dayStats.add(record);
        break;
      case Load::Period:
        periodStats.add(record);
        break;
      case Load::Streak:
        periodStats.consistency.add(record);
        break;
      case Load::Books:
        if (filter == readingstats::BookFilter::Recent) {
          readingstats::addRecent(books, recentCount, record.book);
        } else if (record.firstCompletion && skipped++ >= page * pageSize) {
          finishedBooks[count++] = record.book;
          if (count == pageSize) {
            damaged |= repository.hadDamage();
            repository.close();
            loading = Load::Ready;
            finishedPage = page;
            finishedCount = count;
            requestUpdate();
            return;
          }
        }
        break;
      case Load::Detail:
        if (!std::strcmp(detail.document, record.book.document)) {
          detail = record.book;
          damaged |= repository.hadDamage();
          repository.close();
          loading = Load::Ready;
          requestUpdate();
          return;
        }
        break;
      case Load::Ready:
      case Load::Failed:
        return;
    }
  }
}
void ReadingStatsActivity::action(fui::ActionId action, int value) {
  switch (action) {
    case readingui::ChoosePeriod:
      if (!clockKnown || currentDay < readingstats::dayIndex({2020, 1, 1})) return;
      pickerVisible = true;
      periodFromPicker = true;
      ready = false;
      requestUpdate();
      return;
    case readingui::PickerYear:
      if (!pickerVisible || !clockKnown || currentDay < readingstats::dayIndex({2020, 1, 1})) return;
      pickerYear = std::clamp(pickerYear + value, 2020, readingstats::dateFromDay(currentDay).year);
      ready = false;
      requestUpdate();
      return;
    case readingui::PickerMonth:
      if (value < 1 || value > 12) return;
      pickerVisible = false;
      periodFromPicker = true;
      tab = 1;
      loadPeriod(readingstats::Period::Month, readingstats::dayIndex({pickerYear, value, 1}));
      return;
    case readingui::SelectYear:
      pickerVisible = false;
      periodFromPicker = true;
      tab = 1;
      loadPeriod(readingstats::Period::Year, readingstats::dayIndex({pickerYear, 1, 1}));
      return;
    case readingui::ContinueReading: {
      const auto& books = RECENT_BOOKS.getBooks();
      if (books.empty())
        activityManager.goToFileBrowser();
      else
        activityManager.goToReader(books.front().path);
      return;
    }
    case readingui::Day:
      if (!clockKnown || loading != Load::Ready) return;
      {
        const int next = std::clamp(displayedDay + value, currentDay - readingstats::kHistoryDays + 1, currentDay);
        if (next == displayedDay) return;
        displayedDay = next;
      }
      readingstats::setPeriod(dayStats, readingstats::Period::Day, displayedDay, SETTINGS.weekStartsSunday);
      dayReady = false;
      ready = false;
      loading = beginScan() ? Load::Day : Load::Failed;
      requestUpdate();
      return;
    case readingui::Tab:
      if (value < 0 || value > 3 || !summaryReady) return;
      if (value == tab && !details && !pickerVisible && period == readingstats::Period::Calendar) return;
      repository.close();
      loading = Load::Ready;
      details = false;
      pickerVisible = false;
      periodFromPicker = false;
      tab = value;
      period = readingstats::Period::Calendar;
      if (tab == 3)
        loadPage(0);
      else {
        if (tab == 0 && clockKnown && !dayReady) {
          readingstats::setPeriod(dayStats, readingstats::Period::Day, displayedDay, SETTINGS.weekStartsSunday);
          loading = beginScan() ? Load::Day : Load::Failed;
        }
        ready = false;
        requestUpdate();
      }
      return;
    case readingui::Page:
      if (tab == 2) {
        if (!summary.anchorDay) return;
        const int earliest = readingstats::dateFromDay(summary.anchorDay - readingstats::kHistoryDays + 1).year * 12 +
                             readingstats::dateFromDay(summary.anchorDay - readingstats::kHistoryDays + 1).month - 1;
        const auto last = readingstats::dateFromDay(currentDay);
        const int next = std::clamp(month + value, earliest, last.year * 12 + last.month - 1);
        if (next == month) return;
        month = next;
        ready = false;
        requestUpdate();
      } else if (tab == 3) {
        const int pages = std::max(
            1, static_cast<int>(
                   ((filter == readingstats::BookFilter::Recent ? recentCount : summary.completed) + pageSize - 1) /
                   pageSize));
        const int next = std::clamp(page + value, 0, pages - 1);
        if (next != page) loadPage(next);
      }
      return;
    case readingui::OpenBook:
      if (value < 0 || value >= count) return;
      detail = filter == readingstats::BookFilter::Recent ? books[value + page * pageSize] : finishedBooks[value];
      details = true;
      ready = false;
      loading = beginScan(true) ? Load::Detail : Load::Failed;
      requestUpdate();
      return;
    case readingui::SelectChartDay:
    case readingui::SelectDay:
      loadPeriod(readingstats::Period::Day, value);
      return;
    case readingui::SelectWeek:
      loadPeriod(readingstats::Period::Week, value);
      return;
    case readingui::SelectMonth:
      loadPeriod(readingstats::Period::Month, std::max(value, std::max(readingstats::dayIndex({2020, 1, 1}),
                                                                       currentDay - readingstats::kHistoryDays + 1)));
      return;
    case readingui::BackToCalendar:
    case readingui::BackToBooks:
    case readingui::BackToData:
      back();
      return;
    case readingui::Filter:
      if (value < 0 || value > 1) return;
      filter = static_cast<readingstats::BookFilter>(value);
      loadPage(0);
      return;
    case readingui::PeriodPage: {
      if (period == readingstats::Period::Calendar) return;
      int day = periodStats.first + (period == readingstats::Period::Week ? 7 : 1) * value;
      if (period == readingstats::Period::Year) {
        day = readingstats::dayIndex({readingstats::dateFromDay(periodStats.first).year + value, 1, 1});
      } else if (period == readingstats::Period::Month) {
        const auto d = readingstats::dateFromDay(periodStats.first);
        const int at = d.year * 12 + d.month - 1 + value;
        day = readingstats::dayIndex({at / 12, at % 12 + 1, 1});
      }
      const int earliest = std::max(readingstats::dayIndex({2020, 1, 1}), currentDay - readingstats::kHistoryDays + 1);
      loadPeriod(period,
                 std::clamp(day, periodFromPicker ? readingstats::dayIndex({2020, 1, 1}) : earliest, currentDay));
      return;
    }
    default:
      return;
  }
}
void ReadingStatsActivity::loop() {
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("STATS", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  sectionnav::Input::Event command;
  bool navigated = false;
  while (navigation.pop(command)) {
    if (command.kind == sectionnav::Input::Kind::Back) {
      navigation.clear();
      back();
      return;
    }
    if (*initialBook || details || pickerVisible) continue;
    if (command.kind == sectionnav::Input::Kind::Step) {
      const int from = queuedTab >= 0 ? queuedTab : tab;
      const int next = (from + command.amount % 4 + 4) % 4;
      if (loading == Load::Anchor || loading == Load::Summary)
        queuedTab = next;
      else {
        action(readingui::Tab, next);
        navigated = true;
      }
    } else if (command.kind == sectionnav::Input::Kind::Section && (tab == 2 || tab == 3)) {
      action(readingui::Page, command.amount);
      navigated = true;
    }
  }
  if (navigated) return;
  if (tab == 3 && !details) {
    auto target = toybox::makeTarget(renderer);
    const int capacity = readingui::capacity(target.deviceContext());
    if (pageSize != capacity) {
      const int first = page * pageSize;
      pageSize = capacity;
      loadPage(first / pageSize);
      return;
    }
  }
  if (loading == Load::Anchor || loading == Load::Summary) {
    scan();
    if (loading == Load::Ready && queuedTab >= 0) {
      const int next = queuedTab;
      queuedTab = -1;
      action(readingui::Tab, next);
    }
    return;
  }
  if (loading != Load::Ready && loading != Load::Failed) {
    scan();
    return;
  }
  if (loading == Load::Failed) return;
  if (ready && !details) {
    if (const int direction = sidebuttons::horizontalStep(mappedInput)) {
      if (pickerVisible)
        action(readingui::PickerYear, direction);
      else if (tab == 0)
        action(readingui::Day, direction);
      else if (tab == 3)
        action(readingui::Page, direction);
      else if (tab == 2 || (tab == 1 && periodFromPicker))
        action(period == readingstats::Period::Calendar ? readingui::Page : readingui::PeriodPage, direction);
      return;
    }
  }
  if (!*initialBook && mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    action(readingui::Tab, (tab + 3) % 4);
    return;
  }
  if (!*initialBook && mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    action(readingui::Tab, (tab + 1) % 4);
    return;
  }
  if (tab == 3 && !details) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      if (selected + 1 < count) {
        ++selected;
        requestUpdate();
      } else
        action(readingui::Page, 1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      if (selected > 0) {
        --selected;
        requestUpdate();
      } else
        action(readingui::Page, -1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      action(readingui::OpenBook, selected);
      return;
    }
  }
  if (tab == 2 || (tab == 1 && periodFromPicker && !pickerVisible)) {
    const auto move = period == readingstats::Period::Calendar ? readingui::Page : readingui::PeriodPage;
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) action(move, -1);
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) action(move, 1);
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && period == readingstats::Period::Calendar)
      action(readingui::SelectMonth, std::min(currentDay, readingstats::dayIndex({month / 12, month % 12 + 1, 1})));
  }
  int x, y;
  if (!ready || !mappedInput.wasScreenTapped(x, y)) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = x;
  input.touchY = y;
  const auto event = interactions.route(input);
  if (event.action == readingui::SelectDay) {
    const int day = readingui::calendarDayAt(uiWork, month, SETTINGS.weekStartsSunday, x, y);
    if (day) action(readingui::SelectDay, day);
  } else
    action(event.action, event.value);
}
void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  bool unicode = I18N.getLanguage() != Language::EN;
  for (const auto& b : books)
    for (unsigned char c : b.title) unicode |= c >= 0x80;
  for (unsigned char c : detail.title) unicode |= c >= 0x80;
  pixelui::bindFonts(target, renderer, unicode);
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions);
  toybox::Screen screen(frame);
  readingui::Model model;
  uiWork.status.status.battery.label = nullptr;
  GUI.applyHeaderStatus(renderer, uiWork.status);
  model.battery = uiWork.status.status.battery.percent;
  model.charging = uiWork.status.status.battery.charging;
  model.showPercent = uiWork.status.status.battery.label != nullptr;
  model.displayedDay = displayedDay;
  model.summary = &summary;
  model.periodStats = tab == 0 ? &dayStats : &periodStats;
  model.period = period;
  model.filter = filter;
  model.bookTotal = filter == readingstats::BookFilter::Recent ? recentCount : summary.completed;
  model.weekStartsSunday = SETTINGS.weekStartsSunday;
  model.books = filter == readingstats::BookFilter::Recent ? books + page * pageSize : finishedBooks;
  model.detail = details ? &detail : nullptr;
  model.bookOnly = *initialBook;
  model.periodFromPicker = periodFromPicker;
  model.pickerVisible = pickerVisible;
  model.pickerYear = pickerYear;
  model.today = tab == 0 && !clockKnown ? 0 : currentDay;
  model.month = month;
  model.tab = tab;
  model.page = page;
  model.count = count;
  model.selected = selected;
  model.pageSize = pageSize;
  model.loading = (loading != Load::Ready && loading != Load::Failed) ||
                  (tab == 3 && !details && pageSize != readingui::capacity(target.deviceContext()));
  model.failed = loading == Load::Failed;
  model.damaged = damaged;
  readingui::build(screen, model, uiWork);
  ready = true;
  toybox::reportOverflow(interactions, "ReadingStats");
  renderer.displayBuffer();
}
