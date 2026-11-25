#include "KOReaderSyncActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <TrustedTime.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cassert>

#include "DevMode.h"
#include "Epub/Section.h"
#include "EpubReaderUtils.h"
#include "KOReaderCredentialStore.h"
#include "KOReaderDocumentId.h"
#include "MappedInputManager.h"
#include "ProgressComparison.h"
#include "ReaderUtils.h"
#include "SilentRestart.h"
#include "activities/ActivityManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "apps_local/bookorbit/BookOrbit.h"
#include "apps_local/bookorbit/BookOrbitPending.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "clippings/ClippingSync.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"  // list icons for the compare rows
#include "fontIds.h"
#include "network/WifiPowerSaveGuard.h"

namespace fui = freeink::ui;

namespace {
// One action id for both interactive states: the compare rows (SHOWING_RESULT)
// and the upload button (NO_REMOTE_PROGRESS) never coexist, so state
// disambiguates them in the handler.
constexpr fui::ActionId ACTION_ROW = 1;
constexpr fui::ActionId ACTION_BATCH_CARD = 2;
constexpr int SKIP_BOOK = 2;

void styleBookOrbitActions(fui::ListProps& props) {
  props.rowStyles = pixelui::buttonStyles();
  props.rowRadius = lightui::radius();
  props.rowGap = 8;
  props.selectionMarker = fui::SelectionMarker::None;
}

bool isBookOrbitReset(const KOReaderProgress& progress) {
  return progress.deviceId == "bookorbit-web" && progress.percentage == 0 &&
         progress.progress == "/body/DocFragment[1]/body";
}

bool sameRemoteProgress(const KOReaderProgress& current, const KOReaderProgress& previous) {
  return current.timestamp == previous.timestamp && current.deviceId == previous.deviceId &&
         current.progress == previous.progress && current.percentage == previous.percentage;
}

std::string calculateDocumentHashForMethod(const std::string& path, const DocumentMatchMethod method) {
  return method == DocumentMatchMethod::FILENAME ? KOReaderDocumentId::calculateFromFilename(path)
                                                 : KOReaderDocumentId::calculate(path);
}

DocumentMatchMethod alternateMatchMethod(const DocumentMatchMethod method) {
  return method == DocumentMatchMethod::FILENAME ? DocumentMatchMethod::BINARY : DocumentMatchMethod::FILENAME;
}

const char* matchMethodName(const DocumentMatchMethod method) {
  return method == DocumentMatchMethod::FILENAME ? "filename" : "binary";
}

}  // namespace

KOReaderSyncActivity::KOReaderSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::string& epubPath, CrossPointPosition localPosition,
                                           SavedProgressPosition localKoPos, std::string localChapterName,
                                           bool bookOrbit, std::optional<ReaderRenderSpec> bookOrbitRenderSpec)
    : Activity("KOReaderSync", renderer, mappedInput),
      UiAppHost(renderer),
      epubPath(epubPath),
      localChapterName(std::move(localChapterName)),
      localPosition(localPosition),
      bookOrbit(bookOrbit),
      bookOrbitRenderSpec(bookOrbitRenderSpec),
      remoteProgress{},
      remotePosition{},
      localProgress(std::move(localKoPos)) {}

KOReaderSyncActivity::KOReaderSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::string& epubPath)
    : KOReaderSyncActivity(renderer, mappedInput, epubPath, {}, {}, {}, true) {
  syncSavedBook = true;
}

KOReaderSyncActivity::KOReaderSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           std::vector<bookorbit::BatchBook> recentBooks)
    : KOReaderSyncActivity(renderer, mappedInput, std::string{}) {
  batchBooks = std::move(recentBooks);
}

KOReaderSyncActivity::SavedPositionResult KOReaderSyncActivity::loadSavedBookPosition() {
  ensureEpubLoaded();
  if (!epub) return SavedPositionResult::Unavailable;
  uint8_t bytes[10];
  size_t size;
  {
    HalFile file;
    if (!Storage.openFileForRead("KOSync", epub->getCachePath() + "/progress.bin", file))
      return SavedPositionResult::Unavailable;
    const auto fileSize = file.fileSize();
    if (fileSize != 4 && fileSize != 6 && fileSize != 10) return SavedPositionResult::Unavailable;
    size = static_cast<size_t>(fileSize);
    if (file.read(bytes, size) != static_cast<int>(size)) return SavedPositionResult::ReadError;
  }

  const auto readLittleEndian16 = [&](size_t index) {
    return uint16_t(bytes[index] | (uint16_t(bytes[index + 1]) << 8));
  };
  const int spineCount = epub->getSpineItemsCount();
  const int spine = readLittleEndian16(0);
  if (spineCount <= 0 || spine > spineCount) return SavedPositionResult::Unavailable;

  const bool finished = spine == spineCount;
  localPosition.spineIndex = finished ? spineCount - 1 : spine;
  const auto savedPage = readLittleEndian16(2);
  localPosition.pageNumber = finished ? 1 : (savedPage == UINT16_MAX ? 0 : savedPage);
  localPosition.totalPages = finished ? 2 : (size >= 6 ? readLittleEndian16(4) : 0);
  localPosition.hasResolvedSpineIndex = true;
  if (size == 10 && !finished) {
    localPosition.visibleTextOffset = uint32_t(readLittleEndian16(6)) | (uint32_t(readLittleEndian16(8)) << 16);
    localPosition.hasVisibleTextOffset = true;
  }

  RenderLock lock(*this);
  if (localPosition.totalPages == 0) {
    Section section(epub, localPosition.spineIndex, renderer);
    localPosition.totalPages = section.getCachedPageCount().value_or(0);
  }
  if (localPosition.pageNumber >= localPosition.totalPages) return SavedPositionResult::Unavailable;
  localPosition.hasMappedPage = true;
  GfxRenderer::FrameBufferLoan loan(renderer);
  localProgress = ProgressMapper::toSavedProgress(epub, localPosition);
  if (finished) localProgress.percentage = 1.0f;
  const int toc = epub->getTocIndexForSpineIndex(localPosition.spineIndex);
  if (toc >= 0) localChapterName = epub->getTocItem(toc).title;
  return localProgress.xpath.empty() ? SavedPositionResult::Unavailable : SavedPositionResult::Ready;
}

void KOReaderSyncActivity::ensureEpubLoaded() {
  if (!epub) {
    LOG_DBG("KOSync", "Loading epub for progress mapping (heap: %u)", (unsigned)ESP.getFreeHeap());
    epub = std::make_shared<Epub>(epubPath, "/.crosspoint");
    epub->setupCacheDir();
    // Load metadata only (no CSS needed for progress mapping, don't rebuild if cache is missing).
    if (!epub->load(false, true)) {
      LOG_ERR("KOSync", "Failed to load epub for progress mapping");
      epub.reset();
      return;
    }
    LOG_DBG("KOSync", "Epub loaded (heap: %u)", (unsigned)ESP.getFreeHeap());
  }
}

void KOReaderSyncActivity::saveProgressAndReturn(int spineIndex, int page) {
  // epub is guaranteed non-null here: ensureEpubLoaded() was called in performSync() before
  // SHOWING_RESULT state is entered, and this method is only called from that state.
  assert(epub);
  std::optional<uint32_t> offset;
  if (remotePosition.hasVisibleTextOffset && remotePosition.spineIndex == spineIndex) {
    offset = remotePosition.visibleTextOffset;
  }
  const int total = remotePosition.totalPages;
  const int savedCount = syncSavedBook && total > page && total <= UINT16_MAX ? total : 0;
  if (!EpubReaderUtils::saveProgress(*epub, spineIndex, page, savedCount, offset)) {
    {
      RenderLock lock(*this);
      state = SYNC_FAILED;
      statusMessage = tr(STR_SAVE_PROGRESS_FAILED);
    }
    requestUpdate(true);
    return;
  }
  if (bookOrbit) {
    const bookorbit::Position synced{remoteProgress.progress, remoteProgress.percentage};
    // The server holds subsequent progress from this device until it sees the accepted reset.
    if (isBookOrbitReset(remoteProgress)) {
      WifiPowerSaveGuard psGuard;
      const auto endpoint = bookorbit::Store::get().endpoint();
      KOReaderProgress current{};
      if (KOReaderSyncClient::getProgress(documentHash, current, &endpoint) != KOReaderSyncClient::OK ||
          current.deviceId != remoteProgress.deviceId || current.percentage != 0 || current.progress != synced.anchor ||
          KOReaderSyncClient::updateProgress(current, &endpoint) != KOReaderSyncClient::OK) {
        RenderLock lock(*this);
        state = SYNC_FAILED;
        statusMessage = tr(STR_BOOKORBIT_RESET_RETRY);
        requestUpdate();
        return;
      }
    }
    bookorbit::writeBaseline(documentHash, bookOrbitIdentity, synced, synced, {spineIndex, page, offset});
    bookorbit::clearPending(epubPath);
  }
  if (batchMode()) {
    completeSync(true);
    return;
  }
  exitSync();
}

void KOReaderSyncActivity::exitSync() {
  if (syncSavedBook)
    finish();
  else
    activityManager.goToReader(epubPath);
}

bool KOReaderSyncActivity::smartSyncEnabled() const {
  return !bookOrbit && KOREADER_STORE.getSyncBehavior() == KOReaderSyncBehavior::SMART;
}

void KOReaderSyncActivity::markAutoReturn() {
  if (!batchMode()) autoReturnAt = millis() + AUTO_RETURN_DELAY_MS;
}

void KOReaderSyncActivity::completeSync(bool changed) {
  if (bookOrbit && !changed) bookorbit::clearPending(epubPath);
  {
    RenderLock lock(*this);
    state = batchMode() && !changed ? BATCH_SKIPPED : SYNC_COMPLETE;
  }
  markAutoReturn();
  requestUpdate(true);
}

void KOReaderSyncActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    LOG_DBG("KOSync", "WiFi connection failed, exiting");
    exitSync();
    return;
  }

  LOG_DBG("KOSync", "WiFi connected, starting sync");

  // Keep the station fully awake for the short sync transaction. The web server
  // does the same because ESP32 modem sleep can introduce multi-second network
  // stalls that surface as HTTP timeouts. WiFi is torn down when this activity exits.
  if (!syncSavedBook) {
    WiFi.setSleep(false);
    LOG_DBG("KOSync", "WiFi sleep disabled for sync");
  }

  {
    RenderLock lock(*this);
    state = SYNCING;
    statusMessage = tr(STR_CALC_HASH);
  }
  requestUpdate(true);

  // KOSync requests from CrossPoint do not include a client timestamp.
  if (!batchMode()) performSync();
}

void KOReaderSyncActivity::performSync() {
  WifiPowerSaveGuard psGuard;
  const DocumentMatchMethod primaryMethod = bookOrbit ? DocumentMatchMethod::BINARY : KOREADER_STORE.getMatchMethod();
  documentHash = calculateDocumentHashForMethod(epubPath, primaryMethod);
  if (documentHash.empty()) {
    {
      RenderLock lock(*this);
      state = SYNC_FAILED;
      statusMessage = tr(STR_HASH_FAILED);
    }
    requestUpdate(true);
    return;
  }
  const std::string primaryHash = documentHash;

  LOG_DBG("KOSync", "Document hash (%s): %s", matchMethodName(primaryMethod), documentHash.c_str());

  if (!bookOrbit && KOREADER_STORE.getSyncClippings()) {
    {
      RenderLock lock(*this);
      statusMessage = tr(STR_SYNCING_CLIPPINGS);
    }
    requestUpdateAndWait();
    if (!clippingSync::run(epubPath, documentHash)) {
      {
        RenderLock lock(*this);
        state = SYNC_FAILED;
        statusMessage = tr(STR_CLIPPING_SYNC_FAILED);
      }
      requestUpdate(true);
      return;
    }
  }

  {
    RenderLock lock(*this);
    statusMessage = tr(STR_FETCH_PROGRESS);
  }
  if (!batchMode()) requestUpdateAndWait();

  // Fetch remote progress. In smart mode, retain the alternate document-id
  // record until both records can be mapped after the Epub is reloaded.
  auto result = [this] {
    if (!bookOrbit) return KOReaderSyncClient::getProgress(documentHash, remoteProgress);
    const auto& account = bookorbit::Store::get();
    const auto endpoint = account.endpoint();
    if (!batchMode() || bookOrbitIdentity.status == bookorbit::IdentityStatus::Unavailable)
      bookOrbitIdentity = bookorbit::readServerIdentity(account);
    return KOReaderSyncClient::getProgress(documentHash, remoteProgress, &endpoint);
  }();
  LOG_DBG("KOSync", "Primary remote (%s): result=%d http=%d doc=%s local=%.6f remote=%.6f xpath=%s",
          matchMethodName(primaryMethod), result, KOReaderSyncClient::lastHttpCode, documentHash.c_str(),
          localProgress.percentage, remoteProgress.percentage, remoteProgress.progress.c_str());

  KOReaderProgress alternateProgress;
  bool hasAlternateProgress = false;
  if (smartSyncEnabled()) {
    const DocumentMatchMethod altMethod = alternateMatchMethod(primaryMethod);
    const std::string altHash = calculateDocumentHashForMethod(epubPath, altMethod);
    if (!altHash.empty() && altHash != documentHash) {
      KOReaderProgress altProgress;
      const auto altResult = KOReaderSyncClient::getProgress(altHash, altProgress);
      LOG_DBG("KOSync", "Alternate remote (%s): result=%d http=%d doc=%s local=%.6f remote=%.6f xpath=%s",
              matchMethodName(altMethod), altResult, KOReaderSyncClient::lastHttpCode, altHash.c_str(),
              localProgress.percentage, altProgress.percentage, altProgress.progress.c_str());

      if (altResult == KOReaderSyncClient::OK) {
        alternateProgress = std::move(altProgress);
        hasAlternateProgress = true;
      }
    }
  }

  if (result == KOReaderSyncClient::NOT_FOUND && hasAlternateProgress) {
    remoteProgress = std::move(alternateProgress);
    hasAlternateProgress = false;
    result = KOReaderSyncClient::OK;
  }

  if (result == KOReaderSyncClient::NOT_FOUND || result == KOReaderSyncClient::NO_STORED_PROGRESS) {
    if (smartSyncEnabled()) {
      LOG_DBG("KOSync", "Smart sync: no remote progress found for known document hashes; uploading local %.6f",
              localProgress.percentage);
      performUpload();
      return;
    }

    bookorbit::Baseline baseline;
    if (batchMode() && result == KOReaderSyncClient::NO_STORED_PROGRESS &&
        bookOrbitIdentity.status != bookorbit::IdentityStatus::Unavailable &&
        bookorbit::canUploadFirstPosition(bookorbit::readBaseline(documentHash, baseline),
                                          {localProgress.xpath, localProgress.percentage})) {
      const auto endpoint = bookorbit::Store::get().endpoint();
      KOReaderProgress current{};
      if (KOReaderSyncClient::getProgress(documentHash, current, &endpoint) == KOReaderSyncClient::NO_STORED_PROGRESS) {
        performUpload();
        return;
      }
    }

    // No remote progress - offer to upload
    {
      RenderLock lock(*this);
      if (batchMode()) {
        app.cancelTouchContact();
        resetUi();
      }
      state = NO_REMOTE_PROGRESS;
      hasRemoteProgress = false;
    }
    requestUpdate(true);
    return;
  }

  if (result != KOReaderSyncClient::OK) {
    {
      RenderLock lock(*this);
      state = SYNC_FAILED;
      statusMessage = KOReaderSyncClient::errorString(result);
    }
    requestUpdate(true);
    return;
  }

  // Epub was released before sync to free RAM for the TLS handshake — reload it now.
  hasRemoteProgress = true;
  ensureEpubLoaded();
  if (!epub) {
    {
      RenderLock lock(*this);
      state = SYNC_FAILED;
      statusMessage = "";
    }
    requestUpdate(true);
    return;
  }

  {
    RenderLock lock;
    GfxRenderer::FrameBufferLoan loan(renderer);

    const auto mapRemoteProgress = [&](const KOReaderProgress& progress) {
      // The standard KOReader progress XPath is the authoritative content anchor.
      // The CrossPoint server's existing rich page hints remain a legacy fallback.
      const SavedProgressPosition koPos = {progress.progress, progress.percentage};
      CrossPointPosition mapped =
          ProgressMapper::toCrossPoint(epub, koPos, renderer, localPosition.spineIndex, localPosition.totalPages);
      if (!mapped.hasVisibleTextOffset && progress.position.has_value()) {
        // toCrossPoint above already tried koPos.xpath; if the rich position carries the same XPath,
        // tell fromRichPosition to skip re-resolving it and use its page hints directly.
        const bool sameXPath = progress.position->xpath == progress.progress;
        if (const auto richMapped = ProgressMapper::fromRichPosition(epub, *progress.position, renderer, sameXPath)) {
          mapped = *richMapped;
        }
      }
      return mapped;
    };

    remotePosition = mapRemoteProgress(remoteProgress);
    if (hasAlternateProgress) {
      const CrossPointPosition alternatePosition = mapRemoteProgress(alternateProgress);
      if (selectRemoteRecord(remotePosition, remoteProgress.percentage, alternatePosition,
                             alternateProgress.percentage) == RemoteRecordChoice::Alternate) {
        remoteProgress = std::move(alternateProgress);
        remotePosition = alternatePosition;
        LOG_DBG("KOSync", "Selected alternate remote record after mapped-position comparison");
      } else {
        LOG_DBG("KOSync", "Kept primary remote record after mapped-position comparison");
      }
    }
  }

  const ProgressComparison comparison =
      compareProgress(localPosition, localProgress.percentage, remotePosition, remoteProgress.percentage);
  if (bookOrbit && (batchMode() || bookorbit::Store::get().smartSync) && localProgress.percentage >= 0 &&
      localProgress.percentage <= 1 && remoteProgress.percentage >= 0 && remoteProgress.percentage <= 1) {
    bookorbit::Baseline baseline;
    const bookorbit::Position local{localProgress.xpath, localProgress.percentage};
    const bookorbit::Position remote{remoteProgress.progress, remoteProgress.percentage};
    const bool hasBaseline = bookorbit::readBaseline(documentHash, baseline);
    const bool matchingBaseline =
        hasBaseline && bookorbit::matchesBaseline(baseline, bookOrbitIdentity, bookorbit::Store::get().url);
    const bool matchingBatchPositions = batchMode() && bookorbit::samePosition(local, remote);
    if (matchingBatchPositions && !hasBaseline && bookOrbitIdentity.status != bookorbit::IdentityStatus::Unavailable) {
      bookorbit::writeBaseline(documentHash, bookOrbitIdentity, local, remote);
      completeSync();
      return;
    }
    if (matchingBaseline) {
      bool landingKnown = true;
      if (localPosition.spineIndex == baseline.landing.spine) {
        std::optional<uint16_t> landingPage;
        if (baseline.landing.offset && bookOrbitRenderSpec) {
          RenderLock lock;
          Section section(epub, localPosition.spineIndex, renderer);
          if (section.loadSectionFile(*bookOrbitRenderSpec))
            landingPage = section.getPageForVisibleTextOffset(*baseline.landing.offset);
        }
        const bool exactOffset = baseline.landing.offset && localPosition.hasVisibleTextOffset &&
                                 *baseline.landing.offset == localPosition.visibleTextOffset;
        landingKnown = !baseline.landing.offset || landingPage.has_value() || exactOffset;
        if (exactOffset ||
            (landingKnown && localPosition.pageNumber == (landingPage ? *landingPage : baseline.landing.page)))
          baseline.local = local;
      }
      const auto decision = landingKnown ? bookorbit::decide(baseline, local, remote) : bookorbit::Decision::Skip;
      if (decision == bookorbit::Decision::Equal || (!batchMode() && comparison == ProgressComparison::Synchronized)) {
        bookorbit::writeBaseline(documentHash, bookOrbitIdentity, local, remote);
        completeSync();
        return;
      }
      if (decision == bookorbit::Decision::Upload && comparison == ProgressComparison::LocalAhead) {
        // BookOrbit has no conditional progress PUT. Check the remote again before overwriting it.
        epub.reset();
        const auto endpoint = bookorbit::Store::get().endpoint();
        KOReaderProgress current{};
        const auto checked = KOReaderSyncClient::getProgress(documentHash, current, &endpoint);
        if (checked != KOReaderSyncClient::OK || !sameRemoteProgress(current, remoteProgress)) {
          {
            RenderLock lock(*this);
            state = SYNC_FAILED;
            statusMessage =
                checked == KOReaderSyncClient::OK ? tr(STR_RETRY) : KOReaderSyncClient::errorString(checked);
          }
          requestUpdate(true);
          return;
        }
        performUpload();
        return;
      }
      if (decision == bookorbit::Decision::Download && comparison == ProgressComparison::RemoteAhead) {
        saveProgressAndReturn(remotePosition.spineIndex, remotePosition.pageNumber);
        return;
      }
    }
    if (matchingBatchPositions && !isBookOrbitReset(remoteProgress)) {
      completeSync();
      return;
    }
  }
  if (smartSyncEnabled()) {
    LOG_DBG("KOSync", "Smart decision: doc=%s result=%d local=%.6f remote=%.6f remoteXpath=%s mapped=%d/%d",
            primaryHash.c_str(), static_cast<int>(comparison), localProgress.percentage, remoteProgress.percentage,
            remoteProgress.progress.c_str(), remotePosition.spineIndex, remotePosition.pageNumber);
    switch (comparison) {
      case ProgressComparison::Synchronized:
        completeSync();
        return;
      case ProgressComparison::LocalAhead:
        performUpload();
        return;
      case ProgressComparison::RemoteAhead:
        saveProgressAndReturn(remotePosition.spineIndex, remotePosition.pageNumber);
        return;
      case ProgressComparison::Unknown:
        LOG_DBG("KOSync", "Smart sync comparison unknown; opening manual selection");
        break;
    }
  }

  // localProgress was pre-computed in EpubReaderActivity before the Epub was released.
  {
    RenderLock lock(*this);
    if (batchMode()) {
      app.cancelTouchContact();
      resetUi();
    }
    state = SHOWING_RESULT;

    selectedOption = comparison == ProgressComparison::LocalAhead ? 1 : 0;
  }
  requestUpdate(true);
}

void KOReaderSyncActivity::performUpload() {
  const bool checkManualBatch = batchMode() && (state == SHOWING_RESULT || state == NO_REMOTE_PROGRESS);
  {
    RenderLock lock(*this);
    state = UPLOADING;
    statusMessage = tr(STR_UPLOAD_PROGRESS);
  }
  if (!batchMode()) requestUpdateAndWait();

  // localProgress was pre-computed in EpubReaderActivity before the Epub was released.
  KOReaderProgress progress;
  progress.document = documentHash;
  progress.progress = localProgress.xpath;
  progress.percentage = localProgress.percentage;

  // Rich position for server profiles that explicitly support the CrossPoint extension.
  // The HTTP client enforces the same boundary before serializing the extension.
  if (!bookOrbit && KOREADER_STORE.supportsRichProgress()) {
    KOReaderRichPosition pos;
    const float pct = localProgress.percentage < 0.0f   ? 0.0f
                      : localProgress.percentage > 1.0f ? 1.0f
                                                        : localProgress.percentage;
    pos.pctQ = static_cast<uint32_t>(pct * 1000000.0f + 0.5f);
    pos.spineIndex = static_cast<uint16_t>(localPosition.spineIndex);
    pos.pageNumber = static_cast<uint16_t>(localPosition.pageNumber);
    pos.totalPages = static_cast<uint16_t>(localPosition.totalPages > 0 ? localPosition.totalPages : 1);
    if (localPosition.hasParagraphIndex) {
      pos.paragraphIndex = localPosition.paragraphIndex;
    }
    pos.xpath = localProgress.xpath;
    progress.position = std::move(pos);
  }

  // Optionally include document metadata (KOReader PR #15306)
  if (!bookOrbit && KOREADER_STORE.getSendMetadata()) {
    // The Epub is released before the sync network calls and is only reloaded on the
    // remote-progress path (performSync). When uploading from NO_REMOTE_PROGRESS the
    // Epub is still null, so reload it here and guard the title/author reads to avoid
    // dereferencing a null Epub. Filename is derived from the path and is always safe.
    ensureEpubLoaded();
    KOReaderMetadata meta;
    const auto lastSlash = epubPath.rfind('/');
    meta.filename = (lastSlash != std::string::npos) ? epubPath.substr(lastSlash + 1) : epubPath;
    if (epub) {
      meta.title = epub->getTitle();
      meta.authors = epub->getAuthor();
      if (KOREADER_STORE.supportsExtendedMetadata()) {
        Epub::SyncMetadata syncMetadata;
        if (epub->loadSyncMetadata(syncMetadata)) {
          meta.isbn = std::move(syncMetadata.isbn);
          meta.asin = std::move(syncMetadata.asin);
          meta.series = std::move(syncMetadata.series);
          meta.seriesIndex = syncMetadata.seriesIndex;
        } else {
          LOG_DBG("KOSync", "Could not read extended EPUB metadata; sending core metadata only");
        }
      }
    } else {
      LOG_ERR("KOSync", "Epub unavailable for metadata; sending filename only");
    }
    // Plugin sidecar fields ("<book>.meta.json", written at download time via
    // the catalog sidecar mechanism or /api/plugin-fs) ride along so a custom
    // sync server can route progress by a service book id.
    // A missing sidecar (the common case) leaves extraJson empty.
    Storage.readFileToString("KOSync", epubPath + ".meta.json", 2 * 1024, meta.extraJson);
    progress.metadata = std::move(meta);
  }

  // Release the Epub before the network call so the TLS handshake has enough free heap
  // (consistent with the release-before-sync pattern in performSync); nothing below needs it.
  epub.reset();

  KOReaderSyncClient::Error result;
  {
    // Restore power-save before esp_wifi_stop below; set_ps on a stopped radio fails.
    WifiPowerSaveGuard psGuard;
    if (bookOrbit) {
      const auto endpoint = bookorbit::Store::get().endpoint();
      bool mayUpload = true;
      if (checkManualBatch) {
        KOReaderProgress current{};
        const auto checked = KOReaderSyncClient::getProgress(documentHash, current, &endpoint);
        mayUpload = hasRemoteProgress
                        ? checked == KOReaderSyncClient::OK && sameRemoteProgress(current, remoteProgress)
                        : checked == KOReaderSyncClient::NO_STORED_PROGRESS || checked == KOReaderSyncClient::NOT_FOUND;
      }
      bookorbit::Position confirmed;
      result = mayUpload ? bookorbit::uploadAndConfirm(progress, endpoint, confirmed, batchMode())
                         : KOReaderSyncClient::SERVER_ERROR;
      if (result == KOReaderSyncClient::OK) {
        bookorbit::writeBaseline(documentHash, bookOrbitIdentity, {progress.progress, progress.percentage}, confirmed);
        bookorbit::clearPending(epubPath);
      }
    } else {
      result = KOReaderSyncClient::updateProgress(progress);
    }
  }

  // Drop the radio while user reads the result; full teardown happens at silent reboot.
  // Not if Developer Mode raised it: esp_wifi_stop() is unambiguous -- the radio
  // is off for everyone, including a connection this screen did not make.
  if (!syncSavedBook && !devmode::holdsRadio()) esp_wifi_stop();

  if (batchMode() && result == KOReaderSyncClient::BOOK_NOT_IN_LIBRARY) {
    RenderLock lock(*this);
    state = BATCH_SKIPPED;
    return;
  }

  if (result != KOReaderSyncClient::OK) {
    {
      RenderLock lock(*this);
      state = SYNC_FAILED;
      statusMessage = KOReaderSyncClient::errorString(result);
    }
    requestUpdate();
    return;
  }

  {
    RenderLock lock(*this);
    state = UPLOAD_COMPLETE;
  }
  markAutoReturn();
  requestUpdate(true);
}

void KOReaderSyncActivity::onEnter() {
  Activity::onEnter();
  if (!syncSavedBook) ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);

  resetUi();
  app.on(ACTION_ROW, &KOReaderSyncActivity::onResultRow, this);
  if (batchMode()) {
    app.on(
        ACTION_BATCH_CARD,
        [](const fui::ActionEvent& event, void* user) {
          static_cast<KOReaderSyncActivity*>(user)->openBatchBooks(event.value);
        },
        this);
  }
  app.setScreen(&KOReaderSyncActivity::resultScreen, this);

  // Check for credentials first
  if (!(bookOrbit ? bookorbit::Store::get().configured() : KOREADER_STORE.hasCredentials())) {
    state = NO_CREDENTIALS;
    requestUpdate();
    return;
  }

  if (syncSavedBook && !batchMode()) {
    const auto saved = loadSavedBookPosition();
    epub.reset();
    if (saved != SavedPositionResult::Ready) {
      LOG_ERR("ORBIT", "Could not read a recent book's saved position");
      state = SYNC_FAILED;
      statusMessage = tr(STR_BOOKORBIT_LOAD_PROGRESS_FAILED);
      requestUpdate();
      return;
    }
  }
  if (syncSavedBook) wifiWasOff = WiFi.getMode() == WIFI_MODE_NULL;

  const bool alreadyConnected = WiFi.status() == WL_CONNECTED;
  wifiActivated = !syncSavedBook || !alreadyConnected;
  if (alreadyConnected) {
    LOG_DBG("KOSync", "Already connected to WiFi");
    onWifiSelectionComplete(true);
    return;
  }

  // Launch WiFi selection subactivity
  LOG_DBG("KOSync", "Launching WifiSelectionActivity...");
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void KOReaderSyncActivity::releaseSavedBookWifi() {
  if (!wifiActivated || devmode::holdsRadio()) return;
  wifiActivated = false;
  WiFi.disconnect(false);
  if (wifiWasOff) WiFi.mode(WIFI_OFF);
}

void KOReaderSyncActivity::onExit() {
  Activity::onExit();

  if (syncSavedBook) {
    releaseSavedBookWifi();
  } else if (wifiActivated && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    delay(30);
    silentRestartToReader();
  }
}

void KOReaderSyncActivity::stepBatch() {
  epubPath = std::move(batchBooks[batchIndex].path);
  localPosition = {};
  localProgress = {};
  localChapterName.clear();
  remoteProgress = {};
  remotePosition = {};
  hasRemoteProgress = false;
  selectedOption = 0;

  const auto saved = loadSavedBookPosition();
  epub.reset();
  if (saved != SavedPositionResult::Ready) {
    advanceBatch(saved == SavedPositionResult::ReadError ? SYNC_FAILED : BATCH_SKIPPED);
    return;
  }

  performSync();
  if (state != SHOWING_RESULT && state != NO_REMOTE_PROGRESS) advanceBatch(state);
}

void KOReaderSyncActivity::advanceBatch(State outcome) {
  {
    RenderLock lock(*this);
    epub.reset();
    auto& book = batchBooks[batchIndex];
    book.path = std::move(epubPath);
    book.outcome = outcome == BATCH_SKIPPED                                 ? bookorbit::BatchOutcome::Skipped
                   : outcome == SYNC_COMPLETE || outcome == UPLOAD_COMPLETE ? bookorbit::BatchOutcome::Synced
                                                                            : bookorbit::BatchOutcome::Failed;
    ++batchCounts[static_cast<int>(book.outcome)];
    state = ++batchIndex == batchBooks.size() ? BATCH_COMPLETE : SYNCING;
    if (state == BATCH_COMPLETE) {
      app.cancelTouchContact();
      resetUi();
    }
  }
  if (state == BATCH_COMPLETE) releaseSavedBookWifi();
  requestUpdate();
}

void KOReaderSyncActivity::openBatchBooks(int index) {
  if (state != BATCH_COMPLETE || index < 0 || index >= bookorbit::BATCH_OUTCOME_COUNT) return;
  auto list =
      bookorbit::makeBatchBookList(renderer, mappedInput, batchBooks, static_cast<bookorbit::BatchOutcome>(index));
  if (!list) return;
  {
    RenderLock lock(*this);
    batchSelectedCard = index;
    app.clearTapFlash();
  }
  startActivityForResult(std::move(list), [this](const ActivityResult&) {
    {
      RenderLock lock(*this);
      app.cancelTouchContact();
      resetUi();
    }
    requestUpdate();
  });
}

void KOReaderSyncActivity::chooseResultOption() {
  if (selectedOption == 0) {
    saveProgressAndReturn(remotePosition.spineIndex, remotePosition.pageNumber);
  } else {
    performUpload();
  }
  if (batchMode()) advanceBatch(state);
}

void KOReaderSyncActivity::startUpload() {
  if (documentHash.empty()) {
    documentHash = !bookOrbit && KOREADER_STORE.getMatchMethod() == DocumentMatchMethod::FILENAME
                       ? KOReaderDocumentId::calculateFromFilename(epubPath)
                       : KOReaderDocumentId::calculate(epubPath);
  }
  performUpload();
  if (batchMode()) advanceBatch(state);
}

void KOReaderSyncActivity::onResultRow(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<KOReaderSyncActivity*>(user);
  // Activation leaves this screen (applies/uploads); drop the flash so it does
  // not ghost onto the next paint.
  self->app.clearTapFlash();
  if (self->batchMode() && event.value == SKIP_BOOK) {
    self->advanceBatch(BATCH_SKIPPED);
    return;
  }
  if (self->state == SHOWING_RESULT) {
    if (event.value < 0 || event.value > 1) return;
    self->selectedOption = event.value;
    self->chooseResultOption();
  } else if (self->state == NO_REMOTE_PROGRESS) {
    self->startUpload();
  }
}

void KOReaderSyncActivity::resultScreen(UiScreen& screen, void* user) {
  static_cast<KOReaderSyncActivity*>(user)->buildResultScreen(screen);
}

void KOReaderSyncActivity::buildResultScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  // Side padding is 0 here (like the other FreeInkApp screens): the action list
  // supplies its own theme side padding, and the raw comparison text is indented
  // to line up with the list rows below (see labelIndent).
  if (bookOrbit) {
    pixelui::begin(screen, metrics.buttonHintsHeight);
  } else {
    screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                  static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  }
  if (state == BATCH_COMPLETE) {
    pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
    bookorbit::drawBatchCards(screen.frame(), screen.body(), batchCounts, ACTION_BATCH_CARD, batchSelectedCard);
    return;
  }
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (batchMode()) {
    const auto nameStart = epubPath.find_last_of('/');
    const char* title = epub && !epub->getTitle().empty()
                            ? epub->getTitle().c_str()
                            : epubPath.c_str() + (nameStart == std::string::npos ? 0 : nameStart + 1);
    auto titleStyle = screen.theme().bodyText;
    titleStyle.bold = true;
    titleStyle.maxLines = 2;
    const auto band =
        screen.takeTop(std::max(44, screen.target().lineHeight(titleStyle.font) * 2), screen.theme().spaceMd);
    constexpr int skipWidth = 104;
    screen.target().text(fui::makeRect(band.x, band.y, band.width - skipWidth - 8, band.height), title, titleStyle);
    fui::ButtonProps skip;
    skip.label = tr(STR_UI_SKIP_431DE8);
    skip.action = ACTION_ROW;
    skip.value = SKIP_BOOK;
    skip.inputMask = fui::InputTouch;
    skip.text = screen.theme().smallText;
    skip.styles = pixelui::buttonStyles();
    screen.button(skip, fui::makeRect(band.right() - skipWidth, band.y + (band.height - 44) / 2, skipWidth, 44));
  }

  if (state == SHOWING_RESULT) {
    // Chapter names (remote requires the lazily-loaded Epub; local was
    // pre-computed before the Epub was released).
    const int remoteTocIndex = epub->getTocIndexForSpineIndex(remotePosition.spineIndex);
    const std::string remoteChapter =
        (remoteTocIndex >= 0) ? epub->getTocItem(remoteTocIndex).title
                              : (std::string(tr(STR_SECTION_PREFIX)) + std::to_string(remotePosition.spineIndex + 1));
    char localChapterFallback[32];
    const char* localChapter = localChapterName.c_str();
    if (localChapterName.empty()) {
      snprintf(localChapterFallback, sizeof(localChapterFallback), "%s%d", tr(STR_SECTION_PREFIX),
               localPosition.spineIndex + 1);
      localChapter = localChapterFallback;
    }

    char remoteVal[64];
    snprintf(remoteVal, sizeof(remoteVal), tr(STR_PAGE_OVERALL_FORMAT), remotePosition.pageNumber + 1,
             remoteProgress.percentage * 100);
    char localVal[64];
    snprintf(localVal, sizeof(localVal), tr(STR_PAGE_TOTAL_OVERALL_FORMAT), localPosition.pageNumber + 1,
             localPosition.totalPages, localProgress.percentage * 100);
    char deviceStr[80];
    deviceStr[0] = '\0';
    if (!remoteProgress.device.empty()) {
      snprintf(deviceStr, sizeof(deviceStr), tr(STR_DEVICE_FROM_FORMAT), remoteProgress.device.c_str());
    }

    // Labeled, multi-line comparison flowing from the top. Indent everything to
    // the list rows' content-left (the row inset + side padding the list adds
    // below) so the "Remote"/"Local" labels sit directly above the row icons.
    auto labelStyle = screen.theme().bodyText;
    labelStyle.bold = true;
    auto detailStyle = screen.theme().smallText;
    const int16_t labelH = screen.target().lineHeight(labelStyle.font);
    const int16_t detailH = screen.target().lineHeight(detailStyle.font);
    const int16_t labelIndent = static_cast<int16_t>(screen.theme().listInset + screen.theme().listSidePadding);
    const int16_t detailIndent = static_cast<int16_t>(labelIndent + screen.theme().spaceMd);
    const auto textLine = [&](const char* text, const fui::TextStyle& style, int16_t height, int16_t indent,
                              int16_t gap) {
      fui::Rect r = screen.takeTop(height, gap);
      r.x = static_cast<int16_t>(r.x + indent);
      r.width = static_cast<int16_t>(r.width - indent);
      screen.target().text(r, text, style);
    };
    const auto labelLine = [&](const char* text) {
      textLine(text, labelStyle, labelH, labelIndent, screen.theme().spaceSm);
    };
    const auto detailLine = [&](const char* text) {
      textLine(text, detailStyle, detailH, detailIndent, screen.theme().spaceXs);
    };

    labelLine(tr(STR_REMOTE_LABEL));
    detailLine(remoteChapter.c_str());
    detailLine(remoteVal);
    if (deviceStr[0] != '\0') detailLine(deviceStr);
    screen.spacer(screen.theme().spaceLg);
    labelLine(tr(STR_LOCAL_LABEL));
    detailLine(localChapter);
    detailLine(localVal);

    // Two themed action rows flowing directly below the labels (not anchored to
    // the bottom). Rendered through the list component so they inherit the
    // active theme's row radius, insets, and selection style, matching every
    // other selectable list in the UI. Apply Remote pulls (download), Upload
    // Local pushes (upload); the selected row highlights for physical-button
    // users and tap works either way.
    screen.spacer(screen.theme().spaceMd);
    fui::ListItem actions[2];
    actions[0].label = tr(STR_APPLY_REMOTE);
    actions[0].icon = fui::bitmapFromIcon(icon_download_24);
    actions[0].actionValue = 0;
    actions[1].label = tr(STR_UPLOAD_LOCAL);
    actions[1].icon = fui::bitmapFromIcon(icon_upload_24);
    actions[1].actionValue = 1;
    fui::ListProps actionProps;
    actionProps.items = actions;
    actionProps.count = 2;
    actionProps.selectedIndex = static_cast<int16_t>(selectedOption);
    actionProps.action = ACTION_ROW;
    actionProps.inputMask = fui::InputTouch;  // physical buttons stay in loop()
    actionProps.scrollIndicator = false;      // never scrolls; no indicator needed
    if (bookOrbit) styleBookOrbitActions(actionProps);
    // Non-touch hardware (X3/X4) keeps the original, denser row height instead
    // of FreeInkUI's touch-target-sized default (see
    // UiListActivity::syncListViewport); actionsBand must use the same value
    // or the band and the rows it contains fall out of sync.
    int16_t actionRowHeight = screen.theme().rowHeight;
    if (!mappedInput.hasTouch()) {
      actionRowHeight = static_cast<int16_t>(UITheme::getInstance().getMetrics().listRowHeight);
      actionProps.rowHeight = actionRowHeight;
    }
    // Keep the theme's row inset + side padding so the selected-row highlight has
    // the same padding around its icon/label as every other list in the UI; the
    // labels above are indented to match this content-left.
    const int rowGap = bookOrbit ? actionProps.rowGap : screen.theme().listRowGap;
    const auto actionsBand = static_cast<int16_t>(actionRowHeight * 2 + rowGap + screen.theme().spaceSm);
    screen.list(actionProps, actionsBand);
    return;
  }

  if (state == NO_REMOTE_PROGRESS) {
    auto centered = screen.theme().bodyText;
    centered.align = fui::TextAlign::Center;
    auto centeredBold = centered;
    centeredBold.bold = true;
    const int16_t lineH = screen.target().lineHeight(centered.font);
    screen.target().text(screen.takeTop(lineH, screen.theme().spaceSm), tr(STR_NO_REMOTE_MSG), centeredBold);
    screen.target().text(screen.takeTop(lineH, screen.theme().spaceMd), tr(STR_UPLOAD_PROMPT), centered);

    // Single themed action row anchored to the bottom, matching the lists used
    // everywhere else (inherits the theme's row radius + selection style).
    fui::ListItem action;
    action.label = tr(STR_UPLOAD_LOCAL);
    action.actionValue = 0;
    fui::ListProps actionProps;
    actionProps.items = &action;
    actionProps.count = 1;
    actionProps.selectedIndex = 0;
    actionProps.action = ACTION_ROW;
    actionProps.inputMask = fui::InputTouch;
    actionProps.scrollIndicator = false;
    if (bookOrbit) styleBookOrbitActions(actionProps);
    // See the equivalent override above; keeps actionsBand in sync with the
    // row height actually used on non-touch hardware (X3/X4).
    int16_t actionRowHeight = screen.theme().rowHeight;
    if (!mappedInput.hasTouch()) {
      actionRowHeight = static_cast<int16_t>(UITheme::getInstance().getMetrics().listRowHeight);
      actionProps.rowHeight = actionRowHeight;
    }
    const auto actionsBand = static_cast<int16_t>(actionRowHeight + screen.theme().spaceMd);
    screen.list(actionProps, actionsBand, fui::LayoutAnchor::Bottom);
  }
}

void KOReaderSyncActivity::render(RenderLock&&) {
  renderer.clearScreen();

  auto metrics = UITheme::getInstance().getMetrics();
  Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);

  const char* title = state == SHOWING_RESULT && !batchMode() ? tr(STR_PROGRESS_FOUND)
                      : bookOrbit                             ? tr(STR_BOOKORBIT)
                                                              : tr(STR_KOREADER_SYNC);
  if (bookOrbit) {
    pixelui::drawHeader(renderer, title, I18N.getLanguage() != Language::EN);
    const auto header = pixelui::geometry(uiTarget.deviceContext()).header;
    renderer.drawLine(header.x, header.bottom(), header.right() - 1, header.bottom());
  } else {
    GUI.drawHeader(renderer, Rect{screen.x, screen.y + metrics.topPadding, screen.width, metrics.headerHeight}, title);
  }

  int top = screen.y + screen.height / 2 - 40;
  if (batchMode() && state != NO_CREDENTIALS && state != SHOWING_RESULT && state != NO_REMOTE_PROGRESS) {
    if (state == BATCH_COMPLETE) {
      renderUi();
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    } else {
      char progress[24];
      snprintf(progress, sizeof(progress), "%u / %u", unsigned(batchIndex + 1), unsigned(batchBooks.size()));
      UITheme::drawCenteredWrappedText(renderer, Rect{screen.x + 8, top, screen.width - 16, 64}, UI_10_FONT_ID,
                                       tr(STR_BOOKORBIT_SYNC_PENDING), 2, true, EpdFontFamily::BOLD);
      UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, top + 68, progress);
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    }
    renderer.displayBuffer();
    return;
  }

  if (state == NO_CREDENTIALS) {
    UITheme::drawCenteredWrappedText(renderer, Rect{screen.x, top - 30, screen.width, 60}, UI_10_FONT_ID,
                                     tr(STR_NO_CREDENTIALS_MSG), 2, true, EpdFontFamily::BOLD);
    UITheme::drawCenteredWrappedText(renderer, Rect{screen.x, top + 40, screen.width, 90}, UI_10_FONT_ID,
                                     bookOrbit ? tr(STR_BOOKORBIT_HELP) : tr(STR_KOREADER_SETUP_HINT), 3, true,
                                     EpdFontFamily::BOLD);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == SYNCING || state == UPLOADING) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, top, statusMessage.c_str(), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  if (state == SHOWING_RESULT) {
    // Comparison rows + option selection render through the FreeInkApp
    // (themed rows, tap-flash); the header above shows "Progress Found".
    renderUi();

    const auto labels = mappedInput.mapLabels(batchMode() ? tr(STR_UI_SKIP_431DE8) : tr(STR_BACK), tr(STR_SELECT),
                                              tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == NO_REMOTE_PROGRESS) {
    // Prompt text + upload button render through the FreeInkApp.
    renderUi();

    const auto labels =
        mappedInput.mapLabels(batchMode() ? tr(STR_UI_SKIP_431DE8) : tr(STR_BACK), tr(STR_UPLOAD), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == UPLOAD_COMPLETE || state == SYNC_COMPLETE) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, top,
                              state == UPLOAD_COMPLETE ? tr(STR_UPLOAD_SUCCESS) : tr(STR_ALREADY_SYNCED), true,
                              EpdFontFamily::BOLD);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == SYNC_FAILED) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, top, tr(STR_SYNC_FAILED_MSG), true, EpdFontFamily::BOLD);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, top + 40, statusMessage.c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}

void KOReaderSyncActivity::loop() {
  if (batchMode() && state == SYNCING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back))
      exitSync();
    else
      stepBatch();
    return;
  }
  if (state == BATCH_COMPLETE) {
    if (!routingReady()) return;
    using Button = MappedInputManager::Button;
    if (mappedInput.wasReleased(Button::Back)) {
      exitSync();
      return;
    }
    const auto touch = routeTouch(mappedInput);
    if (touch.routed && app.invalidated()) requestUpdate();
    if (touch) return;
    if (mappedInput.wasReleased(Button::Confirm)) {
      openBatchBooks(batchSelectedCard);
      return;
    }
    const bool previous = mappedInput.wasReleased(Button::Up) || mappedInput.wasReleased(Button::Left);
    const bool next = mappedInput.wasReleased(Button::Down) || mappedInput.wasReleased(Button::Right);
    if (previous || next) {
      const int count = bookorbit::BATCH_OUTCOME_COUNT;
      const int selection = (batchSelectedCard + (previous ? count - 1 : 1)) % count;
      {
        RenderLock lock(*this);
        batchSelectedCard = selection;
      }
      requestUpdate();
    }
    return;
  }

  if (state == NO_CREDENTIALS || state == SYNC_FAILED || state == UPLOAD_COMPLETE || state == SYNC_COMPLETE) {
    if (autoReturnAt != 0 && millis() >= autoReturnAt) {
      exitSync();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      exitSync();
    }
    return;
  }

  if (state == SHOWING_RESULT) {
    if (batchMode() && !routingReady()) return;
    // Touch goes through the FreeInkApp: render() registered the compare rows;
    // route the snapshot and let onResultRow apply/upload on tap.
    const auto route = routeTouch(mappedInput);
    if (route.routed && app.invalidated()) requestUpdate();
    if (route) return;  // dispatched to onResultRow

    // Navigate the two options with physical buttons.
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Left) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down) ||
        mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      selectedOption = (selectedOption + 1) % 2;  // Wrap around among 2 options
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      chooseResultOption();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (batchMode())
        advanceBatch(BATCH_SKIPPED);
      else
        exitSync();
    }
    return;
  }

  if (state == NO_REMOTE_PROGRESS) {
    if (batchMode() && !routingReady()) return;
    // Touch goes through the FreeInkApp: render() registered the upload button.
    const auto route = routeTouch(mappedInput);
    if (route.routed && app.invalidated()) requestUpdate();
    if (route) return;  // dispatched to onResultRow -> startUpload

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startUpload();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (batchMode())
        advanceBatch(BATCH_SKIPPED);
      else
        exitSync();
    }
    return;
  }
}
