#pragma once

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>

#include "../ui/PageIndicator.h"
#include "WordHeading.h"

namespace dictionaryui {
namespace fui = freeink::ui;
enum Action : fui::ActionId { Back = 1, Save, Previous, Next, Close };
struct Layout {
  fui::Rect header, body, footer, panel;
};
inline Layout layout(const fui::DeviceContext& device, const bool lookup = false) {
  const auto safe = device.safeRect();
  const auto header = pixelui::geometry(device).header;
  const int footer = 48, gap = 12, side = 16, bottomMargin = 8;
  const int bodyTop = header.bottom() + gap;
  const int footerTop = safe.bottom() - footer - bottomMargin;
  return {header,
          fui::makeRect(safe.x + side, bodyTop, safe.width - side * 2,
                        std::max(0, footerTop - gap - bodyTop - (lookup ? 32 : 0))),
          fui::makeRect(safe.x + side, footerTop, safe.width - side * 2, footer),
          {}};
}
inline Layout overlayLayout(const fui::DeviceContext& device, const fui::Rect selection) {
  const auto safe = device.safeRect();
  constexpr int inset = 14, gap = 10, headerHeight = 48, footerHeight = 44, padding = 12;
  const int width = safe.width - 2 * inset;
  const int height = std::min(370, safe.height * 47 / 100);
  const int above = selection.y - safe.y - 2 * gap;
  const int below = safe.bottom() - selection.bottom() - 2 * gap;
  const int proposed = below >= height || below >= above ? selection.bottom() + gap : selection.y - gap - height;
  const int y = std::clamp(proposed, int(safe.y) + gap, safe.bottom() - gap - height);
  const auto panel = fui::makeRect(safe.x + inset, y, width, height);
  const int innerX = panel.x + padding, innerWidth = panel.width - 2 * padding;
  return {fui::makeRect(panel.x + 1, panel.y + 1, panel.width - 2, headerHeight),
          fui::makeRect(innerX, y + headerHeight + gap, innerWidth,
                        std::max(0, height - headerHeight - footerHeight - 2 * gap)),
          fui::makeRect(innerX, panel.bottom() - footerHeight - 3, innerWidth, footerHeight), panel};
}
struct Model {
  const char* word = "";
  const char* saveLabel = nullptr;
  int page = 1, pages = 1;
  bool lookup = false;
  bool rounded = false;
};
struct Work {
  pixelui::Header header;
  char title[192]{};
};
template <typename Screen>
void buildOverlay(Screen& screen, const Layout& area, const Model& model, Work& work) {
  auto& target = screen.target();
  const auto black = fui::Paint::solid(fui::Color::Black), white = fui::Paint::solid(fui::Color::White);
  const auto ink = model.rounded ? black : white;
  const auto textColor = model.rounded ? fui::Color::Black : fui::Color::White;
  const int radius = model.rounded ? 8 : 0;
  target.fill(area.panel, white, radius);
  if (!model.rounded) target.fill(area.header, black);
  target.stroke(area.panel, black, model.rounded ? 1 : 2, radius);
  if (model.rounded) target.fill(fui::makeRect(area.body.x, area.header.bottom(), area.body.width, 1), black);
  target.fill(fui::makeRect(area.body.x, area.footer.y - 3, area.body.width, 1), black);

  constexpr int closeWidth = 64, closeRadius = 11, saveGap = 4, titleGap = 8;
  const auto close = fui::makeRect(area.header.right() - closeWidth, area.header.y, closeWidth, area.header.height);
  const int16_t cx = close.x + close.width / 2, cy = close.y + close.height / 2;
  target.line({int16_t(cx - closeRadius), int16_t(cy - closeRadius)},
              {int16_t(cx + closeRadius), int16_t(cy + closeRadius)}, 3, ink);
  target.line({int16_t(cx + closeRadius), int16_t(cy - closeRadius)},
              {int16_t(cx - closeRadius), int16_t(cy + closeRadius)}, 3, ink);
  screen.frame().hit(close, Close, 0, fui::InputTouch);

  int titleRight = close.x;
  if (model.saveLabel) {
    fui::TextStyle style;
    style.font = pixelui::kSmall;
    const int width = std::clamp(target.measureText(pixelui::kSmall, model.saveLabel, style).width + 20, 42,
                                 int(area.header.width) / 2);
    const auto save = fui::makeRect(close.x - saveGap - width, area.header.y, width, area.header.height);
    pixelui::text(target, save.inset({0, 2, 0, 0}), model.saveLabel, pixelui::kSmall, textColor, fui::TextAlign::Right);
    screen.frame().hit(save, Save, 0, fui::InputTouch);
    titleRight = save.x;
  }
  copyWordHeading(work.title, sizeof(work.title), model.word);
  fui::TextStyle heading;
  heading.font = pixelui::kBody;
  heading.color = textColor;
  heading.maxLines = 1;
  target.text(
      fui::makeRect(area.body.x, area.header.y, std::max(0, titleRight - area.body.x - titleGap), area.header.height),
      work.title, heading);
  pageui::build(screen, area.footer, model.page, model.pages, Previous, 0, Next, 0, pixelui::kSmall, 34);
}
template <typename Screen>
void build(Screen& screen, const Model& model, Work& work) {
  const auto area = layout(screen.device(), model.lookup);
  auto& header = work.header;
  header = {};
  copyWordHeading(work.title, sizeof(work.title), model.word);
  header.title = work.title;
  header.titleCase = pixelui::LabelCase::Verbatim;
  header.back = true;
  fui::TextStyle text;
  text.font = 0;
  text.align = fui::TextAlign::Center;
  char caption[128]{};
  if (model.saveLabel) pixelui::copyNatural(caption, sizeof(caption), model.saveLabel);
  const auto safe = screen.device().safeRect();
  const int saveWidth = model.saveLabel ? screen.target().measureText(0, caption, text).width + 32 : 0;
  header.titleReserve = saveWidth ? saveWidth + 12 : 0;
  pixelui::header(screen.target(), screen.device(), header);
  screen.frame().hit(pixelui::geometry(screen.device()).back, Back, 0, fui::InputTouch);
  if (model.saveLabel) {
    const auto button = fui::makeRect(safe.right() - saveWidth - 12, area.header.bottom() - 60, saveWidth, 48);
    screen.target().fill(button, fui::Paint::solid(fui::Color::White));
    const auto ink = screen.target().inkBounds(0, caption, text);
    auto label = button;
    label.y += (button.height - ink.height) / 2 - ink.y;
    label.height = screen.target().lineHeight(0);
    screen.target().text(label, caption, text);
    screen.frame().hit(button, Save, 0, fui::InputTouch);
  }
  if (model.lookup) {
    pixelui::text(screen.target(), fui::makeRect(area.body.x, area.footer.y - 32, area.body.width, 28),
                  tr(STR_DICT_RECURSIVE_HINT), pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
  }
  pageui::build(screen, area.footer, model.page, model.pages, Previous, 0, Next, 0);
}
}  // namespace dictionaryui
