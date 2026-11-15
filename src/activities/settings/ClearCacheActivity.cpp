#include "ClearCacheActivity.h"

#include <Epub/css/CssParser.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "apps_local/home/CoverSceneCache.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BookCacheUtils.h"

void ClearCacheActivity::onEnter() {
  Activity::onEnter();

  state = WARNING;
  const char* options[] = {tr(STR_CANCEL), tr(STR_CLEAR_BUTTON)};
  confirmPopup.show(tr(STR_CLEAR_READING_CACHE), options, 2, 0, [this](int idx) {
    if (idx == 1) {
      beginClear();
    } else {
      goBack();
    }
  });
  requestUpdate();
}

void ClearCacheActivity::onExit() { Activity::onExit(); }

void ClearCacheActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  pixelui::drawHeader(renderer, tr(STR_CLEAR_READING_CACHE), I18N.getLanguage() != Language::EN);

  if (state == WARNING) {
    static constexpr StrId warnings[] = {StrId::STR_CLEAR_CACHE_WARNING_1, StrId::STR_CLEAR_CACHE_WARNING_2,
                                         StrId::STR_CLEAR_CACHE_WARNING_3, StrId::STR_CLEAR_CACHE_WARNING_4};
    const int rowHeight = 2 * renderer.getLineHeight(UI_10_FONT_ID);
    const int top = std::max(UITheme::getContentArea(renderer).y, (pageHeight - 4 * rowHeight - 24) / 2);
    for (int row = 0; row < 4; ++row)
      UITheme::drawCenteredWrappedText(renderer,
                                       Rect{metrics.contentSidePadding, top + row * (rowHeight + 8),
                                            pageWidth - 2 * metrics.contentSidePadding, rowHeight},
                                       UI_10_FONT_ID, I18N.get(warnings[row]), 2, true,
                                       row == 1 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);

    if (confirmPopup.processRender(renderer, mappedInput)) return;

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_CLEAR_BUTTON), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == CLEARING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_CLEARING_CACHE));
    renderer.displayBuffer();
    return;
  }

  if (state == SUCCESS) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CACHE_CLEARED), true, EpdFontFamily::BOLD);
    std::string resultText = std::to_string(clearedCount) + " " + std::string(tr(STR_ITEMS_REMOVED));
    if (failedCount > 0) {
      resultText += ", " + std::to_string(failedCount) + " " + std::string(tr(STR_FAILED_LOWER));
    }
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, resultText.c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CLEAR_CACHE_FAILED), true,
                              EpdFontFamily::BOLD);
    UITheme::drawCenteredWrappedText(
        renderer,
        Rect{metrics.contentSidePadding, pageHeight / 2 + 10, pageWidth - 2 * metrics.contentSidePadding,
             3 * renderer.getLineHeight(UI_10_FONT_ID)},
        UI_10_FONT_ID, tr(STR_CHECK_SERIAL_OUTPUT), 3, true, EpdFontFamily::REGULAR,
        UITheme::TextVerticalAlignment::TOP);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}

void ClearCacheActivity::beginClear() {
  LOG_DBG("CLEAR_CACHE", "User confirmed, starting cache clear");
  {
    RenderLock lock(*this);
    state = CLEARING;
  }
  requestUpdateAndWait();
  clearCache();
}

void ClearCacheActivity::clearCache() {
  homecover::invalidateScenes();
  CssParser::invalidateValidationMemo();
  LOG_DBG("CLEAR_CACHE", "Clearing cache...");

  // Open .crosspoint directory
  auto root = Storage.open("/.crosspoint");
  if (!root || !root.isDirectory()) {
    LOG_DBG("CLEAR_CACHE", "Failed to open cache directory");
    if (root) root.close();
    state = FAILED;
    requestUpdate();
    return;
  }

  clearedCount = 0;
  failedCount = 0;
  char name[128];

  // Iterate through all entries in the directory
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    String itemName(name);

    // Only delete directories matching known book cache names.
    if (file.isDirectory() && isBookCacheDirectoryName(itemName.c_str())) {
      String fullPath = "/.crosspoint/" + itemName;
      LOG_DBG("CLEAR_CACHE", "Removing cache: %s", fullPath.c_str());

      file.close();  // Close before attempting to delete

      if (Storage.removeDir(fullPath.c_str())) {
        clearedCount++;
      } else {
        LOG_ERR("CLEAR_CACHE", "Failed to remove: %s", fullPath.c_str());
        failedCount++;
      }
    } else {
      file.close();
    }
  }
  root.close();

  LOG_DBG("CLEAR_CACHE", "Cache cleared: %d removed, %d failed", clearedCount, failedCount);

  state = SUCCESS;
  requestUpdate();
}

void ClearCacheActivity::loop() {
  if (state == WARNING) {
    if (confirmPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      beginClear();
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      LOG_DBG("CLEAR_CACHE", "User cancelled");
      goBack();
    }
    return;
  }

  if (state == SUCCESS || state == FAILED) {
    int x = 0;
    int y = 0;
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasScreenTapped(x, y)) {
      goBack();
    }
    return;
  }
}
