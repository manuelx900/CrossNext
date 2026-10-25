#include "EpubReaderClippingListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>

#include "ClippingStore.h"
#include "DictionaryDefinitionActivity.h"
#include "MappedInputManager.h"
#include "apps_local/readertext/ReaderSavedScreens.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "clippings/ClippingPreview.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {

constexpr int ENTER_DELETE_MODE_MS = 700;

}  // namespace

EpubReaderClippingListActivity::EpubReaderClippingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                               const std::shared_ptr<Epub>& book)
    : UiListActivity("EpubReaderClippings", renderer, mappedInput, /*wantsTouchLongPress=*/true), book(book) {}

void EpubReaderClippingListActivity::onEnter() {
  UiListActivity::onEnter();
  app.on(ACTION_USER, &EpubReaderClippingListActivity::onSavedAction, this);
  initialListRender = true;
  rebuildRows();
}

int EpubReaderClippingListActivity::listCount() const { return static_cast<int>(CLIPPINGS.clippingCount()); }

const char* EpubReaderClippingListActivity::headerTitle() const { return tr(STR_VIEW_CLIPPINGS); }

void EpubReaderClippingListActivity::rebuildRows() {
  windowStart = -1;
  windowCount = 0;
}

void EpubReaderClippingListActivity::refreshRowWindow(const int start) {
  const int total = listCount();
  int clamped = std::max(0, start);
  if (clamped > total - ROW_WINDOW) clamped = std::max(0, total - ROW_WINDOW);
  if (clamped == windowStart) return;

  windowCount = std::min(ROW_WINDOW, total - clamped);
  for (int slot = 0; slot < windowCount; ++slot) {
    const size_t clippingIndex = static_cast<size_t>(clamped + slot);
    const Clipping* clipping = CLIPPINGS.clippingAt(clippingIndex);
    previewLengths[slot] = 0;
    previews[slot][0] = '\0';
    if (!clipping || !CLIPPINGS.readClippingPreview(clippingIndex, previews[slot].data(), previews[slot].size(),
                                                    previewLengths[slot])) {
      LOG_ERR("CLIP", "Failed to read clipping %u", static_cast<unsigned>(clippingIndex));
    }

    fui::ListItem item;
    item.label = previews[slot].data();
    if (clipping)
      std::snprintf(positions[slot].data(), positions[slot].size(), "%s · %u / %u", clipping->chapterTitle,
                    clipping->startPage + 1, clipping->pageCount);
    else
      positions[slot][0] = 0;
    item.subtitle = positions[slot].data();
    item.value = ">";
    item.icon = listIconFor(UIIcon::Bookmark, 32);
    item.actionValue = static_cast<int16_t>(clippingIndex);
    rowItems[slot] = item;
  }
  windowStart = clamped;
}

void EpubReaderClippingListActivity::openSelected() {
  if (confirmPopup.isActive() || nav.selected < 0 || nav.selected >= listCount()) return;
  const Clipping* clipping = CLIPPINGS.clippingAt(static_cast<size_t>(nav.selected));
  if (!clipping) return;
  const bool anchored = clipping->startOffset != UINT32_MAX && clipping->endOffset != UINT32_MAX;
  if (!anchored) {
    readSelected();
    return;
  }
  ProgressChangeResult result;
  result.spineIndex = clipping->spineIndex;
  result.page = clipping->startPage;
  result.hasVisibleTextOffset = true;
  result.visibleTextOffset = clipping->startOffset;
  setResult(std::move(result));
  finish();
}

void EpubReaderClippingListActivity::readSelected() {
  if (nav.selected < 0 || nav.selected >= listCount()) return;
  const Clipping* clipping = CLIPPINGS.clippingAt(static_cast<size_t>(nav.selected));
  std::string text;
  if (!clipping || !CLIPPINGS.readClippingText(static_cast<size_t>(nav.selected), text)) return;
  const std::string title = clipping->chapterTitle[0] != '\0' ? clipping->chapterTitle : tr(STR_CLIPPINGS);
  auto detail = makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, title, std::move(text));
  if (!detail) {
    LOG_ERR("CLIP", "Failed to allocate clipping detail activity");
    return;
  }
  startActivityForResult(std::move(detail), [this](const ActivityResult&) { requestUpdate(); });
}

void EpubReaderClippingListActivity::activateIndex(const int index) {
  if (confirmPopup.isActive() || index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  openSelected();
}

void EpubReaderClippingListActivity::onRowLongPress(const int index) {
  if (confirmPopup.isActive() || index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  showDeleteConfirmation();
}

void EpubReaderClippingListActivity::showDeleteConfirmation() {
  if (listCount() == 0 || confirmPopup.isActive()) return;
  confirmingDelete = true;
  const char* options[] = {tr(STR_CANCEL), tr(STR_DELETE)};
  confirmPopup.show(tr(STR_DELETE_CLIPPING_CONFIRM), options, 2, 0, [this](const int index) {
    confirmingDelete = false;
    if (index == 1) deleteSelected();
    requestUpdate();
  });
  requestUpdate();
}

void EpubReaderClippingListActivity::deleteSelected() {
  if (nav.selected < 0 || nav.selected >= listCount()) return;
  if (!CLIPPINGS.removeClippingAt(static_cast<size_t>(nav.selected))) {
    LOG_ERR("CLIP", "Failed to delete clipping %d", static_cast<int>(nav.selected));
    return;
  }
  rebuildRows();
  if (listCount() == 0) {
    finish();
    return;
  }
  if (nav.selected >= listCount()) nav.selected = listCount() - 1;
  nav.follow(listCount());
}

bool EpubReaderClippingListActivity::handleCustomInput() {
  if (confirmPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return true;
  if (confirmingDelete) {
    confirmingDelete = false;
    requestUpdate();
    return true;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    moveSelectionTo(readersaved::destination(nav.selected, listCount(), savedPageRows,
                                             swipe == MappedInputManager::SwipeDir::Up ? 1 : -1));
    return true;
  }
  return false;
}

bool EpubReaderClippingListActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (mappedInput.getHeldTime() > ENTER_DELETE_MODE_MS) {
      showDeleteConfirmation();
    } else {
      openSelected();
    }
    return true;
  }
  return false;
}

void EpubReaderClippingListActivity::drawChrome() {
  pixelui::drawHeader(renderer, tr(STR_VIEW_CLIPPINGS), I18N.getLanguage() != Language::EN);
}
void EpubReaderClippingListActivity::onSavedAction(const fui::ActionEvent& event, void* user) {
  auto& self = *static_cast<EpubReaderClippingListActivity*>(user);
  if (self.confirmPopup.isActive()) return;
  self.app.clearTapFlash();
  if (event.value == readersaved::Done) {
    ActivityResult result;
    result.isCancelled = true;
    self.setResult(std::move(result));
    self.finish();
    return;
  }
  self.moveSelectionTo(readersaved::destination(self.nav.selected, self.listCount(), self.savedPageRows,
                                                event.value == readersaved::Previous ? -1 : 1));
}

void EpubReaderClippingListActivity::navigateButtons() {
  buttonNavigator.onNextPress([this] { moveSelectionTo(ButtonNavigator::nextIndex(nav.selected, listCount())); });
  buttonNavigator.onPreviousPress(
      [this] { moveSelectionTo(ButtonNavigator::previousIndex(nav.selected, listCount())); });
  buttonNavigator.onNextContinuous(
      [this] { moveSelectionTo(readersaved::destination(nav.selected, listCount(), savedPageRows, 1)); });
  buttonNavigator.onPreviousContinuous(
      [this] { moveSelectionTo(readersaved::destination(nav.selected, listCount(), savedPageRows, -1)); });
}

void EpubReaderClippingListActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  uiTarget.setFont(4, UI_12_FONT_ID);
  uiTarget.setFont(3, UI_10_FONT_ID);
  readersaved::begin(screen, book ? book->getTitle().c_str() : "", book ? book->getAuthor().c_str() : "", listCount(),
                     true);
  const auto footer = readersaved::reserveFooter(screen);
  if (!listCount()) {
    screen.centeredText(tr(STR_NO_CLIPPINGS), screen.theme().bodyText);
    readersaved::footer(screen, savedButton, footer, 1, 1, ACTION_USER);
    return;
  }
  static fui::ListProps props;
  props = {};
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  readersaved::rows(props);
  const auto page = readersaved::project(nav.selected, listCount(), screen.body().height);
  savedPageRows = page.capacity;
  readersaved::slice(props, nav, page);
  refreshRowWindow(page.first);
  props.items = rowItems.data();
  props.itemsWindowFirst = static_cast<uint16_t>(windowStart);
  props.itemsWindowCount = static_cast<uint16_t>(windowCount);
  props.partialTrailingRow = false;
  screen.list(props);
  fui::TextStyle hint;
  hint.font = 3;
  hint.maxLines = 2;
  const auto body = screen.body();
  auto hintBox = footer.hint;
  hintBox.y =
      std::min(int(hintBox.y), body.y + (page.end - page.first) * (readersaved::RowHeight + readersaved::RowGap) + 4);
  if (!hintBox.empty()) screen.target().text(hintBox, tr(STR_SR_QUOTE_HINT), hint);
  readersaved::footer(screen, savedButton, footer, page.number, page.total, ACTION_USER);
}

void EpubReaderClippingListActivity::render(RenderLock&& lock) {
  if (initialListRender && listCount() > 0) {
    renderer.clearScreen();
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  }
  initialListRender = false;
  UiListActivity::render(std::move(lock));
  if (confirmPopup.processRender(renderer, mappedInput)) return;
}
