#include "BookPreparationScreens.h"

#include <I18n.h>

#include <cstdio>
#include <cstring>

#include "../ui/Dashboard.h"
#include "../ui/ToolsDigits.h"

namespace bookprepui {
namespace fui = freeink::ui;
namespace {
fui::Rect bodyFor(const fui::DeviceContext& device) { return pixelui::geometry(device).body.inset({12, 14, 12, 14}); }
void prose(toybox::Screen& s, fui::Rect r, const char* value, int lines = 2, bool strong = false,
           fui::TextAlign align = fui::TextAlign::Left) {
  if (!value || r.height <= 0) return;
  fui::TextStyle style;
  style.font = 0;
  style.maxLines = lines;
  style.bold = strong;
  style.align = align;
  s.target().text(r, value, style);
}
void control(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, int value = 0,
             bool primary = false) {
  dashboard::button(s, r, label, action, value, primary, nullptr, 0);
}
void footer(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, bool enabled = true) {
  const int half = (r.width - 12) / 2;
  control(s, fui::makeRect(r.x, r.y, half, r.height), tr(STR_BACK), Back);
  const auto button = fui::makeRect(r.x + half + 12, r.y, r.width - half - 12, r.height);
  if (enabled)
    control(s, button, label, action, 0, true);
  else {
    dashboard::frame(s, button);
    dashboard::text(s, button, label, 0);
  }
}
void choiceFrame(toybox::Screen& s, fui::Rect box, fui::ActionId action, int value = 0) {
  dashboard::frame(s, box);
  if (s.frame().buttonSelected(action, value)) dashboard::frame(s, box.inset({3, 3, 3, 3}), 3);
}
void warningIcon(toybox::Screen& s, fui::Rect r) {
  const int mid = r.x + r.width / 2, bottom = r.bottom() - 3;
  s.target().line({int16_t(mid), int16_t(r.y + 3)}, {int16_t(r.x + 3), int16_t(bottom)}, 2, dashboard::ink);
  s.target().line({int16_t(r.x + 3), int16_t(bottom)}, {int16_t(r.right() - 3), int16_t(bottom)}, 2, dashboard::ink);
  s.target().line({int16_t(r.right() - 3), int16_t(bottom)}, {int16_t(mid), int16_t(r.y + 3)}, 2, dashboard::ink);
  s.target().line({int16_t(mid), int16_t(r.y + 14)}, {int16_t(mid), int16_t(bottom - 11)}, 3, dashboard::ink);
  s.target().fill(fui::makeRect(mid - 1, bottom - 7, 3, 3), dashboard::ink);
}
void warning(toybox::Screen& s, fui::Rect r, bool detail) {
  const int titleHeight = detail ? std::min(64, int(r.height)) : int(r.height);
  warningIcon(s, fui::makeRect(r.x, r.y + std::max(0, (titleHeight - 38) / 2), 38, 38));
  prose(s, fui::makeRect(r.x + 52, r.y, r.width - 52, titleHeight), tr(STR_PREPARE_HOURS), 2, true);
  if (detail) prose(s, fui::makeRect(r.x, r.y + 72, r.width, r.height - 72), tr(STR_PREPARE_MANY_WARNING), 5);
}
void formats(toybox::Screen& s, const Model& m, fui::Rect r) {
  prose(s, fui::makeRect(r.x, r.y, r.width, 32), tr(STR_SETTINGS_FORMATS), 1, true);
  static constexpr StrId names[] = {StrId::STR_PREPARE_FORMAT_EPUB, StrId::STR_PREPARE_FORMAT_XTC,
                                    StrId::STR_PREPARE_FORMAT_XTCH};
  static constexpr uint8_t flags[] = {bookprep::FormatEpub, bookprep::FormatXtc, bookprep::FormatXtch};
  const int width = (r.width - 16) / 3;
  for (int n = 0; n < 3; ++n) {
    const auto box = fui::makeRect(r.x + n * (width + 8), r.y + 38, width, 58);
    choiceFrame(s, box, ToggleFormat, flags[n]);
    dashboard::check(s, fui::makeRect(box.x + 10, box.y + 17, 24, 24), m.formats & flags[n]);
    prose(s, fui::makeRect(box.x + 42, box.y + 13, box.width - 48, 32), I18N.get(names[n]), 1);
    dashboard::hit(s, box, ToggleFormat, flags[n]);
  }
}
void optionRow(toybox::Screen& s, fui::Rect r, const char* title, const char* value, fui::ActionId action) {
  choiceFrame(s, r, action);
  prose(s, fui::makeRect(r.x + 16, r.y + 10, r.width - 58, 32), title, 1, true);
  prose(s, fui::makeRect(r.x + 16, r.y + 45, r.width - 58, r.height - 48), value, 2);
  dashboard::chevron(s.target(), fui::makeRect(r.right() - 42, r.y, 28, r.height));
  dashboard::hit(s, r, action);
}
void options(toybox::Screen& s, const Model& m, fui::Rect body, fui::Rect button) {
  const bool wide = s.device().width > s.device().height;
  char value[96];
  int top = body.y;

  dashboard::icon(s, fui::makeRect(body.x, top + 4, 48, 48), extra_reading_book_32);
  prose(s, fui::makeRect(body.x + 64, top, body.width - 64, wide ? 60 : 96), tr(STR_PREPARE_INTRO), wide ? 2 : 3);
  top += wide ? 74 : 112;

  if (wide) {
    const int half = (body.width - 24) / 2;
    formats(s, m, fui::makeRect(body.x, top, half, 100));
    warning(s, fui::makeRect(body.x, top + 116, half, button.y - top - 124), false);
    if (m.formats & bookprep::FormatEpub) {
      std::snprintf(value, sizeof(value), tr(STR_PREPARE_FONT_COUNT), m.fonts ? m.fonts->selectedCount() : 0);
      optionRow(s, fui::makeRect(body.x + half + 24, top, body.width - half - 24, 84), tr(STR_FONT), value,
                ChooseFonts);
      if (m.sizeMode == bookprep::SizeMode::All)
        std::snprintf(value, sizeof(value), "%s", tr(STR_PREPARE_ALL_SIZES));
      else
        std::snprintf(value, sizeof(value), tr(STR_CARD_POINT_SIZE_FORMAT), unsigned(m.pointSize));
      optionRow(s, fui::makeRect(body.x + half + 24, top + 96, body.width - half - 24, 84), tr(STR_PREPARE_SIZES),
                value, ChooseSizes);
    } else
      prose(s, fui::makeRect(body.x + half + 24, top + 12, body.width - half - 24, 170), tr(STR_PREPARE_COVERS_ONLY),
            5);
  } else {
    formats(s, m, fui::makeRect(body.x, top, body.width, 100));
    top += 114;
    if (m.formats & bookprep::FormatEpub) {
      std::snprintf(value, sizeof(value), tr(STR_PREPARE_FONT_COUNT), m.fonts ? m.fonts->selectedCount() : 0);
      optionRow(s, fui::makeRect(body.x, top, body.width, 92), tr(STR_FONT), value, ChooseFonts);
      top += 104;
      if (m.sizeMode == bookprep::SizeMode::All)
        std::snprintf(value, sizeof(value), "%s", tr(STR_PREPARE_ALL_SIZES));
      else
        std::snprintf(value, sizeof(value), tr(STR_CARD_POINT_SIZE_FORMAT), unsigned(m.pointSize));
      optionRow(s, fui::makeRect(body.x, top, body.width, 92), tr(STR_PREPARE_SIZES), value, ChooseSizes);
      top += 110;
    } else {
      prose(s, fui::makeRect(body.x, top, body.width, 128), tr(STR_PREPARE_COVERS_ONLY), 4);
      top += 144;
    }
    warning(s, fui::makeRect(body.x, top, body.width, button.y - top - 12), false);
  }
  const bool enabled = m.formats && (!(m.formats & bookprep::FormatEpub) || (m.fonts && m.fonts->selectedCount()));
  footer(s, button, tr(STR_PREPARE_REVIEW), Start, enabled);
}
void pager(toybox::Screen& s, fui::Rect body, int y, int page, int pages) {
  if (pages <= 1) return;
  char value[32];
  if (page) control(s, fui::makeRect(body.x, y, 52, 44), "<", Page, page - 1);
  std::snprintf(value, sizeof(value), "%d / %d", page + 1, pages);
  dashboard::text(s, fui::makeRect(body.x + 60, y, body.width - 120, 44), value, 0);
  if (page + 1 < pages) control(s, fui::makeRect(body.right() - 52, y, 52, 44), ">", Page, page + 1);
}
void fonts(toybox::Screen& s, const Model& m, fui::Rect body, fui::Rect button) {
  if (!m.fonts) return;
  const bool wide = s.device().width > s.device().height;
  if (!wide) prose(s, fui::makeRect(body.x, body.y, body.width, 64), tr(STR_PREPARE_KEEP_DEFAULT), 2);
  static constexpr StrId labels[] = {StrId::STR_PREPARE_CURRENT, StrId::STR_PREPARE_ALL, StrId::STR_PREPARE_NONE};
  static constexpr fui::ActionId actions[] = {CurrentFont, AllFonts, NoFonts};
  const int width = (body.width - 16) / 3;
  for (int n = 0; n < 3; ++n)
    control(s, fui::makeRect(body.x + n * (width + 8), body.y + (wide ? 0 : 70), width, 44), I18N.get(labels[n]),
            actions[n]);
  const int listY = body.y + (s.device().width > s.device().height ? 58 : 126),
            capacity = pageSize(s.device(), View::Fonts);
  const int pages = std::max(1, (int(m.fonts->count) + capacity - 1) / capacity);
  const int page = std::clamp(m.page, 0, pages - 1), first = page * capacity;
  for (int n = 0; n < capacity && first + n < int(m.fonts->count); ++n) {
    const int index = first + n;
    const auto& choice = m.fonts->fonts[index];
    const auto box = fui::makeRect(body.x, listY + n * 68, body.width, 60);
    choiceFrame(s, box, ToggleFont, index);
    dashboard::check(s, fui::makeRect(box.x + 12, box.y + 16, 28, 28), choice.selected);
    prose(s, fui::makeRect(box.x + 54, box.y + 6, box.width - 68, 48),
          *choice.family ? choice.family : tr(STR_MENU_FONT_CLASSIC), 2);
    dashboard::hit(s, box, ToggleFont, index);
  }
  pager(s, body, button.y - 56, page, pages);
  control(s, button, tr(STR_DONE), Done, 0, true);
}
void sizes(toybox::Screen& s, const Model& m, fui::Rect body, fui::Rect button) {
  prose(s, fui::makeRect(body.x, body.y, body.width, 60), tr(STR_PREPARE_SIZE_HINT), 2);
  const auto all = fui::makeRect(body.x, body.y + 70, body.width, 52);
  choiceFrame(s, all, AllSizes);
  dashboard::check(s, fui::makeRect(all.x + 12, all.y + 12, 28, 28), m.sizeMode == bookprep::SizeMode::All);
  prose(s, fui::makeRect(all.x + 54, all.y + 10, all.width - 68, 34), tr(STR_PREPARE_ALL_SIZES), 1);
  dashboard::hit(s, all, AllSizes);
  const int capacity = pageSize(s.device(), View::Sizes), count = int(m.sizes.count());
  const int pages = std::max(1, (count + capacity - 1) / capacity), page = std::clamp(m.page, 0, pages - 1);
  int point = 0;
  for (int n = 0; n < page * capacity; ++n) point = m.sizes.next(point);
  for (int n = 0; n < capacity && (point = m.sizes.next(point)) > 0; ++n) {
    const auto box = fui::makeRect(body.x, body.y + 134 + n * 68, body.width, 60);
    choiceFrame(s, box, ChooseSize, point);
    dashboard::check(s, fui::makeRect(box.x + 12, box.y + 16, 28, 28),
                     m.sizeMode == bookprep::SizeMode::Single && point == m.pointSize);
    char text[32];
    std::snprintf(text, sizeof(text), tr(STR_CARD_POINT_SIZE_FORMAT), unsigned(point));
    prose(s, fui::makeRect(box.x + 54, box.y + 14, box.width - 68, 32), text, 1);
    dashboard::hit(s, box, ChooseSize, point);
  }
  pager(s, body, button.y - 56, page, pages);
  control(s, button, tr(STR_DONE), Done, 0, true);
}
void confirmation(toybox::Screen& s, const Model& m, fui::Rect body, fui::Rect button) {
  const bool wide = s.device().width > s.device().height;
  char text[128];
  const auto left =
      fui::makeRect(body.x, body.y, wide ? (body.width - 24) / 2 : body.width, wide ? button.y - body.y - 12 : 282);
  dashboard::frame(s, left);
  dashboard::icon(s, fui::makeRect(left.x + 16, left.y + 16, 42, 42), extra_reading_book_32);
  char names[48];
  std::snprintf(
      names, sizeof(names), "%s%s%s%s%s", m.formats & bookprep::FormatEpub ? tr(STR_PREPARE_FORMAT_EPUB) : "",
      (m.formats & bookprep::FormatEpub) && (m.formats & bookprep::FormatXtc) ? " / " : "",
      m.formats & bookprep::FormatXtc ? tr(STR_PREPARE_FORMAT_XTC) : "",
      (m.formats & (bookprep::FormatEpub | bookprep::FormatXtc)) && (m.formats & bookprep::FormatXtch) ? " / " : "",
      m.formats & bookprep::FormatXtch ? tr(STR_PREPARE_FORMAT_XTCH) : "");
  prose(s, fui::makeRect(left.x + 72, left.y + 16, left.width - 88, 48), names, 2, true);
  int y = left.y + 78;
  if (m.formats & bookprep::FormatEpub) {
    const auto count = m.fonts ? m.fonts->passes(m.sizeMode) : 0;
    std::snprintf(text, sizeof(text), "%s: %s", tr(STR_EPUB_ENGINE),
                  m.engine == EpubEngine::Classic ? tr(STR_EPUB_ENGINE_CLASSIC) : tr(STR_EPUB_ENGINE_MODERN));
    prose(s, fui::makeRect(left.x + 16, y, left.width - 32, 62), text, 2, true);
    y += 66;
    std::snprintf(text, sizeof(text), tr(STR_PREPARE_LAYOUTS), count);
    prose(s, fui::makeRect(left.x + 16, y, left.width - 32, 36), text, 1);
    y += 42;
    prose(s, fui::makeRect(left.x + 16, y, left.width - 32, left.bottom() - y - 12), tr(STR_PREPARE_ENGINE_HINT), 3);
  } else
    prose(s, fui::makeRect(left.x + 16, y, left.width - 32, left.height - 96), tr(STR_PREPARE_COVERS_ONLY), 5);
  const auto notice =
      wide ? fui::makeRect(left.right() + 24, body.y + 8, body.width - left.width - 24, button.y - body.y - 24)
           : fui::makeRect(body.x, left.bottom() + 24, body.width, button.y - left.bottom() - 36);
  warning(s, notice, true);
  footer(s, button, tr(STR_PREPARE_START), ConfirmStart);
}
void progress(toybox::Screen& s, const Model& m, fui::Rect body, fui::Rect button) {
  const bool wide = s.device().width > s.device().height;
  const int height = wide ? button.y - body.y - 16 : 258;
  const int width = wide ? (body.width - 24) / 2 : body.width;
  const int top = wide ? body.y : body.y + std::max(0, (button.y - body.y - 536) / 2);
  const auto status = fui::makeRect(body.x, top, width, height);
  dashboard::frame(s, status);
  dashboard::icon(s, fui::makeRect(status.x + 16, status.y + 16, 44, 44), extra_reading_book_32);
  prose(s, fui::makeRect(status.x + 76, status.y + 14, status.width - 92, 56), m.status, 2, true);
  char percent[16];
  std::snprintf(percent, sizeof(percent), "%u%%", m.total ? unsigned(m.done * 100 / m.total) : 0);
  toolsdigits::draw(s, fui::makeRect(status.x + 16, status.y + 88, width - 32, 68), percent);
  const int barY = status.bottom() - 80;
  dashboard::bar(s, fui::makeRect(status.x + 16, barY, width - 32, 16), m.done, m.total);
  prose(s, fui::makeRect(status.x + 16, barY + 24, width - 32, 40), m.counts, 1, false, fui::TextAlign::Center);
  const auto details = wide ? fui::makeRect(status.right() + 24, top, body.width - width - 24, height)
                            : fui::makeRect(body.x, status.bottom() + 16, width, 260);
  dashboard::frame(s, details);
  prose(s, fui::makeRect(details.x + 16, details.y + 14, 78, 32), tr(STR_BOOK), 1, true);
  if (m.running)
    prose(s, fui::makeRect(details.x + 102, details.y + 14, details.width - 118, 32), m.note, 1, false,
          fui::TextAlign::Right);
  prose(s, fui::makeRect(details.x + 16, details.y + 52, details.width - 32, 84), m.detail, 3);
  s.target().line({int16_t(details.x + 16), int16_t(details.y + 144)},
                  {int16_t(details.right() - 16), int16_t(details.y + 144)}, 1,
                  chrome::lightHeader ? lightui::rule : dashboard::ink);
  prose(s, fui::makeRect(details.x + 16, details.y + 158, details.width - 32, details.height - 170),
        m.running ? m.font : m.note, 3);
  control(s, button, m.running ? tr(STR_CANCEL) : tr(STR_BACK), m.running ? Cancel : Back);
}
}  // namespace
int pageSize(const fui::DeviceContext& device, View view) {
  const auto body = bodyFor(device);
  const int top = view == View::Sizes ? 134 : device.width > device.height ? 58 : 126;
  return std::max(1, (int(body.height) - top - 48 - 12 - 56) / 68);
}
void build(toybox::Screen& screen, const Model& m) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_PREPARE_BOOKS);
  header.back = true;
  pixelui::header(screen, header);
  const auto body = bodyFor(screen.device());
  const auto button = fui::makeRect(body.x, body.bottom() - 48, body.width, 48);
  switch (m.view) {
    case View::Options:
      options(screen, m, body, button);
      break;
    case View::Fonts:
      fonts(screen, m, body, button);
      break;
    case View::Sizes:
      sizes(screen, m, body, button);
      break;
    case View::ConfirmMany:
      confirmation(screen, m, body, button);
      break;
    case View::Progress:
      progress(screen, m, body, button);
      break;
  }
}
void openedProgress(toybox::Screen& s, const Model& m) {
  pixelui::begin(s);
  const auto safe = s.device().safeRect();
  const int width = std::min(360, int(safe.width) - 32);
  const auto box = fui::makeRect(safe.x + (safe.width - width) / 2, safe.y + (safe.height - 156) / 2, width, 156);
  const bool light = chrome::lightHeader;
  const auto foreground = light ? fui::Color::Black : fui::Color::White;
  const auto background = light ? fui::Color::White : fui::Color::Black;
  const auto ink = fui::Paint::solid(foreground);
  const int radius = light ? 6 : 0;
  s.target().fill(box, fui::Paint::solid(background), radius);
  s.target().stroke(box, ink, 1, radius);
  fui::TextStyle style;
  style.font = 1;
  style.color = foreground;
  style.align = fui::TextAlign::Center;
  style.maxLines = 1;
  s.target().text(fui::makeRect(box.x + 12, box.y + 12, width - 24, 30), m.status ? m.status : "", style);
  char percent[16];
  std::snprintf(percent, sizeof(percent), "%u%%", m.total ? unsigned(uint64_t(m.done) * 100 / m.total) : 0);
  s.target().text(fui::makeRect(box.x + 12, box.y + 44, width - 24, 28), percent, style);
  const auto bar = fui::makeRect(box.x + 18, box.y + 82, width - 36, 8);
  s.target().stroke(bar, ink, 1, light ? 2 : 0);
  const int fill = m.total ? int(uint64_t(bar.width - 4) * std::min(m.done, m.total) / m.total) : 0;
  if (fill) s.target().fill(fui::makeRect(bar.x + 2, bar.y + 2, fill, 4), ink, light ? 1 : 0);
  const auto action = fui::makeRect(box.x + 12, box.y + 102, width - 24, 44);
  style.font = 3;
  s.target().text(action, m.running ? tr(STR_CANCEL) : tr(STR_BACK), style);
  dashboard::hit(s, action, m.running ? Cancel : Back);
}
}  // namespace bookprepui
