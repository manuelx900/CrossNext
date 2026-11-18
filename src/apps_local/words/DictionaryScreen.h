#pragma once

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>

#include "../ui/PageIndicator.h"
#include "WordHeading.h"

namespace dictionaryui {
namespace fui = freeink::ui;
enum Action : fui::ActionId { Back = 1, Save, Previous, Next };
struct Layout {
  fui::Rect header, body, footer;
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
          fui::makeRect(safe.x + side, footerTop, safe.width - side * 2, footer)};
}
struct Model {
  const char* word = "";
  const char* saveLabel = nullptr;
  int page = 1, pages = 1;
  bool lookup = false;
};
struct Work {
  pixelui::Header header;
  char title[192]{};
};
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
