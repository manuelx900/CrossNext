#include "ReadingStatsActivity.h"

#include <CrossPointSettings.h>
#include <I18n.h>
#include <Memory.h>
#include <RecentBooksStore.h>

#include <algorithm>
#include <cstring>

#include "../ui/PixelMenuTheme.h"
#include "CrossInkStatsImport.h"
#include "ReadingGoal.h"
#include "ReadingStatsCloudClient.h"
#include "ReadingStatsScreens.h"
#include "ReadingStatsSyncActivity.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;
bool ReadingStatsActivity::beginScan(bool reverse) { return repository.begin(reverse, readAhead, sizeof(readAhead)); }
void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  source = readingstats::Source::Local;
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  currentDay = readingstats::today();
  if (currentDay && SETTINGS.readingDailyGoal && !SETTINGS.readingGoalStartDay) {
    SETTINGS.readingGoalStartDay = currentDay;
    if (!SETTINGS.saveToFile()) {
      SETTINGS.readingGoalStartDay = 0;
      goalSaveFailed = true;
    }
  }
  clockKnown = currentDay != 0;
  displayedDay = currentDay;
  readingstats::setPeriod(dayStats, readingstats::Period::Day, currentDay, SETTINGS.weekStartsSunday);
  auto target = toybox::makeTarget(renderer);
  pageSize = readingui::capacity(target.deviceContext());
  const auto date = readingstats::dateFromDay(currentDay ? currentDay : readingstats::dayIndex({2026, 1, 1}));
  month = date.year * 12 + date.month - 1;
  pickerYear = date.year;
  readingstats::setPeriod(annualStats, readingstats::Period::Year, currentDay, SETTINGS.weekStartsSunday);
  summary.anchorDay = currentDay;
  finishedCapacity = pageSize;
  if (!*initialBook && history != History::Local &&
      (readingstats::crossink::installed() || readingstats::cloud::Client::configured())) {
    cloud = makeUniqueNoThrow<readingstats::cloud::Reader>();
    if (!cloud) LOG_ERR("STATS", "OOM: imported history reader");
    loading = cloud ? Load::Cloud : Load::Failed;
  } else
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
  periodView.close();
  daysView.close();
  repository.close();
  completionIndex.reset();
  completionMemory.reset();
  if (combined) combined->close();
  if (cloud) cloud->close();
  Activity::onExit();
}
void ReadingStatsActivity::back() {
  for (auto* view : {&periodView, &daysView})
    if (view->state() != readingstats::PagedView::State::Ready) view->close();
  if (detailPage == DetailPage::Days) {
    detailPage = DetailPage::Summary;
    loading = Load::Ready;
    repository.close();
    ready = false;
    requestUpdate();
    return;
  }
  if (periodBooksVisible) {
    if (details)
      details = false;
    else
      periodBooksVisible = false;
    loading = Load::Ready;
    repository.close();
    ready = false;
    requestUpdate();
    return;
  }
  if (goalCalendarVisible) {
    goalCalendarVisible = false;
    requestUpdate();
    return;
  }
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
    if (detailReset) {
      detailReset = false;
      if (summaryReady)
        loadPage(0);
      else {
        ready = false;
        requestUpdate();
      }
      return;
    }
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
  if (source == readingstats::Source::Unified) {
    const int total = bookTotal();
    page = cloudListedKnown ? std::clamp(page, 0, std::max(0, (total - 1) / pageSize)) : 0;
    cloudBookOrdinal = 0;
    if (!cloudListedKnown) listedCloudFinished = listedCloudRecent = 0;
    loading = cloudAvailable ? Load::CloudBooks : Load::Failed;
    requestUpdate();
    return;
  }
  if (filter == readingstats::BookFilter::Recent && recentCount < 0) {
    page = recentCount = 0;
    loading = beginScan() ? Load::Books : Load::Failed;
  } else if (filter == readingstats::BookFilter::Recent || (finishedPage == page && finishedCapacity == pageSize)) {
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
  if (source == readingstats::Source::Unified) {
    loading = cloudAvailable && combined->project(periodStats, &cloudPeriodKnown) ? Load::Ready : Load::Failed;
    requestUpdate();
    return;
  }
  loading = beginScan(true) ? Load::Period : Load::Failed;
  requestUpdate();
}
void ReadingStatsActivity::loadAnnual(int year) {
  if (year < readingstats::RecordedYears::firstYear || year > readingstats::RecordedYears::lastYear) return;
  statsScope = readingui::StatsScope::Year;
  ready = false;
  if (annualReady && readingstats::dateFromDay(annualStats.first).year == year) {
    requestUpdate();
    return;
  }
  readingstats::setPeriod(annualStats, readingstats::Period::Year, readingstats::dayIndex({year, 1, 1}),
                          SETTINGS.weekStartsSunday);
  annualKnown = true;
  annualReady = false;
  if (source == readingstats::Source::Unified) {
    loading = combined->projectAnnual(annualStats, annualKnown, annualFinishedKnown) ? Load::Ready : Load::Failed;
    annualReady = loading == Load::Ready;
  } else
    loading = beginScan() ? Load::Annual : Load::Failed;
  requestUpdate();
}
bool ReadingStatsActivity::skipLoopDelay() { return loading != Load::Ready && loading != Load::Failed; }
bool ReadingStatsActivity::showPreparedPage() {
  const bool dates = loading == Load::BookDays;
  auto& view = dates ? daysView : periodView;
  int visible = 0;
  const int current = dates ? bookDays.page : periodBooks.page;
  const int slots = dates ? bookDays.capacity : pageSize;
  if (!view.readPage(current, slots, dates ? books : finishedBooks, visible)) return false;
  if (dates) {
    bookDays.count = visible;
    bookDays.total = view.count();
    bookDays.hasNext = int64_t(current + 1) * slots < view.count();
    for (int n = 0; n < visible; ++n) bookDays.entries[n] = {books[n].seconds, books[n].lastDay};
  } else {
    periodBooks.count = visible;
    periodBooks.hasNext = int64_t(current + 1) * slots < view.count();
  }
  return true;
}
void ReadingStatsActivity::loadPeriodBooks(int direction) {
  const auto& range = tab == 0 ? dayStats : (tab == 1 && !periodFromPicker ? annualStats : periodStats);
  const int first = direction ? periodBooks.first : range.first;
  const int last = direction ? periodBooks.last : range.last;
  if (first <= 0 || last < first) return;
  pageSize = renderer.getScreenWidth() > renderer.getScreenHeight() ? 2 : readingstats::kBooksPerPage;
  periodBooks.first = first;
  periodBooks.last = last;
  periodBooks.page = direction ? std::max(0, periodBooks.page + (direction > 0 ? 1 : -1)) : 0;
  recentCount = -1;
  finishedPage = -1;
  selected = 0;
  periodBooksVisible = true;
  details = false;
  ready = false;
  repository.close();
  loading = Load::PeriodBooks;
  if (periodView.matches(readingstats::PagedView::Kind::Period, first, last))
    loading = showPreparedPage() ? Load::Ready : Load::Failed;
  else if (!periodView.begin(readingstats::PagedView::Kind::Period, books, readAhead, first, last) || !beginScan(true))
    loading = Load::Failed;
  requestUpdate();
}
void ReadingStatsActivity::loadBookDays(int direction) {
  const int slots = renderer.getScreenWidth() > renderer.getScreenHeight() ? 2 : readingstats::kBooksPerPage;
  bookDays.capacity = slots;
  bookDays.page = direction ? std::max(0, bookDays.page + (direction > 0 ? 1 : -1)) : 0;
  detailPage = DetailPage::Days;
  recentCount = -1;
  ready = false;
  repository.close();
  loading = Load::BookDays;
  if (daysView.matches(readingstats::PagedView::Kind::Days, 0, 0, detail.document))
    loading = showPreparedPage() ? Load::Ready : Load::Failed;
  else if (!daysView.begin(readingstats::PagedView::Kind::Days, books, readAhead, 0, 0, detail.document) ||
           !beginScan(true))
    loading = Load::Failed;
  requestUpdate();
}

void ReadingStatsActivity::scan() {
  auto* view = loading == Load::PeriodBooks ? &periodView : loading == Load::BookDays ? &daysView : nullptr;
  if (view && view->state() != readingstats::PagedView::State::Collecting) {
    view->step();
    if (view->state() == readingstats::PagedView::State::Ready) {
      loading = showPreparedPage() ? Load::Ready : Load::Failed;
      requestUpdate();
    } else if (view->state() == readingstats::PagedView::State::Failed) {
      loading = Load::Failed;
      requestUpdate();
    }
    return;
  }
  if (loading == Load::CloudBooks) {
    for (int n = 0; n < 32; ++n) {
      if (cloudBookOrdinal == combined->count() || (cloudListedKnown && count == pageSize)) {
        cloudListedKnown = true;
        loading = Load::Ready;
        requestUpdate();
        return;
      }
      if (!combined->bookAt(cloudBookOrdinal++, record.book)) {
        loading = Load::Failed;
        requestUpdate();
        return;
      }
      if (!record.book.visibleInBookHistory()) continue;
      if (!cloudListedKnown) {
        if (record.book.completed)
          ++listedCloudFinished;
        else
          ++listedCloudRecent;
      }
      if (record.book.completed != (filter == readingstats::BookFilter::Finished)) continue;
      if (skipped++ >= page * pageSize && count < pageSize) finishedBooks[count++] = record.book;
    }
    return;
  }
  for (int n = 0; n < 32; ++n) {
    if (view && view->pending()) {
      if (!view->drain()) {
        repository.close();
        loading = Load::Failed;
        requestUpdate();
        return;
      }
      continue;
    }
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
      if (view) {
        if (!view->finishInput()) {
          loading = Load::Failed;
          requestUpdate();
        }
        return;
      }
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
        if (loading == Load::Summary) summary.completed = listedFinished;
        summaryReady = true;
        annualReady = true;
        finishedPage = -1;
        dayReady = clockKnown;
      } else if (loading == Load::Annual)
        annualReady = true;
      else if (loading == Load::Day)
        dayReady = true;
      else if (loading == Load::Books) {
        if (filter == readingstats::BookFilter::Recent) {
          count = std::min(pageSize, recentCount - page * pageSize);
        } else {
          finishedPage = page;
          finishedCount = count;
        }
      }
      loading = Load::Ready;
      requestUpdate();
      return;
    }
    if (status != readingstats::ScanResult::Record) continue;
    if (loading == Load::Summary && (record.book.completed || record.kind == readingstats::RecordKind::Reopen)) {
      if (!completionMemory) {
        const auto slots = readingstats::CompletionIndex::slotsFor(repository.recordCount());
        completionMemory = HalMemory::allocatePsram(slots * sizeof(readingstats::CompletionIndex::Entry));
        if (completionMemory)
          completionIndex.reset(reinterpret_cast<readingstats::CompletionIndex::Entry*>(completionMemory.get()), slots);
        else
          LOG_ERR("STATS", "OOM: completion-event index");
      }
      const bool previouslyListed = completionIndex.completed(record.book.document);
      if (!completionMemory || !completionIndex.apply(record, repository.recordOffset(), /*latestSnapshot=*/true)) {
        LOG_ERR("STATS", "Completion-event index unavailable or full");
        repository.close();
        loading = Load::Failed;
        requestUpdate();
        return;
      }
      const bool nowListed = completionIndex.completed(record.book.document);
      if (nowListed && !previouslyListed) ++listedFinished;
      if (!nowListed && previouslyListed && listedFinished) --listedFinished;
    }
    const bool currentCompletion =
        loading == Load::Books && completionIndex.current(record, repository.recordOffset(), /*latestSnapshot=*/true);
    switch (loading) {
      case Load::Anchor: {
        currentDay =
            record.localDay +
            static_cast<int>((uint64_t(record.secondOfDay) + (record.seconds ? record.seconds - 1 : 0)) / 86400);
        summary.anchorDay = currentDay;
        const auto d = readingstats::dateFromDay(currentDay);
        month = d.year * 12 + d.month - 1;
        pickerYear = d.year;
        readingstats::setPeriod(annualStats, readingstats::Period::Year, currentDay, SETTINGS.weekStartsSunday);
        repository.close();
        loading = beginScan() ? Load::Summary : Load::Failed;
        return;
      }
      case Load::Summary:
        summary.add(record);
        annualStats.add(record);
        recordedYears.add(record);
        if (clockKnown) dayStats.add(record);
        if (record.book.visibleInBookHistory()) readingstats::addRecent(books, recentCount, record.book);
        break;
      case Load::Day:
        dayStats.add(record);
        break;
      case Load::Period:
        periodStats.add(record);
        break;
      case Load::Annual:
        annualStats.add(record);
        break;
      case Load::Streak:
        periodStats.consistency.add(record);
        break;
      case Load::Books:
        if (filter == readingstats::BookFilter::Recent) {
          if (record.book.visibleInBookHistory()) readingstats::addRecent(books, recentCount, record.book);
        } else if (currentCompletion && skipped++ >= page * pageSize) {
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
      case Load::BookDays:
        if (!view->add(record)) {
          loading = Load::Failed;
          repository.close();
          requestUpdate();
          return;
        }
        break;
      case Load::PeriodBooks:
        if (!view->add(record)) {
          loading = Load::Failed;
          repository.close();
          requestUpdate();
          return;
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
      case Load::Cloud:
      case Load::CloudBooks:
        return;
    }
  }
}
void ReadingStatsActivity::action(fui::ActionId action, int value) {
  if (source == readingstats::Source::Unified && !cloudAvailable && action != readingui::ImportHistory &&
      action != readingui::Tab)
    return;
  switch (action) {
    case readingui::ResetBook: {
      if (!details || source != readingstats::Source::Local || loading != Load::Ready ||
          !readingstats::validDocument(detail.document))
        return;
      const char* options[] = {tr(STR_CANCEL), tr(STR_RESET)};
      goalPopup.showMessage(
          tr(STR_STATS_RESET_BOOK), tr(STR_STATS_RESET_BOOK_ASK), options, 2, 0,
          [this](int selected) {
            RenderLock lock(*this);
            if (selected == 1) resetState = ResetState::Requested;
            requestUpdate();
          },
          12);
      requestUpdate();
      return;
    }
    case readingui::BookDays:
      if (details && loading == Load::Ready && readingstats::validDocument(detail.document)) loadBookDays();
      return;
    case readingui::BookDaysPage:
      if (detailPage != DetailPage::Days || loading != Load::Ready) return;
      if ((value < 0 && bookDays.page > 0) || (value > 0 && bookDays.hasNext)) loadBookDays(value);
      return;
    case readingui::BookDaysBack:
      back();
      return;
    case readingui::PeriodBooks:
      if (loading != Load::Ready || details || pickerVisible || goalCalendarVisible) return;
      if (tab == 1 && !periodFromPicker && statsScope != readingui::StatsScope::Year) return;
      loadPeriodBooks();
      return;
    case readingui::PeriodBooksPage:
      if (!periodBooksVisible || details || loading != Load::Ready) return;
      if ((value < 0 && periodBooks.page > 0) || (value > 0 && periodBooks.hasNext)) loadPeriodBooks(value);
      return;
    case readingui::PeriodBooksBack:
      back();
      return;
    case readingui::GoalChoose: {
      if (SETTINGS.readingDailyGoal && !goalCalendarVisible) return;
      std::vector<std::string> options;
      options.reserve(std::size(readinggoal::minutes));
      for (unsigned minutes : readinggoal::minutes) {
        char label[48];
        if (minutes)
          std::snprintf(label, sizeof(label), tr(STR_READING_DURATION_M), static_cast<unsigned long long>(minutes));
        else
          std::snprintf(label, sizeof(label), "%s", tr(STR_NONE_OPT));
        options.emplace_back(label);
      }
      goalPopup.show(StrId::STR_GOAL_DAILY, options, SETTINGS.readingDailyGoal, [this](int n) {
        if (n < 0 || n >= int(std::size(readinggoal::minutes))) return;
        if (SETTINGS.readingDailyGoal != n) {
          const auto previousGoal = SETTINGS.readingDailyGoal;
          const auto previousStart = SETTINGS.readingGoalStartDay;
          const auto previousLegacy = SETTINGS.readingGoalLegacyHistory;
          if (!n || !SETTINGS.readingDailyGoal) SETTINGS.readingGoalStartDay = n ? currentDay : 0;
          if (!n || !SETTINGS.readingDailyGoal) SETTINGS.readingGoalLegacyHistory = false;
          SETTINGS.readingDailyGoal = n;
          if (!SETTINGS.saveToFile()) {
            SETTINGS.readingDailyGoal = previousGoal;
            SETTINGS.readingGoalStartDay = previousStart;
            SETTINGS.readingGoalLegacyHistory = previousLegacy;
            goalSaveFailed = true;
          }
        }
        requestUpdate();
      });
      requestUpdate();
      return;
    }
    case readingui::GoalReset: {
      if (!goalCalendarVisible || !SETTINGS.readingDailyGoal) return;
      const char* options[] = {tr(STR_CANCEL), tr(STR_RESET)};
      goalPopup.showMessage(tr(STR_GOAL_RESET_TITLE), tr(STR_GOAL_RESET_CONFIRM), options, 2, 0, [this](int choice) {
        if (choice == 1) {
          const auto previousGoal = SETTINGS.readingDailyGoal;
          const auto previousStart = SETTINGS.readingGoalStartDay;
          const auto previousLegacy = SETTINGS.readingGoalLegacyHistory;
          SETTINGS.readingDailyGoal = 0;
          SETTINGS.readingGoalStartDay = 0;
          SETTINGS.readingGoalLegacyHistory = false;
          if (!SETTINGS.saveToFile()) {
            SETTINGS.readingDailyGoal = previousGoal;
            SETTINGS.readingGoalStartDay = previousStart;
            SETTINGS.readingGoalLegacyHistory = previousLegacy;
            goalSaveFailed = true;
          } else {
            goalCalendarVisible = false;
          }
        }
        requestUpdate();
      });
      requestUpdate();
      return;
    }
    case readingui::GoalCalendar:
      goalCalendarVisible = true;
      goalMonth =
          clockKnown ? readingstats::dateFromDay(currentDay).year * 12 + readingstats::dateFromDay(currentDay).month - 1
                     : 0;
      requestUpdate();
      return;
    case readingui::GoalBack:
      goalCalendarVisible = false;
      requestUpdate();
      return;
    case readingui::GoalMonth:
      if (!clockKnown) return;
      goalMonth =
          std::clamp(goalMonth + value,
                     (readingstats::dateFromDay(currentDay - readingstats::kHistoryDays + 1).year * 12 +
                      readingstats::dateFromDay(currentDay - readingstats::kHistoryDays + 1).month - 1),
                     readingstats::dateFromDay(currentDay).year * 12 + readingstats::dateFromDay(currentDay).month - 1);
      requestUpdate();
      return;
    case readingui::ImportHistory:
      importHistory();
      return;
    case readingui::ChoosePeriod:
      if (!clockKnown || currentDay < readingstats::dayIndex({2020, 1, 1})) return;
      pickerVisible = true;
      periodFromPicker = true;
      ready = false;
      requestUpdate();
      return;
    case readingui::ChooseStatsScope: {
      if (tab != 1 || pickerVisible || periodFromPicker || loading != Load::Ready) return;
      std::vector<std::string> options;
      // At most 231 short labels; moved into the existing paginated popup.
      options.reserve(recordedYears.count() + 1);
      options.emplace_back(tr(STR_STATS_ALL_TIME));
      int selected = 0;
      for (int n = 0; n < recordedYears.count(); ++n) {
        char label[12];
        const int year = recordedYears.at(n);
        std::snprintf(label, sizeof(label), "%d", year);
        options.emplace_back(label);
        if (statsScope == readingui::StatsScope::Year && year == readingstats::dateFromDay(annualStats.first).year)
          selected = n + 1;
      }
      goalPopup.show(StrId::STR_STATS_SCOPE_TITLE, std::move(options), selected, [this](int choice) {
        RenderLock lock;
        if (choice == 0) {
          statsScope = readingui::StatsScope::AllTime;
          ready = false;
          requestUpdate();
        } else if (choice > 0 && choice <= recordedYears.count())
          loadAnnual(recordedYears.at(choice - 1));
      });
      requestUpdate();
      return;
    }
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
      if (source == readingstats::Source::Unified)
        loading = cloudAvailable && combined->project(dayStats, &cloudDayKnown) ? Load::Ready : Load::Failed;
      else
        loading = beginScan() ? Load::Day : Load::Failed;
      requestUpdate();
      return;
    case readingui::Tab:
      if (value < 0 || value > 3 || !summaryReady) return;
      if (value == tab && !details && !pickerVisible && !goalCalendarVisible &&
          period == readingstats::Period::Calendar)
        return;
      detailPage = DetailPage::Summary;
      periodBooksVisible = false;
      goalCalendarVisible = false;
      repository.close();
      loading = Load::Ready;
      details = false;
      pickerVisible = false;
      periodFromPicker = false;
      tab = value;
      period = readingstats::Period::Calendar;
      if (tab == 1 && statsScope == readingui::StatsScope::Year && !annualReady) {
        loadAnnual(readingstats::dateFromDay(annualStats.first).year);
        return;
      }
      if (tab == 3)
        loadPage(0);
      else {
        if (tab == 0 && clockKnown && !dayReady) {
          readingstats::setPeriod(dayStats, readingstats::Period::Day, displayedDay, SETTINGS.weekStartsSunday);
          if (source == readingstats::Source::Unified)
            loading = cloudAvailable && combined->project(dayStats, &cloudDayKnown) ? Load::Ready : Load::Failed;
          else
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
      } else if (tab == 3 && summaryReady) {
        const int pages = std::max(1, static_cast<int>((bookTotal() + pageSize - 1) / pageSize));
        const int next = std::clamp(page + value, 0, pages - 1);
        if (next != page) loadPage(next);
      }
      return;
    case readingui::OpenBook:
      detailReset = false;
      detailPage = DetailPage::Summary;
      if (value < 0 || value >= (periodBooksVisible ? periodBooks.count : count)) return;
      if (periodBooksVisible) {
        detail = finishedBooks[value];
        details = true;
        ready = false;
        loading = beginScan(true) ? Load::Detail : Load::Failed;
        requestUpdate();
        return;
      }
      detail = source == readingstats::Source::Unified      ? finishedBooks[value]
               : filter == readingstats::BookFilter::Recent ? books[value + page * pageSize]
                                                            : finishedBooks[value];
      details = true;
      ready = false;
      loading = source == readingstats::Source::Unified ? Load::Ready : beginScan(true) ? Load::Detail : Load::Failed;
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
void ReadingStatsActivity::resetSelectedBook() {
  periodView.close();
  daysView.close();
  repository.close();
  const bool saved = repository.resetBook(detail.document);
  repository.close();
  RenderLock lock(*this);
  resetState = ResetState::Idle;
  if (!saved) {
    const char* options[] = {tr(STR_DONE)};
    goalPopup.showMessage(tr(STR_STATS_RESET_BOOK), tr(STR_TOOLS_SAVE_FAILED), options, 1, 0,
                          [this](int) { requestUpdate(); });
    requestUpdate();
    return;
  }
  record.book = detail;
  detail = {};
  readingstats::copyText(detail.document, sizeof(detail.document), record.book.document);
  readingstats::copyText(detail.title, sizeof(detail.title), record.book.title);
  readingstats::copyText(detail.author, sizeof(detail.author), record.book.author);
  detailReset = true;
  detailPage = DetailPage::Summary;
  periodBooksVisible = periodFromPicker = false;
  completionIndex.reset();
  completionMemory.reset();
  summary = {};
  summary.anchorDay = currentDay;
  readingstats::setPeriod(dayStats, readingstats::Period::Day, displayedDay, SETTINGS.weekStartsSunday);
  readingstats::setPeriod(annualStats, readingstats::Period::Year, currentDay, SETTINGS.weekStartsSunday);
  listedFinished = 0;
  recordedYears = {};
  recentCount = count = page = 0;
  finishedPage = -1;
  filter = readingstats::BookFilter::Recent;
  tab = 3;
  summaryReady = dayReady = annualReady = ready = false;
  loading = beginScan(currentDay == 0) ? (currentDay ? Load::Summary : Load::Anchor) : Load::Failed;
  requestUpdate();
}
void ReadingStatsActivity::loop() {
  if (resetState == ResetState::Requested) {
    {
      RenderLock lock(*this);
      resetState = ResetState::Working;
    }
    requestUpdateAndWait();
    resetSelectedBook();
    return;
  }
  if (goalSaveFailed && !goalPopup.isActive()) {
    goalSaveFailed = false;
    const char* options[] = {tr(STR_DONE)};
    goalPopup.showMessage(tr(STR_GOAL_DAILY), tr(STR_TOOLS_SAVE_FAILED), options, 1, 0,
                          [this](int) { requestUpdate(); });
    requestUpdate();
    return;
  }
  if (goalPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("STATS", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (loading == Load::Cloud) {
    lock.unlock();
    requestUpdateAndWait();
    loadCloud();
    return;
  }
  if (!*initialBook && readingstats::cloud::Client::configured() &&
      mappedInput.wasLongPressed(MappedInputManager::Button::Right, 700)) {
    importHistory();
    return;
  }
  sectionnav::Input::Event command;
  bool navigated = false;
  while (navigation.pop(command)) {
    if (command.kind == sectionnav::Input::Kind::Back) {
      navigation.clear();
      back();
      return;
    }
    if (goalCalendarVisible) {
      if (command.kind == sectionnav::Input::Kind::Step || command.kind == sectionnav::Input::Kind::Section)
        action(readingui::GoalMonth, command.amount);
      continue;
    }
    if (detailPage == DetailPage::Days) {
      if (ready && loading == Load::Ready &&
          (command.kind == sectionnav::Input::Kind::Step || command.kind == sectionnav::Input::Kind::Section)) {
        action(readingui::BookDaysPage, command.amount);
        navigated = true;
      }
      continue;
    }
    if (periodBooksVisible && !details) {
      if (ready && loading == Load::Ready) {
        if (command.kind == sectionnav::Input::Kind::Confirm)
          action(readingui::OpenBook, selected);
        else if (command.kind == sectionnav::Input::Kind::Step || command.kind == sectionnav::Input::Kind::Section)
          action(readingui::PeriodBooksPage, command.amount);
        navigated = true;
      }
      continue;
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
  if (loading == Load::Anchor || loading == Load::Summary) {
    scan();
    if (loading == Load::Ready) {
      const int next = queuedTab;
      queuedTab = -1;
      if (next >= 0 && next != tab)
        action(readingui::Tab, next);
      else if (tab == 3 && !details)
        loadPage(0);
    }
    return;
  }
  if (detailPage == DetailPage::Days &&
      bookDays.capacity != (renderer.getScreenWidth() > renderer.getScreenHeight() ? 2 : readingstats::kBooksPerPage)) {
    loadBookDays();
    return;
  }
  if ((tab == 3 || periodBooksVisible) && !details) {
    auto target = toybox::makeTarget(renderer);
    const int capacity = periodBooksVisible && renderer.getScreenWidth() > renderer.getScreenHeight()
                             ? 2
                             : readingui::capacity(target.deviceContext());
    if (pageSize != capacity) {
      const int first = page * pageSize;
      pageSize = capacity;
      if (periodBooksVisible)
        loadPeriodBooks();
      else
        loadPage(first / pageSize);
      return;
    }
  }
  if (loading != Load::Ready && loading != Load::Failed) {
    scan();
    return;
  }
  if (loading == Load::Failed) {
    int x = 0, y = 0;
    if (ready && mappedInput.wasScreenTapped(x, y)) {
      fui::InputSnapshot input;
      input.touchReleased = true;
      input.touchX = x;
      input.touchY = y;
      if (interactions.route(input).action == readingui::ImportHistory) importHistory();
    }
    return;
  }
  if (detailPage == DetailPage::Days) {
    if (!ready) return;
    if (const int direction = sidebuttons::horizontalStep(mappedInput)) {
      action(readingui::BookDaysPage, direction);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
        mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      action(readingui::BookDaysPage, -1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      action(readingui::BookDaysPage, 1);
      return;
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      fui::InputSnapshot input{};
      input.touchReleased = true;
      input.touchX = x;
      input.touchY = y;
      const auto event = interactions.route(input);
      action(event.action, event.value);
    }
    return;
  }
  if (ready && !details) {
    if (const int direction = sidebuttons::horizontalStep(mappedInput)) {
      if (periodBooksVisible)
        action(readingui::PeriodBooksPage, direction);
      else if (goalCalendarVisible)
        action(readingui::GoalMonth, direction);
      else if (pickerVisible)
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
    action(goalCalendarVisible ? readingui::GoalMonth : readingui::Tab, goalCalendarVisible ? -1 : (tab + 3) % 4);
    return;
  }
  if (!*initialBook && mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    action(goalCalendarVisible ? readingui::GoalMonth : readingui::Tab, goalCalendarVisible ? 1 : (tab + 1) % 4);
    return;
  }
  if (detailPage == DetailPage::Days &&
      bookDays.capacity != (renderer.getScreenWidth() > renderer.getScreenHeight() ? 2 : readingstats::kBooksPerPage)) {
    loadBookDays();
    return;
  }
  if ((tab == 3 || periodBooksVisible) && !details) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      if (selected + 1 < (periodBooksVisible ? periodBooks.count : count)) {
        ++selected;
        requestUpdate();
      } else
        action(periodBooksVisible ? readingui::PeriodBooksPage : readingui::Page, 1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      if (selected > 0) {
        --selected;
        requestUpdate();
      } else
        action(periodBooksVisible ? readingui::PeriodBooksPage : readingui::Page, -1);
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
  for (const auto& b : finishedBooks)
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
  model.annualStats = &annualStats;
  model.statsScope = statsScope;
  model.annualKnown = annualKnown;
  model.annualFinishedKnown = annualFinishedKnown;
  model.periodStats = tab == 0 ? &dayStats : &periodStats;
  model.period = period;
  model.filter = filter;
  model.bookTotal = bookTotal();
  model.weekStartsSunday = SETTINGS.weekStartsSunday;
  model.books = source == readingstats::Source::Unified      ? finishedBooks
                : filter == readingstats::BookFilter::Recent ? books + page * pageSize
                                                             : finishedBooks;
  if (periodBooksVisible) model.books = finishedBooks;
  model.periodBooks = periodBooksVisible;
  model.periodBookFirst = periodBooks.first;
  model.periodBookLast = periodBooks.last;
  model.periodBookPage = periodBooks.page;
  model.periodBookCount = periodBooks.count;
  model.periodBookTotal = periodView.count();
  model.periodBookNext = periodBooks.hasNext;
  model.bookDays = detailPage == DetailPage::Days ? &bookDays : nullptr;
  model.detail = details && (detailReset || detail.visibleInBookHistory()) ? &detail : nullptr;
  model.canResetBook = details && detailPage == DetailPage::Summary && source == readingstats::Source::Local &&
                       loading == Load::Ready && readingstats::validDocument(detail.document);
  model.resetting = resetState == ResetState::Working;
  model.bookOnly = *initialBook;
  model.periodFromPicker = periodFromPicker;
  model.pickerVisible = pickerVisible;
  model.pickerYear = pickerYear;
  model.today = tab == 0 && !clockKnown ? 0 : currentDay;
  model.month = goalCalendarVisible ? goalMonth : month;
  model.goalCalendarVisible = goalCalendarVisible;
  model.goalMinutes = readinggoal::target(SETTINGS.readingDailyGoal);
  model.goalStartDay = SETTINGS.readingGoalStartDay;
  model.goalLegacyHistory = SETTINGS.readingGoalLegacyHistory;
  model.tab = tab;
  model.page = page;
  model.count = count;
  model.selected = selected;
  model.pageSize = pageSize;
  model.loading = (loading != Load::Ready && loading != Load::Failed) ||
                  (tab == 3 && !details && pageSize != readingui::capacity(target.deviceContext()));
  model.failed = loading == Load::Failed;
  model.damaged = damaged;
  model.source = source;
  model.syncAvailable = readingstats::crossink::installed() || readingstats::cloud::Client::configured();
  model.importedAvailable = cloudAvailable;
  model.importedActive = cloudAvailable ? combined->data().active : nullptr;
  model.importedAnchor = cloudAvailable ? combined->data().summary.anchorDay : 0;
  model.dailyKnown = tab == 0 ? cloudDayKnown : cloudPeriodKnown;
  readingui::build(screen, model, uiWork);
  if (goalPopup.isActive()) goalPopup.render(renderer);
  ready = true;
  toybox::reportOverflow(interactions, "ReadingStats");
  renderer.displayBuffer();
}

int ReadingStatsActivity::bookTotal() const {
  if (source == readingstats::Source::Unified) {
    if (!cloudAvailable) return 0;
    return filter == readingstats::BookFilter::Finished ? listedCloudFinished : listedCloudRecent;
  }
  return filter == readingstats::BookFilter::Recent ? recentCount : listedFinished;
}
void ReadingStatsActivity::importHistory() {
  if (!readingstats::crossink::installed()) {
    syncHistory();
    return;
  }
  const char* labels[] = {tr(STR_CROSSINK_LOCAL), tr(STR_CROSSINK_HISTORY), tr(STR_READING_SYNC_IMPORT)};
  const bool network = readingstats::cloud::Client::configured();
  goalPopup.showMessage(tr(STR_CROSSINK_SOURCE), tr(STR_CROSSINK_SOURCES_HELP), labels, network ? 3 : 2, 1,
                        [this](int selected) {
                          if (selected == 2) {
                            syncHistory();
                            return;
                          }
                          auto next = makeUniqueNoThrow<ReadingStatsActivity>(renderer, mappedInput);
                          if (next) {
                            next->useHistory(selected == 0 ? History::Local : History::CrossInk);
                            activityManager.replaceActivity(std::move(next));
                          } else
                            LOG_ERR("STATS", "OOM: history source");
                        });
  requestUpdate();
}
void ReadingStatsActivity::syncHistory() {
  if (*initialBook || !readingstats::cloud::Client::configured()) return;
  auto next = makeUniqueNoThrow<ReadingStatsSyncActivity>(renderer, mappedInput);
  if (!next) {
    LOG_ERR("STATS", "OOM: import activity");
    return;
  }
  repository.close();
  cloudAvailable = false;
  if (combined) combined.reset();
  if (cloud) cloud->close();
  startActivityForResult(std::move(next), [this](const ActivityResult& result) {
    (void)result;
    auto replacement = makeUniqueNoThrow<ReadingStatsActivity>(renderer, mappedInput);
    if (replacement) {
      replacement->useHistory(History::Cloud);
      activityManager.replaceActivity(std::move(replacement));
    } else {
      LOG_ERR("STATS", "OOM: imported history");
      activityManager.goHome(HomeMenuItem::READING_STATS);
    }
  });
}
void ReadingStatsActivity::loadCloud() {
  const bool crossink =
      history == History::CrossInk || (history == History::Automatic && readingstats::crossink::installed());
  const auto status =
      cloud->open(crossink ? readingstats::cloud::SnapshotKind::CrossInk : readingstats::cloud::SnapshotKind::Cloud);
  const bool available = status == readingstats::cloud::Result::Success &&
                         (crossink || cloud->data().account == readingstats::cloud::Client::accountId());
  auto projected = readingstats::cloud::Result::Success;
  if (available) {
    combined = makeUniqueNoThrow<readingstats::cloud::Projection>();
    if (combined)
      projected = combined->open(cloud.get(), currentDay, crossink ? 1 : readingstats::cloud::Client::deviceTag());
    else {
      LOG_ERR("STATS", "OOM: combined projection");
      projected = readingstats::cloud::Result::LowMemory;
    }
  }
  RenderLock lock;
  cloudAvailable = available && projected == readingstats::cloud::Result::Success;
  if (!available) {
    cloud->close();
    source = readingstats::Source::Local;
    if (status == readingstats::cloud::Result::LowMemory || status == readingstats::cloud::Result::NewerCache) {
      loading = Load::Failed;
      ready = false;
      requestUpdate();
      return;
    }
    loading = beginScan(currentDay == 0) ? (currentDay ? Load::Summary : Load::Anchor) : Load::Failed;
    ready = false;
    requestUpdate();
    return;
  }
  if (projected != readingstats::cloud::Result::Success) {
    loading = Load::Failed;
    ready = false;
    requestUpdate();
    return;
  }
  source = readingstats::Source::Unified;
  summary = combined->data().summary;
  recordedYears = combined->recordedYears();
  if (!clockKnown) {
    currentDay = summary.anchorDay;
    displayedDay = currentDay;
    const auto date = readingstats::dateFromDay(currentDay);
    month = date.year * 12 + date.month - 1;
    pickerYear = date.year;
  }
  if (clockKnown) {
    readingstats::setPeriod(dayStats, readingstats::Period::Day, displayedDay, SETTINGS.weekStartsSunday);
    if (!combined->project(dayStats, &cloudDayKnown)) {
      loading = Load::Failed;
      requestUpdate();
      return;
    }
  }
  readingstats::setPeriod(annualStats, readingstats::Period::Year, currentDay, SETTINGS.weekStartsSunday);
  if (!combined->projectAnnual(annualStats, annualKnown, annualFinishedKnown)) {
    loading = Load::Failed;
    requestUpdate();
    return;
  }
  annualReady = true;
  summaryReady = true;
  dayReady = true;
  loading = Load::Ready;
  ready = false;
  requestUpdate();
}
