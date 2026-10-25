#include "HomeDashboardUi.h"

#include <ChromeRegion.h>
#include <FreeInkUIGfxRenderer.h>
#include <FreeInkUIIcon.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>
#include <ctime>
#include <functional>

#include "../../CrossPointSettings.h"
#include "../../MappedInputManager.h"
#include "../../RecentBooksStore.h"
#include "../../components/UITheme.h"
#include "../../fontIds.h"
#include "../../util/BookProgress.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "CoverSceneCache.h"
#include "HomeMenuIntegration.h"

namespace fui = freeink::ui;

void HomeDashboardUi::readDate(char* out, const size_t size, char* clock, size_t clockSize) const {
  struct tm local{};
  const time_t now = std::time(nullptr);
  if (!(now > 0 && localtime_r(&now, &local) && local.tm_year >= 120) && !halClock.localTime(local)) {
    homeui::formatDate(out, size, 0, 0, 0);
    std::snprintf(clock, clockSize, "--:--");
    return;
  }
  if (SETTINGS.clockFormat == 1)
    std::snprintf(clock, clockSize, "%d:%02d %s", (local.tm_hour + 11) % 12 + 1, local.tm_min,
                  local.tm_hour < 12 ? tr(STR_CLOCK_AM) : tr(STR_CLOCK_PM));
  else
    std::snprintf(clock, clockSize, "%02d:%02d", local.tm_hour, local.tm_min);
  homeui::formatDate(out, size, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

void HomeDashboardUi::begin() {
  toybox::ensureFonts(renderer);
  readDate(model.date, sizeof(model.date), model.time, sizeof(model.time));
  lastDatePoll = millis();
  resetUi();
  app.on(homeui::kSelect, &HomeDashboardUi::onAction, this);
  app.setScreen(&HomeDashboardUi::screenFn, this);
}

bool HomeDashboardUi::pollDate() {
  if (millis() - lastDatePoll < 1000) return false;
  lastDatePoll = millis();
  char date[sizeof(model.date)];
  char clock[sizeof(model.time)];
  readDate(date, sizeof(date), clock, sizeof(clock));
  if (!std::strcmp(date, model.date) && !std::strcmp(clock, model.time)) return false;
  std::strcpy(model.date, date);
  std::strcpy(model.time, clock);
  return true;
}

void HomeDashboardUi::addRow(const char* label, const homeui::Icon icon) {
  if (model.count == homeui::kMaxRows) return;
  auto& row = model.rows[model.count++];
  model.unicode |= homeui::copyLabel(row.label, sizeof(row.label), label);
  row.icon = icon;
  static constexpr StrId labels[] = {StrId::STR_SHORT_FILES,    StrId::STR_SHORT_RECENT,  StrId::STR_SHORT_TRANSFER,
                                     StrId::STR_SHORT_SETTINGS, StrId::STR_SHORT_PLUGINS, StrId::STR_SHORT_GAMES,
                                     StrId::STR_SHORT_APPS,     StrId::STR_SHORT_STATS};
  homeui::copyLabel(row.brief, sizeof(row.brief), I18N.get(labels[static_cast<unsigned>(icon)]));
}

bool HomeDashboardUi::draw(const RecentBook* book, bool librarySlot, bool plugins, int selected, int recentIndex,
                           int recentCount, const RecentBook* const* sides) {
  model.style =
      static_cast<homeui::Style>(SETTINGS.homeLayout < CrossPointSettings::HOME_LAYOUT_COUNT ? SETTINGS.homeLayout : 0);
  model.recentIndex = recentIndex;
  model.recentCount = recentCount;
  for (int n = 0; n < 4; ++n) neighbors[n] = sides ? sides[n] : nullptr;
  if (book && progressPath != book->path) {
    progressPath = book->path;
    model.progress = loadBookProgress(book->path);
  }
  model.unicode = false;
  model.hasBook = book != nullptr;
  model.canPrevious = model.canNext = recentCount > 1;
  model.selected = selected;
  model.count = 0;
  for (int n = 0; n < homemenu::count(librarySlot); ++n) {
    const auto id = homemenu::at(homemenu::config(), n, librarySlot);
    addRow(homemenu::label(id, plugins), homemenu::icon(id));
    auto& row = model.rows[model.count - 1];
    row.shortcutIcon = {};
    if (const auto* entry = homemenu::shortcutItem(id)) {
      row.shortcutIcon = fui::bitmapFromIcon(*entry->icon);
      row.brief[0] = '\0';
    }
  }
  model.unicode |= homeui::copyLabel(model.continueLabel, sizeof(model.continueLabel), tr(STR_CONTINUE_READING));
  model.unicode |= homeui::copyLabel(model.emptyTitle, sizeof(model.emptyTitle), tr(STR_NO_OPEN_BOOK));
  model.unicode |= homeui::copyLabel(model.emptyHelp, sizeof(model.emptyHelp), tr(STR_START_READING));
  if (book) {
    model.unicode |= homeui::copyLabel(model.title, sizeof(model.title), book->title.c_str());
    model.unicode |= homeui::copyLabel(model.author, sizeof(model.author), book->author.c_str());
  }
  status.status.battery.label = nullptr;
  GUI.applyHeaderStatus(renderer, status);
  model.battery = status.status.battery.percent;
  model.charging = status.status.battery.charging;
  model.showPercent = status.status.battery.label != nullptr;

  const auto next = homeui::layout(uiTarget.deviceContext(), model.count, model.style);
  const auto& cover = next.cover;
  const bool changed = cover.x != layout.cover.x || cover.y != layout.cover.y || cover.width != layout.cover.width ||
                       cover.height != layout.cover.height ||
                       orientation != static_cast<int>(renderer.getOrientation());
  if (changed) resetUi();
  layout = next;
  orientation = static_cast<int>(renderer.getOrientation());
  toybox::ensureFonts(renderer);
  uiTarget.setFont(0, toybox::kButtonFontId);
  uiTarget.setFont(1, toybox::kUiFontId);
  uiTarget.setFont(2, toybox::kDisplayFontId);
  uiTarget.setFont(3, toybox::kTileFontId);
  uiTarget.setFont(4, toybox::kCarouselTitleFontId);
  chrome::headerHeight = layout.header.bottom();
  renderUi();
  return changed;
}

void HomeDashboardUi::paintCover(const RecentBook& book) {
  uint64_t key = 14695981039346656037ULL;
  const auto bytes = [&key](const void* data, size_t count) {
    const auto* p = static_cast<const uint8_t*>(data);
    while (count--) key = (key ^ *p++) * 1099511628211ULL;
  };
  const auto source = [&bytes](const RecentBook* item) {
    const bool present = item != nullptr;
    bytes(&present, sizeof(present));
    if (item) {
      bytes(item->path.c_str(), item->path.size() + 1);
      bytes(item->coverBmpPath.c_str(), item->coverBmpPath.size() + 1);
    }
  };
  const auto rect = [&bytes](fui::Rect value) {
    const int16_t geometry[] = {value.x, value.y, value.width, value.height};
    bytes(geometry, sizeof(geometry));
  };
  bytes(&orientation, sizeof(orientation));
  bytes(&SETTINGS.screenInverted, sizeof(SETTINGS.screenInverted));
  bytes(&model.style, sizeof(model.style));
  rect(layout.coverRegion);
  rect(layout.cover);
  source(&book);
  if (model.style != homeui::Style::Framed)
    for (int n : {1, 2}) {
      rect(layout.sides[n]);
      source(neighbors[n]);
    }
  if (homecover::restoreScene(renderer, layout.coverRegion, key)) return;
  if (model.style != homeui::Style::Framed) {
    const int order[] = {1, 2};
    for (int n : order)
      if (neighbors[n]) covers.paint(*neighbors[n], layout.sides[n], n < 2 ? -1 : 1);
  }
  covers.paint(book, layout.cover);
  homecover::storeScene(renderer);
}

void HomeDashboardUi::screenFn(UiScreen& screen, void* user) {
  auto& self = *static_cast<HomeDashboardUi*>(user);
  homeui::build(screen, self.model, self.layout, self.hit);
}

void HomeDashboardUi::finishCover() { homeui::drawCardSelection(uiTarget, model, layout); }

bool HomeDashboardUi::canRepaintSelection(int selected) {
  if (selected == model.selected || orientation != static_cast<int>(renderer.getOrientation()) ||
      static_cast<int>(model.style) != SETTINGS.homeLayout || !routingReady())
    return false;
  status.status.battery.label = nullptr;
  GUI.applyHeaderStatus(renderer, status);
  return model.battery == status.status.battery.percent && model.charging == status.status.battery.charging &&
         model.showPercent == (status.status.battery.label != nullptr);
}

bool HomeDashboardUi::repaintSelection(int selected) {
  if (model.selected == selected) return false;
  homeui::drawMenuSelection(uiTarget, model, layout, selected);
  return true;
}

void HomeDashboardUi::onAction(const fui::ActionEvent& event, void* user) {
  auto& self = *static_cast<HomeDashboardUi*>(user);
  self.pending = event.value;
  self.app.clearTapFlash();
}

int HomeDashboardUi::selectedAction(const MappedInputManager& input) {
  pending = -1;
  const auto touch = routeTouch(input);
  return touch.snap.touchReleased ? pending : -1;
}
