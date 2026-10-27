#include "XtcReaderActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <iterator>

#include "CrossPointSettings.h"
#include "ProgressFile.h"
#include "ReaderActivity.h"
#include "ReaderUtils.h"
#include "XtcPageView.h"
#include "XtcReaderChapterSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void XtcReaderActivity::openTextSettings() {
  if (!xtc || isAtEndOfBook() || pageSettings) return;
  RenderLock lock;
  pageSettings = makeUniqueNoThrow<ReaderToolbarUi>(renderer);
  if (!pageSettings) {
    LOG_ERR("XTR", "OOM: page sheet");
    return;
  }
  settingsTab = xtc->getBitDepth() == 1 ? 1 : 3;
  pageSettings->begin();
  renderPageSettings();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
void XtcReaderActivity::renderPageSettings() {
  if (!pageSettings) return;
  static constexpr StrId positions[] = {StrId::STR_HIDE, StrId::STR_BOTTOM, StrId::STR_TOP};
  static constexpr StrId refresh[] = {StrId::STR_PAGES_1,  StrId::STR_PAGES_5,  StrId::STR_PAGES_10,
                                      StrId::STR_PAGES_15, StrId::STR_PAGES_30, StrId::STR_NEVER};
  static_assert(std::size(refresh) == CrossPointSettings::REFRESH_FREQUENCY_COUNT);
  auto& model = overlayModel;
  model = {};
  model.textSheet = true;
  auto& text = model.text;
  text.fixed = true;
  text.statusAvailable = xtc && xtc->getBitDepth() == 1;
  text.tab = settingsTab;
  text.contentsAvailable = !xtc->getChapters().empty();
  text.listContext = this;
  text.listPage = settingsPage;
  text.listCount = settingsTab == 2 ? xtc->getChapters().size() : 2;
  text.listLabel = [](const void* context, int n, char* out, size_t size) {
    const auto* self = static_cast<const XtcReaderActivity*>(context);
    std::snprintf(out, size, "%s",
                  self->settingsTab == 2 ? self->xtc->getChapters()[n].name.c_str()
                  : n == 0               ? tr(STR_REFRESH_FREQ)
                                         : I18N.get(self->completionMenuLabelId()));
  };
  text.listCheck = [](const void* context, int n) {
    const auto* self = static_cast<const XtcReaderActivity*>(context);
    (void)self;
    (void)n;
    return -1;
  };
  text.listValue = [](const void* context, int n, char* out, size_t size) {
    const auto* self = static_cast<const XtcReaderActivity*>(context);
    if (self->settingsTab == 3 && n == 0) std::snprintf(out, size, "%s", self->overlayModel.text.refresh);
  };
  text.placement = I18N.get(positions[SETTINGS.xtcStatusBarMode % 3]);
  text.refresh = I18N.get(refresh[SETTINGS.refreshFrequency % CrossPointSettings::REFRESH_FREQUENCY_COUNT]);
  text.percentage = SETTINGS.statusBarBookProgressPercentage;
  text.night = SETTINGS.screenInverted;
  pageSettings->setModel(model);
  pageSettings->render();
}

bool XtcReaderActivity::loadBook() {
  auto loadedXtc = makeUniqueNoThrow<Xtc>(bookPath, "/.crosspoint");
  if (!loadedXtc) {
    LOG_ERR("XTR", "Failed to allocate XTC object");
    return false;
  }
  if (!loadedXtc->load()) {
    LOG_ERR("XTR", "Failed to load XTC");
    return false;
  }
  xtc = std::move(loadedXtc);
  xtc->setupCacheDir();
  loadProgress();
  return true;
}

void XtcReaderActivity::openChapterSelection() {
  if (xtc && xtc->hasChapters() && !xtc->getChapters().empty()) {
    startActivityForResult(std::make_unique<XtcReaderChapterSelectionActivity>(renderer, mappedInput, xtc, currentPage),
                           [this](const ActivityResult& result) {
                             if (!result.isCancelled) {
                               currentPage = std::get<PageResult>(result.data).page;
                               requestUpdate();
                             }
                           });
  }
}

bool XtcReaderActivity::handleFormatInput() {
  if (!xtc) {
    return false;
  }

  if (pageSettings) {
    const auto routed = pageSettings->route(mappedInput);
    int swipeStep = 0;
    if (settingsTab == 2 || settingsTab == 3) {
      switch (mappedInput.wasSwipe()) {
        case MappedInputManager::SwipeDir::Left:
          swipeStep = 1;
          break;
        case MappedInputManager::SwipeDir::Right:
          swipeStep = -1;
          break;
        case MappedInputManager::SwipeDir::None:
        case MappedInputManager::SwipeDir::Up:
        case MappedInputManager::SwipeDir::Down:
          break;
      }
    }
    if (routed.event == ReaderToolbarUi::Event::Dismiss ||
        (!swipeStep && mappedInput.wasReleased(MappedInputManager::Button::Back)) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Down) {
      RenderLock lock;
      pageSettings.reset();
      requestUpdate();
      return true;
    }
    if (routed.event == ReaderToolbarUi::Event::Tool) {
      RenderLock lock;
      settingsTab = std::clamp(routed.value, 1, 3);
      settingsPage = 0;
      pageSettings->begin();
      renderPageSettings();
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      return true;
    }
    if (routed.event == ReaderToolbarUi::Event::Row || swipeStep) {
      if (!swipeStep && settingsTab == 3 && routed.value == readertext::MorePick + 1) {
        {
          RenderLock lock;
          pageSettings.reset();
        }
        requestFinishBook();
        return true;
      }
      {
        RenderLock lock;
        int control = routed.value;
        if (swipeStep || control == readertext::SheetPrevious || control == readertext::SheetNext) {
          const int count = settingsTab == 2 ? xtc->getChapters().size() : 2;
          const int capacity = readertext::sheetRowsPerPage(pageSettings->uiTarget.deviceContext(), true);
          const int step = swipeStep ? swipeStep : (control == readertext::SheetNext ? 1 : -1);
          const int page = std::clamp(settingsPage + step, 0, std::max(0, (count - 1) / capacity));
          if (page == settingsPage) return true;
          settingsPage = page;
          pageSettings->begin();
          renderPageSettings();
          renderer.displayBuffer(HalDisplay::FAST_REFRESH);
          return true;
        }
        if (settingsTab == 2 && control >= readertext::ChapterPick) {
          const int n = control - readertext::ChapterPick;
          if (n < int(xtc->getChapters().size())) {
            currentPage = xtc->getChapters()[n].startPage;
            pageSettings.reset();
            requestUpdate();
          }
          return true;
        }
        if (settingsTab == 3 && control >= readertext::MorePick && control < readertext::MorePick + 1)
          control = readertext::Refresh;
        switch (control) {
          case readertext::Placement:
            if (xtc->getBitDepth() != 1) return true;
            SETTINGS.xtcStatusBarMode = (SETTINGS.xtcStatusBarMode + 1) % 3;
            break;
          case readertext::Percentage:
            if (xtc->getBitDepth() != 1) return true;
            SETTINGS.statusBarBookProgressPercentage = !SETTINGS.statusBarBookProgressPercentage;
            if (SETTINGS.statusBarBookProgressPercentage &&
                SETTINGS.xtcStatusBarMode == CrossPointSettings::XTC_STATUS_BAR_HIDE)
              SETTINGS.xtcStatusBarMode = CrossPointSettings::XTC_STATUS_BAR_BOTTOM;
            break;
          case readertext::Night:
            SETTINGS.screenInverted = !SETTINGS.screenInverted;
            renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
            break;
          case readertext::Refresh:
            SETTINGS.refreshFrequency = (SETTINGS.refreshFrequency + 1) % CrossPointSettings::REFRESH_FREQUENCY_COUNT;
            break;
          default:
            return true;
        }
      }
      SETTINGS.saveToFile();
      requestUpdate();
    }
    return true;
  }

  // Enter chapter selection activity on Confirm release or touch menu gesture
  const bool touchMenu = ReaderUtils::isTouchMenuGesture(renderer, mappedInput);
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) || touchMenu) {
    if (touchMenu && xtc->hasChapters() && !xtc->getChapters().empty()) haptic_feedback::touchAction();
    openChapterSelection();
    return true;
  }
  return false;
}

void XtcReaderActivity::applyInitialOrientation() { ReaderActivity::applyInitialOrientation(); }

void XtcReaderActivity::renderBook() {
  if (!xtc) {
    return;
  }

  renderPage();
  saveProgress();
  if (pageSettings) {
    renderPageSettings();
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}

XtcReaderActivity::StatusBarInfo XtcReaderActivity::getStatusBarInfo() const {
  const auto sb = SETTINGS.statusBarSpec();
  const int bookPageCount = static_cast<int>(xtc->getPageCount());
  const int bookPage = static_cast<int>(currentPage) + 1;
  std::string title = sb.titleMode == CrossPointSettings::STATUS_BAR_TITLE::BOOK_TITLE ? xtc->getTitle() : "";

  if (!xtc->hasChapters()) {
    return StatusBarInfo{bookPage, bookPageCount, std::move(title)};
  }

  const auto& chapters = xtc->getChapters();
  const auto chapterIt = std::find_if(chapters.begin(), chapters.end(), [this](const xtc::ChapterInfo& chapter) {
    return currentPage >= chapter.startPage && currentPage <= chapter.endPage;
  });

  if (chapterIt == chapters.end() || chapterIt->endPage < chapterIt->startPage) {
    return StatusBarInfo{bookPage, bookPageCount, std::move(title)};
  }

  if (sb.titleMode == CrossPointSettings::STATUS_BAR_TITLE::CHAPTER_TITLE) {
    title = chapterIt->name.empty() ? tr(STR_UNNAMED) : chapterIt->name;
  }

  return StatusBarInfo{static_cast<int>(currentPage - chapterIt->startPage) + 1,
                       static_cast<int>(chapterIt->endPage - chapterIt->startPage) + 1, std::move(title)};
}

void XtcReaderActivity::renderStatusBarOverlay(GfxRenderer& renderer, const StatusBarOverlayPosition position) const {
  const auto sb = SETTINGS.statusBarSpec();
  const bool drawBottom = sb.xtcMode == CrossPointSettings::XTC_STATUS_BAR_MODE::XTC_STATUS_BAR_BOTTOM &&
                          position == StatusBarOverlayPosition::Bottom;
  const bool drawTop = sb.xtcMode == CrossPointSettings::XTC_STATUS_BAR_MODE::XTC_STATUS_BAR_TOP &&
                       position == StatusBarOverlayPosition::Top;
  if (!drawBottom && !drawTop) {
    return;
  }

  const int statusBarHeight = UITheme::getInstance().getStatusBarHeight();
  if (statusBarHeight <= 0) {
    return;
  }

  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);

  int clearY;
  int paddingBottom = 0;
  if (position == StatusBarOverlayPosition::Bottom) {
    clearY = renderer.getScreenHeight() - orientedMarginBottom - statusBarHeight - 4;
    if (clearY < 0) {
      clearY = 0;
    }
  } else {
    clearY = orientedMarginTop;
    paddingBottom = renderer.getScreenHeight() - statusBarHeight - orientedMarginBottom - orientedMarginTop - 4;
  }
  const int clearHeight = position == StatusBarOverlayPosition::Bottom
                              ? renderer.getScreenHeight() - orientedMarginBottom - clearY
                              : statusBarHeight + 4;
  if (clearHeight > 0) {
    renderer.fillRect(0, clearY, renderer.getScreenWidth(), clearHeight, false);
  }

  const int pageCount = static_cast<int>(xtc->getPageCount());
  const int displayPage = static_cast<int>(currentPage) + 1;
  const float progress = pageCount > 0 ? (static_cast<float>(displayPage) * 100.0f) / pageCount : 0.0f;
  const auto pageInfo = getStatusBarInfo();
  GUI.drawStatusBar(renderer, progress, pageInfo.currentPage, pageInfo.pageCount, pageInfo.title, paddingBottom);
}

void XtcReaderActivity::renderPage() {
  const uint16_t pageWidth = xtc->getPageWidth();
  const uint16_t pageHeight = xtc->getPageHeight();
  const uint8_t bitDepth = xtc->getBitDepth();

  size_t pageBufferSize;
  if (bitDepth == 2) {
    pageBufferSize = static_cast<size_t>(pageWidth) * ((static_cast<size_t>(pageHeight) + 7) / 8) * 2;
  } else {
    pageBufferSize = ((pageWidth + 7) / 8) * pageHeight;
  }

  auto pageBuffer = makeUniqueNoThrow<uint8_t[]>(pageBufferSize);
  if (!pageBuffer) {
    LOG_ERR("XTR", "Failed to allocate page buffer (%lu bytes)", pageBufferSize);
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_MEMORY_ERROR), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  size_t bytesRead = xtc->loadPage(currentPage, pageBuffer.get(), pageBufferSize);
  if (bytesRead == 0) {
    LOG_ERR("XTR", "Failed to load page %lu: bufferSize=%lu bitDepth=%u error=%s", currentPage, pageBufferSize,
            bitDepth, xtc::errorToString(xtc->getLastError()));
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_PAGE_LOAD_ERROR), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  renderer.clearScreen();

  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  if (bitDepth == 1) {
    const int statusHeight = UITheme::getInstance().getStatusBarHeight();
    if (statusHeight > 0) {
      const auto placement = SETTINGS.statusBarSpec().xtcMode;
      if (placement == CrossPointSettings::XTC_STATUS_BAR_TOP) top += statusHeight + 4;
      if (placement == CrossPointSettings::XTC_STATUS_BAR_BOTTOM) bottom += statusHeight + 4;
    }
  }
  const auto view = xtcview::fit(pageWidth, pageHeight, renderer.getScreenWidth(), renderer.getScreenHeight(), top,
                                 right, bottom, left);
  auto getPixelValue = [&](int x, int y) {
    return xtcview::pixel(pageBuffer.get(), pageWidth, pageHeight, bitDepth, view.sourceX(x), view.sourceY(y));
  };

  if (bitDepth == 2) {
    for (int y = 0; y < view.height; ++y) {
      for (int x = 0; x < view.width; ++x) {
        if (getPixelValue(x, y) >= 1) {
          renderer.drawPixel(view.x + x, view.y + y, true);
        }
      }
    }

    if (pagesUntilFullRefresh <= 1) {
      // Periodic ghost cleanup: scrub via the normal path, then run the
      // settle flavor of the grayscale base pass (DTM planes are equal after
      // the display sync, so only the gentle reinforcement cells fire).
      // Combined-base panels (Paper Mono) instead defer the base so the gray
      // planes below join it in one waveform.
      if (renderer.grayscaleCapabilities().base == HalDisplay::GrayscaleBase::Combined) {
        renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
      } else {
        renderer.displayBuffer(HalDisplay::HALF_REFRESH);
        renderer.preconditionGrayscale();
      }
      pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
    } else {
      renderer.displayGrayscaleBase(HalDisplay::FAST_REFRESH);
      pagesUntilFullRefresh--;
    }

    renderer.clearScreen(0x00);
    for (int y = 0; y < view.height; ++y) {
      for (int x = 0; x < view.width; ++x) {
        if (getPixelValue(x, y) == 1) {
          renderer.drawPixel(view.x + x, view.y + y, false);
        }
      }
    }
    renderer.copyGrayscaleLsbBuffers();

    renderer.clearScreen(0x00);
    for (int y = 0; y < view.height; ++y) {
      for (int x = 0; x < view.width; ++x) {
        const uint8_t pv = getPixelValue(x, y);
        if (pv == 1 || pv == 2) {
          renderer.drawPixel(view.x + x, view.y + y, false);
        }
      }
    }
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();

    renderer.clearScreen();
    for (int y = 0; y < view.height; ++y) {
      for (int x = 0; x < view.width; ++x) {
        if (getPixelValue(x, y) >= 1) {
          renderer.drawPixel(view.x + x, view.y + y, true);
        }
      }
    }

    renderer.cleanupGrayscaleWithFrameBuffer();

    LOG_DBG("XTR", "Rendered page %lu/%lu (2-bit grayscale)", currentPage + 1, xtc->getPageCount());
    markPageRendered();
    return;
  } else {
    for (int y = 0; y < view.height; ++y) {
      for (int x = 0; x < view.width; ++x) {
        if (getPixelValue(x, y)) renderer.drawPixel(view.x + x, view.y + y, true);
      }
    }
  }

  if (SETTINGS.statusBarSpec().xtcMode == CrossPointSettings::XTC_STATUS_BAR_MODE::XTC_STATUS_BAR_TOP) {
    renderStatusBarOverlay(renderer, StatusBarOverlayPosition::Top);
  } else {
    renderStatusBarOverlay(renderer, StatusBarOverlayPosition::Bottom);
  }

  ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);

  LOG_DBG("XTR", "Rendered page %lu/%lu (%u-bit)", currentPage + 1, xtc->getPageCount(), bitDepth);
  markPageRendered();
}

bool XtcReaderActivity::pageTurn(bool isForward) {
  if (!xtc) return false;
  if (isForward) {
    if (currentPage < xtc->getPageCount()) {
      currentPage++;
      return true;
    }
  } else {
    if (currentPage > 0) {
      currentPage--;
      return true;
    }
  }
  return false;
}

bool XtcReaderActivity::skipPages(int amount) {
  if (!xtc) return false;
  int newPage = static_cast<int>(currentPage) + amount;
  if (newPage < 0) newPage = 0;
  if (newPage > static_cast<int>(xtc->getPageCount())) newPage = static_cast<int>(xtc->getPageCount());
  if (newPage != static_cast<int>(currentPage)) {
    currentPage = static_cast<uint32_t>(newPage);
    return true;
  }
  return false;
}

bool XtcReaderActivity::isAtEndOfBook() const { return xtc && currentPage >= xtc->getPageCount(); }

bool XtcReaderActivity::saveFinishedPosition() {
  if (!xtc || !xtc->getPageCount()) return false;
  const uint32_t page = xtc->getPageCount();
  uint8_t data[4];
  for (int n = 0; n < 4; ++n) data[n] = static_cast<uint8_t>(page >> (8 * n));
  if (!ProgressFile::writeAtomic(xtc->getCachePath(), data, sizeof(data))) return false;
  currentPage = lastSavedPage = page;
  pageSettings.reset();
  return true;
}

void XtcReaderActivity::onReturnFromEndOfBook() {
  if (xtc && xtc->getPageCount() > 0) {
    currentPage = xtc->getPageCount() - 1;
  } else {
    currentPage = 0;
  }
}

void XtcReaderActivity::saveProgress() const {
  if (!xtc || currentPage == lastSavedPage) return;
  uint8_t data[4];
  data[0] = currentPage & 0xFF;
  data[1] = (currentPage >> 8) & 0xFF;
  data[2] = (currentPage >> 16) & 0xFF;
  data[3] = (currentPage >> 24) & 0xFF;
  if (!ProgressFile::writeAtomic(xtc->getCachePath(), data, sizeof(data))) {
    LOG_ERR("XTC", "Failed to save progress: page %lu", currentPage);
  } else
    lastSavedPage = currentPage;
}

void XtcReaderActivity::loadProgress() {
  if (!xtc) return;
  HalFile f;
  if (Storage.openFileForRead("XTC", xtc->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    if (f.read(data, 4) == 4) {
      currentPage = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      if (currentPage > xtc->getPageCount() && xtc->getPageCount() > 0) {
        currentPage = xtc->getPageCount() - 1;
      }
      LOG_DBG("XTC", "Loaded progress: page %lu/%lu", currentPage + 1, xtc->getPageCount());
    }
  }
}

ScreenshotInfo XtcReaderActivity::getScreenshotInfo() const {
  ScreenshotInfo info;
  info.readerType = ScreenshotInfo::ReaderType::Xtc;
  if (xtc) {
    const std::string t = xtc->getTitle();
    snprintf(info.title, sizeof(info.title), "%s", t.c_str());
    const uint32_t pageCount = xtc->getPageCount();
    info.totalPages = pageCount;
    uint32_t clampedPage = (pageCount > 0 && currentPage >= pageCount) ? pageCount - 1 : currentPage;
    info.progressPercent = isAtEndOfBook() ? 100 : pageCount > 0 ? xtc->calculateProgress(clampedPage) : 0;
    info.currentPage = static_cast<int>(clampedPage) + 1;
  } else {
    info.currentPage = currentPage + 1;
  }
  return info;
}
