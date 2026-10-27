#pragma once

#include <FreeInkUI.h>

#include <cstddef>

namespace homeui {
namespace fui = freeink::ui;

constexpr int kMaxRows = 8;
enum class Style : uint8_t { Framed, Carousel };
enum class Icon { Files, Library, Transfer, Settings, Plugins, Games, Apps, Stats };
struct Row {
  char label[96]{};
  char brief[32]{};
  Icon icon = Icon::Files;
  fui::BitmapRef shortcutIcon{};
};
struct Model {
  Style style = Style::Framed;
  int recentIndex = 0, recentCount = 0, progress = -1;
  char date[20]{}, time[10]{};
  char title[192]{};
  char author[96]{};
  char continueLabel[96]{};
  char emptyTitle[96]{};
  char emptyHelp[128]{};
  Row rows[kMaxRows]{};
  int count = 0;
  int selected = 0;
  int battery = 0;
  bool hasBook = false;
  bool charging = false;
  bool showPercent = true;
  bool unicode = false;
  bool canPrevious = false, canNext = false;
};
struct Layout {
  Style style = Style::Framed;
  fui::Rect sides[4]{}, coverRegion{}, progress{}, bookProgress{};
  fui::Rect header{}, card{}, cover{}, badge{}, previous{}, next{}, title{}, author{}, menu{};
  fui::Rect rows[kMaxRows]{};
  int count = 0;
};

// ASCII labels use the pixel face; UTF-8 is preserved for the UI font fallback.
bool copyLabel(char* out, size_t size, const char* text);
void formatDate(char* out, size_t size, int year, int month, int day);
void drawIcon(fui::DrawTarget& target, fui::Rect rect, Icon kind, bool inverted = false);
Layout layout(const fui::DeviceContext& device, int rows, Style style = Style::Framed);
void draw(fui::DrawTarget& target, const Model& model, const Layout& layout);
// Keeps the book, status and published interaction geometry untouched.
void drawMenuSelection(fui::DrawTarget& target, Model& model, const Layout& layout, int selected);
// Draw after the byte-aligned cover snapshot has been captured or restored.
void drawCardSelection(fui::DrawTarget& target, const Model& model, const Layout& layout);

constexpr fui::ActionId kSelect = 1;
constexpr int kPreviousBook = -2, kNextBook = -3;
template <typename Screen>
void build(Screen& screen, const Model& model, const Layout& geometry, fui::ButtonProps& hit) {
  draw(screen.target(), model, geometry);
  hit.value = 0;
  hit.action = kSelect;
  hit.styles.explicitlySet = true;
  hit.minTouchSize = 0;
  if (model.hasBook) {
    const auto card = geometry.card;
    const auto previous = geometry.previous;
    const auto next = geometry.next;
    const fui::Rect regions[] = {
        fui::makeRect(card.x, card.y, card.width, previous.y - card.y),
        fui::makeRect(card.x, previous.bottom(), card.width, card.bottom() - previous.bottom()),
        fui::makeRect(card.x, previous.y, previous.x - card.x, previous.height),
        fui::makeRect(previous.right(), previous.y, next.x - previous.right(), previous.height),
        fui::makeRect(next.right(), previous.y, card.right() - next.right(), previous.height)};
    for (int n = 0; n < 5; ++n)
      if (!geometry.badge.empty() && (model.style == Style::Framed || n != 1) && !regions[n].empty())
        screen.frame().hit(regions[n], kSelect, 0, fui::InputTouch);
    if (model.style != Style::Framed) {
      const auto art = geometry.coverRegion;
      const int controlsBottom = geometry.badge.empty() ? card.y : previous.bottom();
      const auto above = fui::makeRect(card.x, controlsBottom, card.width, art.y - controlsBottom);
      const auto below = fui::makeRect(card.x, art.bottom(), card.width, card.bottom() - art.bottom());
      if (!above.empty()) screen.frame().hit(above, kSelect, 0, fui::InputTouch);
      if (!below.empty()) screen.frame().hit(below, kSelect, 0, fui::InputTouch);
      screen.frame().hit(geometry.cover, kSelect, 0, fui::InputTouch);
      if (model.canPrevious)
        screen.frame().hit(fui::makeRect(art.x, art.y, geometry.cover.x - art.x, art.height), kSelect, kPreviousBook,
                           fui::InputTouch);
      if (model.canNext)
        screen.frame().hit(
            fui::makeRect(geometry.cover.right(), art.y, art.right() - geometry.cover.right(), art.height), kSelect,
            kNextBook, fui::InputTouch);
    }
    hit.value = kPreviousBook;
    if (model.canPrevious && !geometry.previous.empty()) screen.button(hit, geometry.previous);
    hit.value = kNextBook;
    if (model.canNext && !geometry.next.empty()) screen.button(hit, geometry.next);
  }
  for (int n = 0; n < geometry.count; ++n) {
    hit.value = static_cast<int16_t>(n + (model.hasBook ? 1 : 0));
    screen.button(hit, geometry.rows[n]);
  }
}
}  // namespace homeui
