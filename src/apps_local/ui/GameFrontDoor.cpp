#include "GameFrontDoor.h"

#include <cstring>

#include "PixelText.h"
#include "ToyboxIcons.h"

namespace frontdoor {
Layout layout(const fui::DeviceContext& device, int count, bool detail, bool note, bool compact) {
  Layout out;
  const auto body = pixelui::geometry(device).body;
  count = std::clamp(count, 1, 5);
  const bool wide = body.width > body.height;
  const int gap = 8, rowHeight = wide ? 56 : 62;
  const int rowWidth = wide ? (body.width - 28) / 2 : body.width;
  const int rowX = wide ? body.right() - rowWidth : body.x;
  const int noteHeight = note ? 60 : 0;
  const int blockHeight = count * (rowHeight + gap) - gap;
  const int wideTop = gamepanel::centeredBlock(body, blockHeight).y;
  const int rowY = wide ? wideTop : body.bottom() - noteHeight - count * (rowHeight + gap) + gap;
  for (int n = 0; n < count; ++n) out.rows[n] = fui::makeRect(rowX, rowY + n * (rowHeight + gap), rowWidth, rowHeight);
  const int width = wide ? rowWidth : body.width;
  out.heading = fui::makeRect(body.x, body.y + (wide && compact ? 0 : 4), width, 48);
  if (detail) out.detail = fui::makeRect(body.x, out.heading.bottom() + 2, width, 30);
  const int top = detail ? out.detail.bottom() + 14 : out.heading.bottom() + (wide && compact ? 8 : 16);
  const int bottom = wide ? body.bottom() - noteHeight - (compact ? 16 : 0) : rowY - 18;
  out.preview = fui::makeRect(body.x, top, width, std::max(0, bottom - top));
  if (note) out.footnote = fui::makeRect(body.x, body.bottom() - noteHeight + 8, width, noteHeight - 8);
  return out;
}
void begin(toybox::Screen& screen, const char* title, const char* right) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.rightLabel = right;
  header.back = true;
  pixelui::header(screen, header);
  backui::publish(true, pixelui::geometry(screen.device()).back);
}
void text(toybox::Screen& s, fui::Rect box, const char* label, int font, bool centered, int lines) {
  if (!label || box.empty()) return;
  char capital[160];
  if (font == 1) {
    pixelui::copyCaps(capital, sizeof(capital), label);
    label = capital;
  }
  fui::TextStyle style;
  style.font = font;
  style.maxLines = lines;
  style.align = centered ? fui::TextAlign::Center : fui::TextAlign::Left;
  const int candidates[] = {font, 0, 4};
  for (int candidate : candidates) {
    style.font = candidate;
    if (s.target().lineHeight(candidate) > box.height) continue;
    style.maxLines = std::min(lines, std::max(1, int(box.height) / int(s.target().lineHeight(candidate))));
    bool fits = true;
    fui::layoutText(s.target(), box, label, style, [&](const char* run, fui::Rect) {
      if (std::strstr(run, "…")) fits = false;
    });
    if (fits) break;
  }
  s.target().text(box, label, style);
}
void rows(toybox::Screen& s, const Layout& g, const Row* rows, int count, int selected) {
  for (int n = 0; n < count && n < 5; ++n) {
    const auto box = g.rows[n];
    const auto& row = rows[n];
    const bool primary = s.frame().buttonSelected(row.action, row.value, n == selected);
    const auto paint = primary ? dashboard::paper : dashboard::ink;
    s.target().fill(box, primary ? dashboard::ink : dashboard::paper);
    s.target().stroke(box, dashboard::ink, pixelui::kBorder);
    const int side = std::min(38, int(box.height) - 14);
    const auto mark = fui::makeRect(box.x + 14, box.y + (box.height - side) / 2, side, side);
    if (row.symbol == Symbol::Nearby || row.symbol == Symbol::Settings)
      s.target().bitmap(mark, fui::bitmapFromIcon(row.symbol == Symbol::Nearby ? icon_nearby_32 : icon_go_settings_32),
                        fui::BitmapMode::Contain, paint);
    else {
      const auto symbol = row.symbol == Symbol::Restart ? gamepanel::Mark::Restart
                          : row.symbol == Symbol::Grid  ? gamepanel::Mark::Grid
                          : row.symbol == Symbol::Help  ? gamepanel::Mark::Help
                                                        : gamepanel::Mark::Play;
      gamepanel::mark(s, mark, symbol, primary);
    }
    char label[160];
    pixelui::copyCaps(label, sizeof(label), row.label);
    fui::TextStyle style;
    style.font = 1;
    style.color = primary ? fui::Color::White : fui::Color::Black;
    int detailWidth = 0;
    if (row.detail && *row.detail)
      detailWidth = std::min(int(box.width) * 2 / 5, int(s.target().measureText(0, row.detail, {}).width) + 8);
    const auto labelBox =
        fui::makeRect(mark.right() + 16, box.y + 4, box.right() - mark.right() - 50 - detailWidth, box.height - 8);
    for (int font : {1, 0, 4}) {
      style.font = font;
      style.maxLines = std::min(2, std::max(1, int(labelBox.height) / int(s.target().lineHeight(font))));
      bool fits = s.target().lineHeight(font) <= labelBox.height;
      fui::layoutText(s.target(), labelBox, label, style, [&](const char* run, fui::Rect) {
        if (std::strstr(run, "…")) fits = false;
      });
      if (fits) break;
    }
    s.target().text(labelBox, label, style);
    if (detailWidth) {
      style.font = 0;
      style.align = fui::TextAlign::Right;
      s.target().text(fui::makeRect(box.right() - detailWidth - 30, box.y + 4, detailWidth, box.height - 8), row.detail,
                      style);
    }
    const int x = box.right() - 18, y = box.y + box.height / 2;
    s.target().line({int16_t(x - 5), int16_t(y - 9)}, {int16_t(x + 4), int16_t(y)}, 3, paint);
    s.target().line({int16_t(x + 4), int16_t(y)}, {int16_t(x - 5), int16_t(y + 9)}, 3, paint);
    s.frame().hit(box, row.action, row.value, fui::InputDefault);
  }
}
}  // namespace frontdoor
