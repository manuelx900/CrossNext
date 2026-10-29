#include "ReaderActivity.h"

#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <I18n.h>
#include <KOReaderDocumentId.h>
#include <Memory.h>
#include <TrustedTime.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../../apps_local/reading/ReadingStatsStore.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "EpubReaderActivity.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "SdCardFontSystem.h"
#include "XtcReaderActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "util/PluginEvents.h"

ReaderActivity::ReaderActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput,
                               std::string bookPath, const bool allowFastInitialRefresh)
    : Activity(name, renderer, mappedInput), bookPath(std::move(bookPath)) {
  setInterfaceFontSection(menufont::Section::Reader);
  if (allowFastInitialRefresh) {
    const int refreshFrequency = SETTINGS.getRefreshFrequency();
    pagesUntilFullRefresh = refreshFrequency > 1 ? refreshFrequency : 2;
  }
}

std::unique_ptr<ReaderActivity> ReaderActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       std::string path, const bool allowFastInitialRefresh) {
  // ActivityManager requires heap ownership; each branch allocates exactly one screen-lifetime object.
  std::unique_ptr<ReaderActivity> activity;
  if (FsHelpers::hasXtcExtension(path)) {
    activity = makeUniqueNoThrow<XtcReaderActivity>(renderer, mappedInput, std::move(path), allowFastInitialRefresh);
  } else {
    activity = makeUniqueNoThrow<EpubReaderActivity>(renderer, mappedInput, std::move(path), allowFastInitialRefresh);
  }

  if (!activity) {
    LOG_ERR("READER", "OOM: reader activity");
  }
  return activity;
}

void ReaderActivity::applyInitialOrientation() { ReaderUtils::applyOrientation(renderer, SETTINGS.orientation); }

void ReaderActivity::disableFastInitialRefresh() { pagesUntilFullRefresh = 0; }

void ReaderActivity::notePageTurn(const bool forward, const bool succeeded) {
  RenderLock lock(*this);
  readerSession.noteTurn(forward, succeeded);
}

void ReaderActivity::onEnter() {
  Activity::onEnter();

  // Heap ledger for field crash reports: free vs largest block distinguishes a
  // leak (free falls) from fragmentation (free stable, largest collapses).
  LOG_INF("MEM", "reader enter: free=%u max_block=%u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  if (!Storage.exists(bookPath.c_str())) {
    LOG_ERR("READER", "File does not exist: %s", bookPath.c_str());
    finish();
    return;
  }

  // Clear remembered book after opening it
  if (!APP_STATE.openEpubPath.empty()) {
    APP_STATE.openEpubPath.clear();
    APP_STATE.saveToFile();
  }

  sdFontSystem.ensureLoaded(renderer);
  applyInitialOrientation();

  if (!loadBook()) {
    if (!handleLoadFailure()) finish();
    return;
  }
  const auto document = KOReaderDocumentId::calculate(bookPath);
  bool completed = false, manuallyUnfinished = false;
  if (readingstats::validDocument(document.c_str()) &&
      readingstats::completionStatus(document.c_str(), completed, manuallyUnfinished))
    bookFinished.store(completed, std::memory_order_release);

  requestUpdate();
}

void ReaderActivity::rememberBookOnceRendered() {
  if (bookRemembered || !pageRendered.load(std::memory_order_acquire)) return;
  bookRemembered = true;
  APP_STATE.openEpubPath = bookPath;
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(bookPath, getBookTitle(), getBookAuthor(), getBookThumbBmpPath());
  const pluginevents::Var openVars[] = {{"book", bookPath.c_str()}};
  pluginevents::emit(pluginevents::Event::ReaderOpen, openVars, 1);
}

void ReaderActivity::onExit() {
  Activity::onExit();

  // Keep rebuildable font buffers from pinning the heap between reading sessions.
  if (auto* fontCache = renderer.getFontCacheManager()) {
    fontCache->releaseSdFontCaches();
  }

  LOG_INF("MEM", "reader exit: free=%u max_block=%u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  // Flush BEFORE the ReaderExit event: the session's final progress must be
  // durable before a subscriber can act on the exit notification.
  flushReaderSession();

  if (pluginevents::anySubscriber(pluginevents::Event::ReaderExit)) {
    char percent[8];
    snprintf(percent, sizeof(percent), "%d", getScreenshotInfo().progressPercent);
    const pluginevents::Var vars[] = {{"book", bookPath.c_str()}, {"percent", percent}};
    pluginevents::emit(pluginevents::Event::ReaderExit, vars, 2);
  }

  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();

  endOfBookOptions.reset();
  endOfBookOptionsReady.store(false, std::memory_order_release);
}

void ReaderActivity::prepareForSleep() { flushReaderSession(); }

bool ReaderActivity::flushReaderSession() {
  if (!readerSession.isEmitWorthy()) {
    readerSession.reset();
    return true;
  }

  const std::string document = KOReaderDocumentId::calculate(bookPath);
  const bool validDocument =
      document.size() == 32 && std::all_of(document.begin(), document.end(), [](const unsigned char c) {
        return std::isdigit(c) || (c >= 'a' && c <= 'f');
      });
  bool saved = true;
  if (validDocument) {
    if (readingstats::enabledFormat(bookPath.c_str(), SETTINGS.readingFormats)) {
      bool completed = isBookFinished();
      saved = readingstats::recordSession(document.c_str(), getBookTitle().c_str(), getBookAuthor().c_str(),
                                          readerSession.startTime(), readerSession.durationSeconds(),
                                          readerSession.pagesRead(), readerSession.endProgressBp(), &completed);
      if (saved) bookFinished.store(completed, std::memory_order_release);
    }
    if (!pluginevents::anySubscriber(pluginevents::Event::ReaderSession)) {
      readerSession.reset();
      return saved;
    }
    char startTime[24];
    char endTime[24];
    char duration[16];
    char startProgress[8];
    char endProgress[8];
    snprintf(startTime, sizeof(startTime), "%lld", static_cast<long long>(readerSession.startTime()));
    snprintf(endTime, sizeof(endTime), "%lld", static_cast<long long>(readerSession.endTime()));
    snprintf(duration, sizeof(duration), "%lu", static_cast<unsigned long>(readerSession.durationSeconds()));
    snprintf(startProgress, sizeof(startProgress), "%u", readerSession.startProgressBp());
    snprintf(endProgress, sizeof(endProgress), "%u", readerSession.endProgressBp());
    const pluginevents::Var vars[] = {{"book", bookPath.c_str()},       {"document", document.c_str()},
                                      {"start_time", startTime},        {"end_time", endTime},
                                      {"duration_seconds", duration},   {"start_progress_bp", startProgress},
                                      {"end_progress_bp", endProgress}, {"progress_scale", "10000"}};
    pluginevents::emit(pluginevents::Event::ReaderSession, vars, 8);
  }
  // Verification can fail after a complete append. Never replay this
  // best-effort session on another flush and count the same reading twice.
  readerSession.reset();
  return validDocument && saved;
}

void ReaderActivity::showFinishBookError(const char* message) {
  auto dialog =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_READER_FINISH_BOOK), message, true);
  if (!dialog) {
    LOG_ERR("READER", "OOM: completion error dialog");
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(dialog), [this](const ActivityResult&) { requestUpdate(); });
}

void ReaderActivity::requestFinishBook() {
  // The activity stack owns the modal; cancel leaves the reading position untouched.
  const bool reopen = isBookFinished() && !finishedPositionPending;
  auto dialog = makeUniqueNoThrow<ConfirmationActivity>(
      renderer, mappedInput, reopen ? tr(STR_READER_MARK_UNFINISHED) : tr(STR_READER_FINISH_BOOK),
      reopen ? tr(STR_READER_UNFINISH_CONFIRM) : tr(STR_READER_FINISH_CONFIRM));
  if (!dialog) {
    LOG_ERR("READER", "OOM: finish-book confirmation");
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(dialog), [this, reopen](const ActivityResult& result) {
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    const char* error = nullptr;
    {
      RenderLock lock(*this);
      const auto now = trustedtime::trustedNow();
      const auto document = KOReaderDocumentId::calculate(bookPath);
      if (!now)
        error = tr(STR_READER_FINISH_CLOCK);
      else if (!readingstats::validDocument(document.c_str()) || !flushReaderSession())
        error = tr(STR_READER_FINISH_FAILED);
      else {
        const bool saved = reopen ? readingstats::recordReopening(
                                        document.c_str(), getBookTitle().c_str(), getBookAuthor().c_str(), now,
                                        static_cast<uint16_t>(std::clamp(getProgressBasisPoints(), 0, 9999)))
                                  : readingstats::recordCompletion(document.c_str(), getBookTitle().c_str(),
                                                                   getBookAuthor().c_str(), now);
        if (!saved)
          error = tr(STR_READER_FINISH_FAILED);
        else {
          bookFinished.store(!reopen, std::memory_order_release);
          finishedPositionPending = !reopen && !saveFinishedPosition();
          if (finishedPositionPending) error = tr(STR_READER_FINISH_POSITION_FAILED);
        }
      }
    }
    if (error)
      showFinishBookError(error);
    else
      requestUpdate();
  });
}

void ReaderActivity::recordEndOfBook() {
  if (!endRenderedPending.exchange(false, std::memory_order_acquire)) return;
  RenderLock lock(*this);
  if (!readingstats::enabledFormat(bookPath.c_str(), SETTINGS.readingFormats)) return;
  if (!isAtEndOfBook()) {
    endCompletionAttempted = false;
    return;
  }
  if (endCompletionAttempted || isBookFinished()) return;
  endCompletionAttempted = true;
  const auto now = trustedtime::trustedNow();
  const auto document = KOReaderDocumentId::calculate(bookPath);
  if (now && readingstats::validDocument(document.c_str()) && flushReaderSession() &&
      readingstats::recordCompletion(document.c_str(), getBookTitle().c_str(), getBookAuthor().c_str(), now))
    bookFinished.store(true, std::memory_order_release);
  else
    LOG_ERR("READER", "Could not record end-of-book completion");
}

bool ReaderActivity::handleBackNavigation() {
  return ReaderUtils::handleBackNavigation(mappedInput, activityManager, bookPath.c_str(),
                                           {this, [](void* ctx) { static_cast<ReaderActivity*>(ctx)->onGoHome(); }});
}

void ReaderActivity::clearEndOfBookOptionsIfNeeded() {
  if (isAtEndOfBook() || !endOfBookOptionsReady.load(std::memory_order_acquire)) return;

  RenderLock lock(*this);
  endCompletionAttempted = false;
  endOfBookOptionsReady.store(false, std::memory_order_release);
  endOfBookOptions.reset();
}

bool ReaderActivity::endOfBookMenuActive() const {
  return isAtEndOfBook() && endOfBookOptionsReady.load(std::memory_order_acquire) && endOfBookOptions->menuActive();
}

bool ReaderActivity::handleEndOfBookMenu(const bool suppressConfirmRelease) {
  if (suppressConfirmRelease || !endOfBookMenuActive()) {
    return false;
  }

  switch (endOfBookOptions->handleMenuInput(mappedInput)) {
    case EndOfBookOptions::Action::GoHome:
      onGoHome();
      return true;
    case EndOfBookOptions::Action::LastPage:
      onReturnFromEndOfBook();
      requestUpdate();
      return true;
    case EndOfBookOptions::Action::Redraw:
      requestUpdate();
      return true;
    case EndOfBookOptions::Action::None:
      return false;
  }

  return false;
}

bool ReaderActivity::handleEndOfBookPageTurn(const bool prevTriggered, const bool nextTriggered) {
  if (!isAtEndOfBook()) return false;

  if (endOfBookOptionsReady.load(std::memory_order_acquire) && endOfBookOptions->menuActive()) {
    return true;
  }
  if (nextTriggered) {
    onGoHome();
  } else if (prevTriggered) {
    onReturnFromEndOfBook();
    requestUpdate();
  }
  return true;
}

void ReaderActivity::loop() {
  rememberBookOnceRendered();
  if (pageRendered.load(std::memory_order_acquire)) recordEndOfBook();
  clearEndOfBookOptionsIfNeeded();
  if (handleEndOfBookMenu()) return;
  if (handleFormatInput()) return;
  if (handleBackNavigation()) return;

  const auto touch = ReaderUtils::detectTouchPageTurn(
      renderer, mappedInput, false, SETTINGS.xtcInvertPageDirection && FsHelpers::hasXtcExtension(bookPath));
  auto [prevTriggered, nextTriggered, fromTilt] = ReaderUtils::detectPageTurn(mappedInput);
  prevTriggered = prevTriggered || touch.prev;
  nextTriggered = nextTriggered || touch.next;
  if (!prevTriggered && !nextTriggered) return;
  if (handleEndOfBookPageTurn(prevTriggered, nextTriggered)) return;

  const unsigned long heldMs = (touch.prev || touch.next) ? touch.heldMs : mappedInput.getHeldTime();
  const bool skip =
      !fromTilt && SETTINGS.longPressButtonBehavior == SETTINGS.CHAPTER_SKIP && heldMs >= ReaderUtils::SKIP_HOLD_MS;

  const bool changed = skip ? skipPages(prevTriggered ? -10 : 10) : pageTurn(!prevTriggered);
  // A skip is navigation, not reading: it never counts toward session dwell.
  notePageTurn(!skip && !prevTriggered, changed);
  if (changed && (touch.prev || touch.next)) haptic_feedback::touchAction(skip);
  requestUpdate();
}

void ReaderActivity::render(RenderLock&&) {
  if (isAtEndOfBook()) {
    if (!endOfBookOptions) {
      endOfBookOptions = makeUniqueNoThrow<EndOfBookOptions>(renderer);
      if (!endOfBookOptions) LOG_ERR("READER", "OOM: EndOfBookOptions");
    }
    renderer.clearScreen();
    if (endOfBookOptions) {
      endOfBookOptions->loadOnce();
      // Release-publish AFTER loadOnce() so the main task's acquire load can't
      // observe an object whose screen and input handlers are still being set up.
      endOfBookOptionsReady.store(true, std::memory_order_release);
      endOfBookOptions->render();
    }
    renderer.displayBuffer();
    onEndOfBookRendered();
    markPageRendered();
    readerSession.onRenderComplete(millis(), trustedtime::trustedNow(), getProgressBasisPoints());
    endRenderedPending.store(true, std::memory_order_release);
    return;
  }

  renderBook();
  readerSession.onRenderComplete(millis(), trustedtime::trustedNow(), getProgressBasisPoints());
}

bool ReaderActivity::handleForcedRefresh() {
  {
    RenderLock lock(*this);
    pagesUntilFullRefresh = 1;
    forcedRefreshPending = true;
  }
  requestUpdate();
  return true;
}
