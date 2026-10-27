#pragma once

#include <FreeInkUIIcon.h>

#include <algorithm>

#include "../ui/PageIndicator.h"

namespace dictionaryui {
namespace fui = freeink::ui;
enum Action : fui::ActionId { Back = 1, Save, Previous, Next };
struct Layout {
  fui::Rect header, body, footer;
};
inline Layout layout(const fui::DeviceContext& device) {
  const auto safe = device.safeRect();
  const auto header = pixelui::geometry(device).header;
  const int footer = 48, gap = 12, side = 16, bottomMargin = 8;
  const int bodyTop = header.bottom() + gap;
  const int footerTop = safe.bottom() - footer - bottomMargin;
  return {header, fui::makeRect(safe.x + side, bodyTop, safe.width - side * 2, std::max(0, footerTop - gap - bodyTop)),
          fui::makeRect(safe.x + side, footerTop, safe.width - side * 2, footer)};
}
struct Model {
  const char* word = "";
  const char* saveLabel = nullptr;
  int page = 1, pages = 1;
};
struct Work {
  pixelui::Header header;
};
template <typename Screen>
void build(Screen& screen, const Model& model, Work& work) {
  const auto area = layout(screen.device());
  auto& header = work.header;
  header = {};
  header.title = model.word;
  header.back = true;
  fui::TextStyle text;
  text.font = 0;
  const auto safe = screen.device().safeRect();
  const int saveWidth = model.saveLabel ? screen.target().measureText(0, model.saveLabel, text).width + 32 : 0;
  header.titleReserve = saveWidth ? saveWidth + 12 : 0;
  pixelui::header(screen.target(), screen.device(), header);
  screen.frame().hit(pixelui::geometry(screen.device()).back, Back, 0, fui::InputTouch);
  if (model.saveLabel) {
    const auto button = fui::makeRect(safe.right() - saveWidth - 12, area.header.bottom() - 60, saveWidth, 48);
    screen.target().fill(button, fui::Paint::solid(fui::Color::White));
    pixelui::text(screen.target(), button, model.saveLabel, 0, fui::Color::Black, fui::TextAlign::Center);
    screen.frame().hit(button, Save, 0, fui::InputTouch);
  }
  pageui::build(screen, area.footer, model.page, model.pages, Previous, 0, Next, 0);
}
}  // namespace dictionaryui
