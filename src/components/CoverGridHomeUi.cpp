#include "CoverGridHomeUi.h"

#include <BoardConfig.h>
#include <ChromeRegion.h>
#include <FreeInkUIIcon.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <KOReaderDocumentId.h>
#include <Logging.h>
#include <Memory.h>
#include <RecentBooksStore.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <utility>

#include "../apps_local/Shelf.h"  // fork-local seam
#include "../apps_local/home/CoverSceneCache.h"
#include "../apps_local/home/HomeMenuIntegration.h"
#include "../apps_local/home/HomeScreen.h"
#include "../apps_local/home/LicoriceLayout.h"
#include "../apps_local/reading/ReadingStatsStore.h"
#include "../apps_local/ui/PixelText.h"
#include "../apps_local/ui/ToyboxFonts.h"
#include "../util/DateFormat.h"
#include "MappedInputManager.h"
#include "UITheme.h"
#include "icons/blocks.h"
#include "icons/book.h"
#include "icons/folder.h"
#include "icons/library.h"
#include "icons/settings2.h"
#include "icons/shelfIcons.h"  // fork-local seam
#include "icons/transfer.h"
#include "util/BookProgress.h"

namespace fui = freeink::ui;
namespace {
constexpr fui::ActionId SELECT = 1;
// Grid cell padding around each cover; also feeds the screen's horizontal
// inset so the cover columns land on the header chrome's inset line.
constexpr int16_t COVER_CELL_INSET = 6;
constexpr int COVER_ROW_EXTRA_HEIGHT = 8;
void softCoverOutline(fui::DrawTarget& target, fui::Rect art) {
  const auto ink = fui::Paint::solid(fui::Color::Black), paper = fui::Paint::solid(fui::Color::White);
  static constexpr int cuts[] = {5, 3, 2, 1, 1};
  for (int row = 0; row < 5; ++row) {
    const int cut = cuts[row];
    for (int y : {int(art.y) + row, int(art.bottom()) - 1 - row}) {
      target.fill(fui::makeRect(art.x, y, cut, 1), paper);
      target.fill(fui::makeRect(art.right() - cut, y, cut, 1), paper);
      target.fill(fui::makeRect(art.x + cut, y, 1, 1), ink);
      target.fill(fui::makeRect(art.right() - cut - 1, y, 1, 1), ink);
    }
  }
}
void coverBadge(fui::DrawTarget& target, fui::Rect rect) {
  const auto ink = fui::Paint::solid(fui::Color::Black);
  static constexpr int cuts[] = {4, 2, 1, 1};
  target.fill(fui::makeRect(rect.x, rect.y + 4, rect.width, rect.height - 8), ink);
  for (int row = 0; row < 4; ++row) {
    const int cut = cuts[row];
    target.fill(fui::makeRect(rect.x + cut, rect.y + row, rect.width - cut * 2, 1), ink);
    target.fill(fui::makeRect(rect.x + cut, rect.bottom() - 1 - row, rect.width - cut * 2, 1), ink);
  }
}
}  // namespace

CoverGridHomeUi::CoverGridHomeUi(GfxRenderer& renderer)
    : UiAppHost(renderer), coverCache(renderer), renderer(renderer) {}

void CoverGridHomeUi::begin(const std::vector<RecentBook>& recent, bool opds, bool continuing, bool plugins) {
  hasPlugins = plugins;
  books = &recent;
  if (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE)
    noteThumbHeight(homeui::shelfLayout(uiTarget.deviceContext()).artHeight);
  hasOpds = opds;
  hasContinueReading = continuing;
  if (!recent.empty()) coverCache.begin(SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE);
  resetUi();
  app.on(SELECT, &CoverGridHomeUi::onAction, this);
  app.setScreen(&CoverGridHomeUi::screenFn, this);
  refreshCoverPaths();
  if (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE && !readingTimeLoaded && !recent.empty()) {
    readingTimeLoaded = true;
    struct ReadingSnapshot {
      readingstats::Repository repository;
      readingstats::Book book;
    };
    // Repository scratch and Book exceed the render-task stack budget.
    auto snapshot = makeUniqueNoThrow<ReadingSnapshot>();
    if (snapshot) {
      const auto document = KOReaderDocumentId::calculate(recent.front().path);
      readingTimeKnown = snapshot->repository.cachedBook(document.c_str(), snapshot->book);
      if (readingTimeKnown)
        readingSeconds = snapshot->book.seconds;
      else
        readingTimeKnown = !document.empty() && !Storage.exists(readingstats::kJournalPath);
    } else
      LOG_ERR("HOME", "OOM: featured reading snapshot");
  }
  if (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE) {
    // Eighteen shelf slots plus the featured book, PSRAM-only and activity-owned.
    if (!progressMemory) {
      progressMemory = HalMemory::allocatePsram(PROGRESS_RESULTS * sizeof(ProgressResult));
      if (!progressMemory)
        LOG_ERR("HOME", "OOM: %u-byte shelf progress cache", unsigned(PROGRESS_RESULTS * sizeof(ProgressResult)));
    }
  }
  progress = hasContinueReading && !books->empty()
                 ? (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE ? shelfProgress(books->front())
                                                                             : loadBookProgress(books->front().path))
                 : -1;
  if (progress >= 0) snprintf(progressText, sizeof(progressText), "%d%%", progress);
  if (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE) {
    for (size_t i = 0; i < recent.size() && i < MAX_BOOKS; ++i)
      bookProgress[i] = i == 0 ? progress : shelfProgress(recent[i]);
  }
}

int CoverGridHomeUi::shelfProgress(const RecentBook& book) {
  auto* results = reinterpret_cast<ProgressResult*>(progressMemory.get());
  for (int n = 0; results && n < progressCount; ++n)
    if (!std::strcmp(results[n].path, book.path.c_str())) return results[n].percent;
  int percent = loadBookProgress(book.path);
  if (percent < 0) {
    const auto& history = RECENT_BOOKS.getBooks();
    if (std::any_of(history.begin(), history.end(), [&](const RecentBook& recent) { return recent.path == book.path; }))
      percent = 0;
  }
  if (results && progressCount < PROGRESS_RESULTS && book.path.size() < sizeof(results[0].path)) {
    auto& result = results[progressCount++];
    std::memcpy(result.path, book.path.c_str(), book.path.size() + 1);
    result.percent = percent;
  }
  return percent;
}

void CoverGridHomeUi::refreshCoverPaths() {
  coverCache.invalidate();
  for (size_t i = 0; i < books->size() && i < coverPaths.size(); ++i) refreshCoverPath(i);
}

void CoverGridHomeUi::refreshCoverPath(size_t index) {
  if (index >= books->size() || index >= coverPaths.size()) return;
  coverCache.invalidate(index);
  coverPaths[index] =
      thumbHeight > 0 ? UITheme::getCoverThumbPath((*books)[index].coverBmpPath, thumbHeight) : std::string();
}

int CoverGridHomeUi::thumbHeightFor() const { return thumbHeight > 0 ? thumbHeight : THUMB_HEIGHT; }

bool CoverGridHomeUi::takeThumbHeightChanged() { return std::exchange(thumbHeightChanged, false); }

void CoverGridHomeUi::noteThumbHeight(int slotHeight) {
  // One shared height for every slot (hero and grid covers are the same
  // size). The +8 surplus gives the paint clip a little bleed so the art's
  // rightward nudge never exposes the left edge; covers narrower than the
  // 2:3 slot may show a thin sliver at the sides rather than being cropped.
  const int height = std::max(1, slotHeight + 8);
  if (thumbHeight != height) {
    thumbHeight = height;
    thumbHeightChanged = true;
    refreshCoverPaths();
  }
}

void CoverGridHomeUi::onAction(const fui::ActionEvent& event, void* user) {
  auto& self = *static_cast<CoverGridHomeUi*>(user);
  self.pending = event.value;
  self.app.clearTapFlash();
}

int CoverGridHomeUi::selectedAction(const MappedInputManager& input) {
  pending = -1;
  const auto touch = routeTouch(input);
  if (touch.snap.touchReleased && pending == MENU_TOGGLE) {
    drawer = !drawer;
    return MENU_TOGGLE;
  }
  return touch.snap.touchReleased ? pending : -1;
}

void CoverGridHomeUi::screenFn(UiScreen& screen, void* user) { static_cast<CoverGridHomeUi*>(user)->draw(screen); }

void CoverGridHomeUi::draw(UiScreen& screen) {
  if (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE) {
    drawLicorice(screen);
    return;
  }
  coverCache.prepare();
  const auto& theme = screen.theme();
  const auto safe = UITheme::getInstance().getScreenSafeArea(renderer, true);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y), static_cast<int16_t>(renderer.getScreenWidth() - safe.x - safe.width),
      static_cast<int16_t>(renderer.getScreenHeight() - safe.y - safe.height), static_cast<int16_t>(safe.x)});
  // Horizontal inset sized so the outer cover columns (content x plus the
  // grid's cell inset) sit on the theme's side-padding line: the heading and
  // tabs then align with the clock and battery, which tuck a few px further
  // in (headerStatusInset's optical bias).
  const int16_t hInset = std::max<int16_t>(
      0, static_cast<int16_t>(UITheme::getInstance().getMetrics().headerSidePadding - COVER_CELL_INSET - safe.x));
  // Drop the status band, hero, and grid down: touch boards get a full step
  // (no button-hint band to fit), button boards a modest nudge. The tabs stay
  // put (bottom-anchored below).
  const int16_t topInset =
      BoardConfig::hasTouch() ? static_cast<int16_t>(theme.spaceLg) : static_cast<int16_t>(theme.spaceSm + 4);
  screen.insetContent(fui::Insets{topInset, hInset, theme.spaceSm, hInset});
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  // Reserve the band's slot in the flow (its content draws at a fixed screen
  // position in drawHeaderBand); the slot doubles as padding above the heading.
  screen.takeTop(UITheme::getInstance().getMetrics().batteryBarHeight, theme.spaceSm);
  // Button boards run tighter above the tabs to make room for the top step.
  const int16_t tabGap = BoardConfig::hasTouch() ? theme.spaceSm : static_cast<int16_t>(4);
  auto tabRect = screen.takeBottom(UITheme::getInstance().getMetrics().coverGridTabBarHeight, tabGap);
  if (books->empty()) {
    drawTabs(screen, tabRect.inset(fui::Insets{0, COVER_CELL_INSET, 0, COVER_CELL_INSET}));
    drawEmpty(screen);
    drawHeaderBand();
    return;
  }
  // Hero cover matches the grid covers: the body splits into three equal
  // cover rows (hero + two grid rows), so the featured section is one row
  // tall and every cover on screen shares one size. The metadata floor still
  // applies when the rows would be too short for the hero's text lines.
  const fui::Rect body = screen.body();
  const int rowGap = std::max<int>(4, body.width / 100);
  grid.gap = grid.rowGap = rowGap;
  const int coverRowHeight = std::max(1, (body.height - theme.spaceSm - 2 * rowGap) / 3 + COVER_ROW_EXTRA_HEIGHT);
  const int16_t featuredHeight = std::min<int>(
      body.height, std::max<int>(coverRowHeight, screen.target().lineHeight(theme.bodyText.font) * (landscape ? 1 : 2) +
                                                     screen.target().lineHeight(theme.smallText.font) * 2 + 32));
  drawCurrent(screen, screen.takeTop(featuredHeight, theme.spaceSm), coverRowHeight);
  drawGrid(screen);
  tabRect.x = gridBounds.x + grid.cellInset.left;
  tabRect.width = gridBounds.width - grid.cellInset.left - grid.cellInset.right;
  drawTabs(screen, tabRect);
  drawHeaderBand();
}

void CoverGridHomeUi::drawLicorice(UiScreen& screen) {
  coverCache.prepare();
  const auto safe = screen.device().safeRect();
  const bool wide = screen.device().width > screen.device().height;
  const int band = safe.y + 44;
  chrome::headerHeight = band;
  GUI.applyHeaderStatus(renderer, shelfStatus);
  auto& target = screen.target();
  const auto ink = fui::Paint::solid(fui::Color::Black), paper = fui::Paint::solid(fui::Color::White);
  const auto label = [&target](fui::Rect r, const char* value, int font = 0, bool inverted = false,
                               fui::TextAlign align = fui::TextAlign::Left) {
    fui::TextStyle style;
    style.font = font;
    style.align = align;
    style.maxLines = 1;
    style.color = inverted ? fui::Color::White : fui::Color::Black;
    target.text(r, value, style);
  };
  target.fill(fui::makeRect(0, 0, screen.device().width, band), ink);
  const auto battery = fui::makeRect(safe.right() - 40, safe.y + 14, 26, 16);
  target.stroke(battery, paper, 1);
  target.fill(fui::makeRect(battery.right(), battery.y + 4, 3, 8), paper);
  target.fill(fui::makeRect(battery.x + 3, battery.y + 3,
                            20 * std::clamp(int(shelfStatus.status.battery.percent), 0, 100) / 100, 10),
              paper);
  if (shelfStatus.status.battery.label) {
    char value[8];
    std::snprintf(value, sizeof(value), "%d%%", int(shelfStatus.status.battery.percent));
    label(fui::makeRect(battery.x - 60, safe.y, 56, 44), value, 0, true, fui::TextAlign::Right);
  }
  struct tm local{};
  const time_t now = std::time(nullptr);
  if ((now > 0 && localtime_r(&now, &local) && local.tm_year >= 120) || halClock.localTime(local)) {
    char clock[16];
    if (SETTINGS.clockFormat == 1)
      std::snprintf(clock, sizeof(clock), "%d:%02d %s", (local.tm_hour + 11) % 12 + 1, local.tm_min,
                    local.tm_hour < 12 ? tr(STR_CLOCK_AM) : tr(STR_CLOCK_PM));
    else
      std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
    label(fui::makeRect(safe.x + safe.width / 2 - 66, safe.y, 132, 44), clock, 0, true, fui::TextAlign::Center);
    char date[32];
    dateformat::format(date, sizeof(date), local.tm_mday, local.tm_mon + 1, local.tm_year + 1900,
                       dateformat::fromStored(SETTINGS.dateFormat));
    label(fui::makeRect(safe.x + 14, safe.y, safe.width / 2 - 88, 44), date, 3, true);
  }
  const auto menuButton = fui::makeRect(safe.right() - 54, band + 6, 44, 40);
  for (int n = 0; n < 3; ++n) target.fill(fui::makeRect(menuButton.x + 8, menuButton.y + 9 + n * 9, 28, 2), ink);
  screen.frame().hit(menuButton, SELECT, MENU_TOGGLE, fui::InputTouch);
  label(fui::makeRect(safe.x + 14, band + 6, safe.width - 80, 40), tr(STR_HOME_BOOKSHELF), 1);
  const auto layout = homeui::shelfLayout(screen.device());
  const auto footer = layout.footer, gridArea = layout.grid;
  const int columns = 3, rows = layout.rows, gap = layout.gap, cellWidth = layout.cellWidth,
            artHeight = layout.artHeight;
  noteThumbHeight(artHeight);
  uint64_t key = 14695981039346656037ULL;
  const auto hash = [&key](const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    while (size--) key = (key ^ *bytes++) * 1099511628211ULL;
  };
  const int orientation = int(renderer.getOrientation());
  const auto font = toybox::interfaceFontStyle();
  hash(&orientation, sizeof(orientation));
  hash(&SETTINGS.screenInverted, sizeof(SETTINGS.screenInverted));
  hash(&font, sizeof(font));
  const char* newLabel = tr(STR_HOME_NEW_BOOK);
  hash(newLabel, std::strlen(newLabel) + 1);
  for (int n = 1; n <= rows * columns && size_t(n) < books->size(); ++n) {
    const auto& book = (*books)[n];
    hash(book.path.c_str(), book.path.size() + 1);
    hash(book.title.c_str(), book.title.size() + 1);
    hash(coverPaths[n].c_str(), coverPaths[n].size() + 1);
    hash(&bookProgress[n], sizeof(bookProgress[n]));
  }
  const bool cached = homecover::restoreScene(renderer, gridArea, key);
  for (int n = 0; n < rows * columns && size_t(n + 1) < books->size(); ++n) {
    const int index = n + 1;
    const int x = gridArea.x + n % columns * (cellWidth + gap);
    const int y = gridArea.y + n / columns * (artHeight + 34 + gap);
    const auto art = fui::makeRect(x + (cellWidth - artHeight * 2 / 3) / 2, y, artHeight * 2 / 3, artHeight);
    screen.frame().hit(fui::makeRect(x, y, cellWidth, artHeight + 32), SELECT, index, fui::InputTouch);
    if (cached) continue;
    coverCache.paint(art, index, coverPaths[index], true, true);
    if (!coverCache.hasImage(index)) {
      fui::TextStyle placeholder;
      placeholder.font = 0;
      placeholder.maxLines = 3;
      placeholder.align = fui::TextAlign::Center;
      target.text(art.inset({16, 8, 16, 8}), (*books)[index].title.c_str(), placeholder);
    }
    char value[24];
    if (bookProgress[index] >= 0)
      std::snprintf(value, sizeof(value), "%d%%", bookProgress[index]);
    else
      pixelui::copyCaps(value, sizeof(value), tr(STR_HOME_NEW_BOOK));
    const char* badgeText = value;
    fui::TextStyle badge;
    badge.font = 0;
    badge.color = fui::Color::White;
    const int width = std::min(int(art.width) - 6, int(target.measureText(0, badgeText, badge).width) + 12);
    const int height = target.lineHeight(0) + 4;
    const auto tag = fui::makeRect(art.right() - width - 3, art.y + 3, width, height);
    coverBadge(target, tag);
    label(tag, badgeText, 0, true, fui::TextAlign::Center);
    softCoverOutline(target, art);
    label(fui::makeRect(x, art.bottom() + 4, cellWidth, 26), (*books)[index].title.c_str());
  }
  if (!cached) homecover::storeScene(renderer);
  const auto pager = fui::makeRect(safe.x + safe.width / 2 - 60, footer.y - 32, 120, 24);
  char pageText[24];
  std::snprintf(pageText, sizeof(pageText), "%d / %d", page + 1, pages);
  label(pager, pageText, 0, false, fui::TextAlign::Center);
  if (page > 0)
    screen.frame().hit(fui::makeRect(pager.x - 48, pager.y, 48, 24), SELECT, PAGE_PREVIOUS, fui::InputTouch);
  if (page + 1 < pages)
    screen.frame().hit(fui::makeRect(pager.right(), pager.y, 48, 24), SELECT, PAGE_NEXT, fui::InputTouch);
  target.fill(fui::makeRect(0, footer.y, screen.device().width, 4), fui::Paint::dither(fui::Color::DarkGray));
  if (!books->empty()) {
    const int height = footer.height + 8;
    const auto art = fui::makeRect(footer.x + 2, footer.y - 18, height * 2 / 3, height);
    target.fill(art, paper);
    coverCache.paint(art, 0, coverPaths[0], true);
    softCoverOutline(target, art);
    const int x = art.right() + 14;
    const int copyY = footer.y + (footer.height - 100) / 2;
    label(fui::makeRect(x, copyY, footer.right() - x, 30), books->front().title.c_str(), 1);
    label(fui::makeRect(x, copyY + 34, footer.right() - x, 24), books->front().author.c_str());
    char value[64];
    if (readingTimeKnown)
      std::snprintf(value, sizeof(value), tr(STR_HOME_READING_VALUE), std::max(0, progress),
                    static_cast<unsigned long long>(readingSeconds / 3600), unsigned(readingSeconds / 60 % 60));
    else
      std::snprintf(value, sizeof(value), "%d%% · —", std::max(0, progress));
    label(fui::makeRect(x, copyY + 64, footer.right() - x, 24), value);
    const auto track = fui::makeRect(x, footer.bottom() - 16, footer.right() - x, 3);
    target.fill(track, fui::Paint::dither(fui::Color::LightGray));
    target.fill(fui::makeRect(track.x, track.y, track.width * std::clamp(progress, 0, 100) / 100, track.height), ink);
    screen.frame().hit(footer, SELECT, 0, fui::InputTouch);
  }
  if (!drawer) return;
  const int count = homemenu::count(hasOpds);
  constexpr int panelPercent = 62;
  const int left = screen.device().width * (100 - panelPercent) / 100;
  const auto panel = fui::makeRect(left, safe.y, screen.device().width - left, safe.bottom() - safe.y);
  static constexpr uint8_t shadeBits[] = {0xAA, 0x00};
  const fui::BitmapRef shade{shadeBits, 8, 2, fui::BitmapFormat::BW1};
  target.bitmap(fui::makeRect(0, band, left, safe.bottom() - band), shade, fui::BitmapMode::Tile, ink);
  // Header output polarity is inverted; black bits produce the same visible shade.
  target.fill(fui::makeRect(0, 0, left, band), ink);
  target.bitmap(fui::makeRect(0, 0, left, band), shade, fui::BitmapMode::Tile, paper);
  if (local.tm_year >= 120) {
    char clock[16];
    if (SETTINGS.clockFormat == 1)
      std::snprintf(clock, sizeof(clock), "%d:%02d %s", (local.tm_hour + 11) % 12 + 1, local.tm_min,
                    local.tm_hour < 12 ? tr(STR_CLOCK_AM) : tr(STR_CLOCK_PM));
    else
      std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
    label(fui::makeRect(safe.x + safe.width / 2 - 66, safe.y, 132, 44), clock, 0, true, fui::TextAlign::Center);
    char date[32];
    dateformat::format(date, sizeof(date), local.tm_mday, local.tm_mon + 1, local.tm_year + 1900,
                       dateformat::fromStored(SETTINGS.dateFormat));
    label(fui::makeRect(safe.x + 14, safe.y, safe.width / 2 - 88, 44), date, 3, true);
  }
  screen.frame().hit(fui::makeRect(0, band, left, safe.bottom() - band), SELECT, MENU_TOGGLE, fui::InputTouch);
  target.fill(fui::makeRect(panel.x, band, panel.width, panel.bottom() - band), paper);
  screen.frame().hit(panel, SELECT, MENU_IGNORE, fui::InputTouch);
  const auto close = fui::makeRect(safe.right() - 50, band + 6, 44, 40);
  target.line({int16_t(close.x + 14), int16_t(close.y + 12)},
              {int16_t(close.right() - 14), int16_t(close.bottom() - 12)}, 2, ink);
  target.line({int16_t(close.right() - 14), int16_t(close.y + 12)},
              {int16_t(close.x + 14), int16_t(close.bottom() - 12)}, 2, ink);
  screen.frame().hit(close, SELECT, MENU_TOGGLE, fui::InputTouch);
  const int top = band + 58;
  const int rowHeight = std::min(wide ? 42 : 66, (safe.bottom() - 46 - top) / std::max(1, count));
  for (int n = 0; n < count; ++n) {
    const auto id = homemenu::at(homemenu::config(), n, hasOpds);
    const auto row = fui::makeRect(left + 10, top + n * rowHeight, panel.width - 20, rowHeight);
    const bool focus = selected == int(books->size()) + n;
    if (focus && !screen.device().hasTouch) target.fill(fui::makeRect(row.x, row.y + 10, 2, row.height - 20), ink);
    const int iconSize = wide ? 26 : 32;
    homeui::drawIcon(target, fui::makeRect(row.x + 12, row.y + (row.height - iconSize) / 2, iconSize, iconSize),
                     homemenu::icon(id));
    label(fui::makeRect(row.x + 54, row.y, row.width - 58, row.height), homemenu::label(id, hasPlugins), wide ? 0 : 1);
    screen.frame().hit(row, SELECT, books->size() + n, fui::InputTouch);
  }
}

void CoverGridHomeUi::drawHeaderBand() {
  // The stock full-width band at the theme's topPadding, exactly like every
  // pushed screen's header: the battery/clock hold one position across the
  // whole UI, and the grid is widened to meet them (see the hInset above).
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.batteryBarHeight}, nullptr);
}

void CoverGridHomeUi::drawEmpty(UiScreen& screen) {
  const auto& theme = screen.theme();
  const auto body = screen.body();
  auto title = theme.titleText;
  title.bold = true;
  title.align = fui::TextAlign::Center;
  auto message = theme.bodyText;
  message.align = fui::TextAlign::Center;
  constexpr int16_t ICON_SIZE = 32;
  const int16_t titleHeight = screen.target().lineHeight(title.font);
  const int16_t messageHeight = screen.target().lineHeight(message.font);
  const int16_t contentHeight = ICON_SIZE + theme.spaceLg + titleHeight + theme.spaceSm + messageHeight;
  int16_t y = body.y + std::max(0, (body.height - contentHeight) / 2);
  renderer.drawIcon(BookIcon, body.x + (body.width - ICON_SIZE) / 2, y, ICON_SIZE);
  y += ICON_SIZE + theme.spaceLg;
  screen.target().text(fui::Rect{body.x, y, body.width, titleHeight}, tr(STR_NO_OPEN_BOOK), title);
  y += titleHeight + theme.spaceSm;
  screen.target().text(fui::Rect{body.x, y, body.width, messageHeight}, tr(STR_START_READING), message);
}

void CoverGridHomeUi::drawCurrent(UiScreen& screen, fui::Rect rect, const int coverRowHeight) {
  const auto& theme = screen.theme();
  const auto& book = books->front();
  card.title = book.title.c_str();
  card.author = book.author.empty() ? nullptr : book.author.c_str();
  card.meta = nullptr;
  card.progressLabel = progress >= 0 ? progressText : nullptr;
  card.centerTextOnCover = true;
  card.progress = std::max(0, progress);
  card.progressMax = progress >= 0 ? 100 : 0;
  card.action = SELECT;
  // The featured card's selected state is a slim accent bar drawn after the
  // card (see below), not a bookCard indicator: every ring/outline treatment
  // tried here either overwhelmed the large cover or made the heading above
  // read as misaligned.
  card.state = fui::StateNormal;
  card.styles = theme.listRow;
  card.styles.selected.background = fui::Paint::dither(fui::Color::LightGray);
  // The grid thumbs' selection ring draws with this border: gray like Lyra's
  // selection box, not solid black.
  card.styles.selected.border = fui::Paint::dither(fui::Color::LightGray);
  card.styles.selected.foreground = fui::Paint::solid(fui::Color::Black);
  card.styles.selected.radius = theme.listRowRadius;
  card.styles.active = card.styles.selected;
  card.titleText = theme.bodyText;
  card.titleText.maxLines = renderer.getScreenWidth() > renderer.getScreenHeight() ? 1 : 2;
  card.authorText = theme.smallText;
  card.progressText = theme.smallText;
  card.progressHeight = 6;
  card.padding = fui::Insets{6, 6, 6, 6};
  card.gap = theme.spaceLg + theme.spaceSm;
  // Fit the shared 2:3 cover box within both the row and a grid column. No
  // gap reserve: the cell insets already keep adjacent covers apart, and the
  // SpaceBetween layout absorbs whatever slack remains.
  const int maxCoverWidth = rect.width / GRID_COLUMNS - 2 * COVER_CELL_INSET;
  card.coverSize.height = std::max(1, std::min(std::min<int>(rect.height, coverRowHeight) - 12, maxCoverWidth * 3 / 2));
  card.coverSize.width = std::max(1, card.coverSize.height * 2 / 3);
  noteThumbHeight(card.coverSize.height);
  gridBounds = layoutGrid(screen.body());
  rect.x = gridBounds.x;
  rect.width = gridBounds.width;
  card.coverPainterUserData = this;
  card.coverPainter = [](fui::DrawTarget& target, fui::Rect cover, const fui::BookCardProps&, void* user) {
    return static_cast<CoverGridHomeUi*>(user)->paintFramedCover(target, cover, 0);
  };
  fui::bookCard(screen.frame(), rect, card);

  if (selected == 0 && !BoardConfig::hasTouch()) {
    // Button boards only: a vertical accent bar left of the card, cover-height
    // and vertically centered on it, marks the featured card as the button
    // cursor without framing the cover. Touch boards tap directly and need no
    // cursor on the hero card.
    const int16_t barH = card.coverSize.height;
    screen.target().fill(
        fui::Rect{static_cast<int16_t>(rect.x - 5), static_cast<int16_t>(rect.y + (rect.height - barH) / 2), 3, barH},
        fui::Paint::dither(fui::Color::LightGray));
  }
}

fui::Rect CoverGridHomeUi::layoutGrid(fui::Rect rect) {
  // Exactly the hero's box: the three-equal-rows split in draw() already
  // guarantees it fits, and sharing the size keeps one cached thumb per book.
  grid.cellInset = fui::Insets{COVER_CELL_INSET, COVER_CELL_INSET, COVER_CELL_INSET, COVER_CELL_INSET};
  grid.coverSize = card.coverSize;
  grid.rowHeight = grid.coverSize.height + 12;
  // Full content width: the SpaceBetween column layout pins the outer covers
  // to the rect edges, so the grid reaches the chrome's inset line instead of
  // centering at its natural width.
  rect.height = GRID_ROWS * grid.rowHeight + (GRID_ROWS - 1) * grid.rowGap;
  return rect;
}

void CoverGridHomeUi::drawGrid(UiScreen& screen) {
  const auto rect = gridBounds;
  grid.count = books->size() > 1 ? books->size() - 1 : 0;
  grid.columns = GRID_COLUMNS;
  grid.columnLayout = fui::CoverGridColumnLayout::SpaceBetween;
  grid.action = SELECT;
  grid.inputMask = fui::InputTouch;
  grid.selectedIndex = selected > 0 && selected < static_cast<int>(books->size()) ? selected - 1 : -1;
  // Same thick cover ring as the featured card; the dithered Cell background
  // was easy to miss behind a dark cover.
  grid.selectionIndicator = fui::CoverGridSelectionIndicator::CoverFrame;
  // Thick dithered ring sized for the small thumbs: 6px outside the cover,
  // 2px over its edge.
  grid.selectedCoverFrameGap = 6;
  grid.selectedCoverFrameWidth = 8;
  grid.cellStyles = card.styles;
  grid.labelHeight = 0;
  grid.labelGap = 0;
  grid.scrollIndicator = false;
  grid.itemProvider = [](uint16_t index, void*) { return fui::coverGridItem(nullptr, index + 1); };
  grid.coverPainterUserData = this;
  grid.coverPainter = [](fui::DrawTarget& target, fui::Rect cover, const fui::CoverGridItem&, uint16_t index,
                         void* user) {
    return static_cast<CoverGridHomeUi*>(user)->paintFramedCover(target, cover, index + 1);
  };
  fui::coverGrid(screen.frame(), rect, grid);
}

void CoverGridHomeUi::drawTabs(UiScreen& screen, fui::Rect rect) {
  const int count = homemenu::count(hasOpds);
  for (int i = 0; i < count; ++i) {
    auto& tab = tabItems[i];
    tab.value = books->size() + i;
    tab.selected = selected == tab.value;
    tab.label = homemenu::label(homemenu::at(homemenu::config(), i, hasOpds), hasPlugins);
  }
  tabs.tabs = tabItems.data();
  tabs.count = count;
  tabs.layout = fui::TabBarLayout::SpaceBetween;
  tabs.action = SELECT;
  tabs.inputMask = fui::InputTouch;
  tabs.iconSize = 32;
  tabs.iconPainterUserData = this;
  tabs.iconPainter = [](fui::DrawTarget& target, fui::Rect iconRect, const fui::TabItem& tab, uint8_t, void* user) {
    const auto& self = *static_cast<CoverGridHomeUi*>(user);
    const int index = tab.value - static_cast<int>(self.books->size());
    if (const auto* item = homemenu::shortcutItem(homemenu::at(homemenu::config(), index, self.hasOpds))) {
      target.bitmap(iconRect, fui::bitmapFromIcon(*item->icon), fui::BitmapMode::Contain);
      return true;
    }
    const uint8_t* icon = LibraryIcon;
    switch (homemenu::at(homemenu::config(), index, self.hasOpds)) {
      case homemenu::Id::Files:
        icon = FolderIcon;
        break;
      case homemenu::Id::Recent:
        icon = LibraryIcon;
        break;
      case homemenu::Id::Stats:
        icon = BlocksIcon;
        break;
      case homemenu::Id::Settings:
        icon = Settings2Icon;
        break;
      case homemenu::Id::Games:
        icon = ShelfGamesIcon;
        break;
      case homemenu::Id::Apps:
        icon = ShelfAppsIcon;
        break;
      case homemenu::Id::Transfer:
        icon = TransferIcon;
        break;
      case homemenu::Id::Services:
        icon = BlocksIcon;
        break;
      case homemenu::Id::Count:
        return false;
      case homemenu::Id::Shortcut0:
      case homemenu::Id::Shortcut1:
      case homemenu::Id::Shortcut2:
      case homemenu::Id::Shortcut3:
      case homemenu::Id::Shortcut4:
        icon = homemenu::icon(homemenu::at(homemenu::config(), index, self.hasOpds)) == homeui::Icon::Games
                   ? ShelfGamesIcon
                   : ShelfAppsIcon;
        break;
    }
    self.renderer.drawIcon(icon, iconRect.x, iconRect.y, iconRect.width);
    return true;
  };
  tabs.tabStyles.normal.background = fui::Paint::solid(fui::Color::White);
  tabs.tabStyles.selected.background = fui::Paint::solid(fui::Color::White);
  tabs.selectedUnderline = 2;
  tabs.text.maxLines = 1;
  tabs.distributedSlotWidth = 0;
  fui::tabBar(screen.frame(), rect, tabs);
}

bool CoverGridHomeUi::paintFramedCover(fui::DrawTarget& target, fui::Rect rect, size_t index) {
  constexpr int16_t SHADOW_OFFSET = 2;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  target.fill(fui::Rect{rect.right(), static_cast<int16_t>(rect.y + SHADOW_OFFSET), SHADOW_OFFSET, rect.height}, ink);
  target.fill(fui::Rect{static_cast<int16_t>(rect.x + SHADOW_OFFSET), rect.bottom(), rect.width, SHADOW_OFFSET}, ink);
  // The false spine draws inside the cover paint (HomeCoverCache), glued to
  // the art's left edge, so it stays aligned whatever each cover's margin is.
  const bool drawn = index < coverPaths.size() && coverCache.paint(rect, index, coverPaths[index]);
  return drawn;
}
