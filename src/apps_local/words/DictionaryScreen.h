#pragma once

#include <FreeInkUIIcon.h>

#include <algorithm>

#include "../../components/icons/readerToolbarIcons.h"
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
  const int footer = 56, gap = 12, side = 14, bottomMargin = 8;
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
  fui::HeaderProps header;
};
template <typename Screen>
void build(Screen& screen, const Model& model, Work& work) {
  const auto area = layout(screen.device());
  auto& header = work.header;
  header = {};
  header.title = model.word;
  header.titleText = screen.theme().bodyText;
  header.titleText.color = fui::Color::White;
  header.titleText.bold = true;
  header.titleText.maxLines = 1;
  header.sidePadding = 12;
  header.leadingIcon = fui::bitmapFromIcon(icon_reader_back_24);
  header.leadingAction = Back;
  header.leadingStyles = fui::plainStyles(fui::Paint::solid(fui::Color::White));
  header.leadingSize = 44;
  header.trailingLabel = model.saveLabel;
  header.trailingAction = model.saveLabel ? Save : fui::NO_ACTION;
  header.trailingText = screen.theme().smallText;
  header.trailingText.bold = true;
  header.trailingStyles = pixelui::rowStyles();
  header.trailingStyles.normal.borderWidth = 3;
  header.trailingStyles.selected = header.trailingStyles.normal;
  header.trailingStyles.selected.borderWidth = 4;
  header.trailingStyles.focused = header.trailingStyles.active = header.trailingStyles.selected;
  header.trailingSize = 44;
  header.actionOffsetY = (area.header.height - header.trailingSize) / 2 - 4;
  header.styles = fui::plainStyles(fui::Paint::solid(fui::Color::White));
  header.styles.normal.background = fui::Paint::solid(fui::Color::Black);
  header.minTouchSize = 44;
  fui::header(screen.frame(), area.header, header);
  pageui::build(screen, area.footer, model.page, model.pages, Previous, 0, Next, 0);
}
}  // namespace dictionaryui
