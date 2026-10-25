#include "EpubReaderBookmarksActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>

#include "../../util/BookmarkFile.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "apps_local/readertext/ReaderSavedScreens.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr int ENTER_ACTIONS_MODE_MS = 700;
}  // namespace

EpubReaderBookmarksActivity::EpubReaderBookmarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                         const std::shared_ptr<Epub>& epub, const std::string& epubPath)
    : UiListActivity("EpubReaderBookmarks", renderer, mappedInput, /*wantsTouchLongPress=*/true),
      epub(epub),
      epubPath(epubPath) {}

void EpubReaderBookmarksActivity::onEnter() {
  UiListActivity::onEnter();
  app.on(ACTION_USER, &EpubReaderBookmarksActivity::onSavedAction, this);

  if (!epub) {
    return;
  }

  if (!BookmarkFile::load(epubPath, bookmarks)) {
    bookmarks.shrink_to_fit();
  }
  LOG_DBG("EPB", "Loaded %d bookmarks for book: %s", static_cast<int>(bookmarks.size()), epubPath.c_str());
  rebuildBookmarkRowItems();
}

// Derives bookmarkSubtitles/bookmarkRowItems from `bookmarks`. Called
// whenever `bookmarks` changes (onEnter() load, post-delete) so buildScreen()
// reuses the cached rows on every repaint instead of re-composing a
// percentage/chapter/TOC-title subtitle string per bookmark each time.
void EpubReaderBookmarksActivity::rebuildBookmarkRowItems() {
  bookmarkSubtitles.clear();
  bookmarkRowItems.clear();
  if (!epub) {
    return;
  }
  bookmarkSubtitles.reserve(bookmarks.size());
  bookmarkRowItems.reserve(bookmarks.size());
  for (const auto& bookmark : bookmarks) {
    const auto tocIndex = epub->getTocIndexForSpineIndex(bookmark.computedSpineIndex);
    const auto tocTitle = (tocIndex >= 0) ? epub->getTocItem(tocIndex).title : tr(STR_UNNAMED);
    std::string subtitle = std::to_string((int)(std::clamp(bookmark.percentage, 0.0f, 1.0f) * 100.0f + 0.5f)) + "% - ";
    if (bookmark.computedChapterPageCount > 0) {
      subtitle += std::to_string(bookmark.computedChapterProgress + 1) + "/" +
                  std::to_string(bookmark.computedChapterPageCount) + " - ";
    }
    subtitle += tocTitle;
    bookmarkSubtitles.push_back(std::move(subtitle));

    fui::ListItem item;
    item.label = bookmark.name.empty() ? bookmark.summary.c_str() : bookmark.name.c_str();
    item.subtitle = bookmarkSubtitles.back().c_str();
    item.icon = listIconFor(UIIcon::Bookmark, 32);  // subtitle rows carry the larger icon
    item.value = ">";
    item.actionValue = static_cast<int16_t>(bookmarkRowItems.size());
    bookmarkRowItems.push_back(item);
  }
}

void EpubReaderBookmarksActivity::openSelectedBookmark() {
  if (bookmarks.empty()) {
    return;
  }
  const auto& bookmark = bookmarks.at(nav.selected);
  ProgressChangeResult result{};
  result.xpath = bookmark.xpath;
  result.percentage = bookmark.percentage;
  result.hasSavedProgress = true;
  result.hasVisibleTextOffset = bookmark.hasVisibleTextOffset;
  result.visibleTextOffset = bookmark.visibleTextOffset;
  // The offset is spine-relative, so carry its spine even when the legacy page hints below
  // are stale. The reader validates the index.
  result.spineIndex = bookmark.computedSpineIndex;
  if (bookmark.computedChapterPageCount > 0 && bookmark.computedChapterProgress < bookmark.computedChapterPageCount &&
      bookmark.computedSpineIndex < epub->getSpineItemsCount()) {
    result.page = bookmark.computedChapterProgress;
    result.totalPages = bookmark.computedChapterPageCount;
  }
  setResult(std::move(result));
  finish();
}

void EpubReaderBookmarksActivity::activateIndex(const int index) {
  if (confirmPopup.isActive()) return;
  // The interaction table can deliver a row index captured before a delete
  // shrank the list; the next render re-registers the rows.
  if (index < 0 || index >= listCount()) return;
  // The tapped row leaves this screen; a lingering flash would gray an
  // unrelated row on the next render.
  app.clearTapFlash();
  nav.selected = index;
  openSelectedBookmark();
}

void EpubReaderBookmarksActivity::onRowLongPress(const int index) {
  if (confirmPopup.isActive()) return;
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  showBookmarkActions();
}

bool EpubReaderBookmarksActivity::handleCustomInput() {
  // Bookmark action or delete confirmation popup.
  if (confirmPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return true;
  if (confirmingDelete) {
    // Popup dismissed without a selection (Back button or tap outside): cancel delete
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

bool EpubReaderBookmarksActivity::handleButtons() {
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, ENTER_ACTIONS_MODE_MS)) {
    showBookmarkActions();
    return true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openSelectedBookmark();
    return true;
  }

  return false;
}

void EpubReaderBookmarksActivity::startRename() {
  if (bookmarks.empty() || nav.selected < 0 || nav.selected >= listCount()) {
    return;
  }

  app.clearTapFlash();
  const int renameIndex = nav.selected;
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_RENAME), bookmarks[renameIndex].name,
                                               BookmarkEntry::MAX_NAME_LENGTH, InputType::Text);
  if (!keyboard) {
    LOG_ERR("EPB", "OOM: bookmark rename keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this, renameIndex](const ActivityResult& result) {
    if (result.isCancelled || renameIndex < 0 || renameIndex >= listCount()) {
      return;
    }
    std::string previousName = std::move(bookmarks[renameIndex].name);
    bookmarks[renameIndex].name = std::get<KeyboardResult>(result.data).text;
    rebuildBookmarkRowItems();
    if (!BookmarkFile::save(epubPath, bookmarks)) {
      LOG_ERR("EPB", "Failed to save bookmarks after rename");
      bookmarks[renameIndex].name = std::move(previousName);
      rebuildBookmarkRowItems();
    }
    requestUpdate();
  });
}

void EpubReaderBookmarksActivity::showBookmarkActions() {
  if (bookmarks.empty() || confirmPopup.isActive()) {
    return;
  }
  const StrId options[] = {StrId::STR_OPEN, StrId::STR_RENAME, StrId::STR_DELETE};
  confirmPopup.show(StrId::STR_BOOKMARKS, options, 3, 0, [this](const int idx) {
    if (idx == 0) {
      openSelectedBookmark();
    } else if (idx == 1) {
      startRename();
    } else if (idx == 2) {
      showDeleteConfirmation();
    }
  });
  requestUpdate();
}

void EpubReaderBookmarksActivity::showDeleteConfirmation() {
  if (bookmarks.empty() || confirmPopup.isActive()) {
    return;
  }
  confirmingDelete = true;
  const char* options[] = {tr(STR_CANCEL), tr(STR_DELETE)};
  confirmPopup.show(tr(STR_CONFIRM_DELETE_BOOKMARK), options, 2, 0, [this](int idx) {
    confirmingDelete = false;
    if (idx == 1) {
      deleteSelectedBookmark();
    }
    requestUpdate();
  });
  requestUpdate();
}

void EpubReaderBookmarksActivity::deleteSelectedBookmark() {
  bookmarks.erase(bookmarks.begin() + nav.selected);
  // Deleting shifts every later bookmark's index, so the cached subtitles and
  // actionValues must be re-derived, not just trimmed — and before the SD
  // save, so the render task never sees rows aliasing the erased storage.
  rebuildBookmarkRowItems();
  if (!BookmarkFile::save(epubPath, bookmarks)) {
    LOG_ERR("EPB", "Failed to save bookmarks after delete");
  }

  // Move selector up if we deleted the last item
  if (nav.selected >= static_cast<int>(bookmarks.size()) && nav.selected > 0) {
    nav.selected--;
  }

  if (bookmarks.empty()) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  nav.follow(listCount());
  requestUpdate();
}

void EpubReaderBookmarksActivity::onSavedAction(const fui::ActionEvent& event, void* user) {
  auto& self = *static_cast<EpubReaderBookmarksActivity*>(user);
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

void EpubReaderBookmarksActivity::navigateButtons() {
  buttonNavigator.onNextPress([this] { moveSelectionTo(ButtonNavigator::nextIndex(nav.selected, listCount())); });
  buttonNavigator.onPreviousPress(
      [this] { moveSelectionTo(ButtonNavigator::previousIndex(nav.selected, listCount())); });
  buttonNavigator.onNextContinuous(
      [this] { moveSelectionTo(readersaved::destination(nav.selected, listCount(), savedPageRows, 1)); });
  buttonNavigator.onPreviousContinuous(
      [this] { moveSelectionTo(readersaved::destination(nav.selected, listCount(), savedPageRows, -1)); });
}

void EpubReaderBookmarksActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  uiTarget.setFont(4, UI_12_FONT_ID);
  uiTarget.setFont(3, UI_10_FONT_ID);
  readersaved::begin(screen, epub ? epub->getTitle().c_str() : "", epub ? epub->getAuthor().c_str() : "", listCount(),
                     false);
  const auto footer = readersaved::reserveFooter(screen);
  if (bookmarks.empty()) {
    screen.centeredText(tr(STR_NO_BOOKMARKS), screen.theme().bodyText);
    readersaved::footer(screen, savedButton, footer, 1, 1, ACTION_USER);
    return;
  }
  static fui::ListProps props;
  props = {};
  props.items = bookmarkRowItems.data();
  props.count = static_cast<uint16_t>(bookmarkRowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  readersaved::rows(props);
  const auto page = readersaved::project(nav.selected, listCount(), screen.body().height);
  savedPageRows = page.capacity;
  readersaved::slice(props, nav, page);
  props.partialTrailingRow = false;
  screen.list(props);
  fui::TextStyle hint;
  hint.font = 3;
  hint.maxLines = 2;
  const auto body = screen.body();
  auto hintBox = footer.hint;
  hintBox.y =
      std::min(int(hintBox.y), body.y + (page.end - page.first) * (readersaved::RowHeight + readersaved::RowGap) + 4);
  if (!hintBox.empty()) screen.target().text(hintBox, tr(STR_SR_BOOKMARK_HINT), hint);
  readersaved::footer(screen, savedButton, footer, page.number, page.total, ACTION_USER);
}

void EpubReaderBookmarksActivity::render(RenderLock&&) {
  renderer.clearScreen();
  pixelui::drawHeader(renderer, tr(STR_BOOKMARKS), I18N.getLanguage() != Language::EN);
  renderUi();
  if (confirmPopup.processRender(renderer, mappedInput)) return;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
