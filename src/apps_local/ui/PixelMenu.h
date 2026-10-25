#pragma once

#include <FreeInkUI.h>

#include <algorithm>

#include "PixelText.h"

namespace toybox {
class Screen;
}

namespace pixelui {
namespace fui = freeink::ui;
constexpr int kMargin = 10;
constexpr int kRowHeight = 66;
constexpr int kBorder = 4;
constexpr int kPageHeight = 44;
constexpr int kPageReserve = kPageHeight + 16;
constexpr fui::FontId kSmall = 0, kBody = 1, kTitle = 2;

struct Geometry {
  fui::Rect header{}, back{}, body{};
};
Geometry geometry(const fui::DeviceContext& device, int bottomReserve = 0);
struct Header {
  const char* title = "";
  const char* rightLabel = nullptr;
  const char* trailingLabel = nullptr;
  fui::BitmapRef mark{};
  int battery = -1;
  bool charging = false;
  bool showPercent = true;
  bool back = false;
  int titleReserve = 0;
  bool compactBattery = false;
  const char* clockText = nullptr;
  fui::ActionId markAction = 0;
  fui::BitmapRef secondaryMark{};
  fui::ActionId secondaryMarkAction = 0;
  uint16_t actionInputMask = fui::InputTouch;
  bool compactPercent = false;
};
void header(toybox::Screen& screen, Header model);
void header(fui::DrawTarget& target, const fui::DeviceContext& device, const Header& model);
void text(fui::DrawTarget& target, fui::Rect box, const char* label, fui::FontId font = kBody,
          fui::Color color = fui::Color::Black, fui::TextAlign align = fui::TextAlign::Left);
fui::StyleSet rowStyles();
void styleList(fui::ListProps& props, bool dense = false);
fui::Rect rowArea(fui::Rect band, const fui::ListProps& props);
void fitLabel(const fui::DrawTarget& target, char* text, int width, fui::TextStyle& style);

// One reusable display row, irrespective of the folder's size. Activity-owned
// buffers avoid per-frame allocations and keep provider pointers alive until
// the next row is requested. The source's action values and state are retained.
class RowSource {
 public:
  void bind(fui::ListProps& props, fui::DrawTarget& target, fui::Rect band);

 private:
  const fui::ListItem* items = nullptr;
  void (*provider)(void*, uint16_t, fui::ListItem&) = nullptr;
  void* context = nullptr;
  fui::DrawTarget* target = nullptr;
  fui::TextStyle labelStyle{}, subtitleStyle{}, valueStyle{};
  int width = 0, padding = 0, iconSize = 0, gap = 0, valueInset = 0, toggleWidth = 0;
  char label[512]{}, subtitle[224]{}, value[128]{};
  static void provide(void* context, uint16_t index, fui::ListItem& item);
};

template <typename Screen>
void begin(Screen& screen, int bottomReserve = 0) {
  const auto device = screen.device();
  const auto body = geometry(device, bottomReserve).body;
  screen.setContentMarginAbsolute(fui::Insets{body.y, static_cast<int16_t>(device.width - body.right()),
                                              static_cast<int16_t>(device.height - body.bottom()), body.x});
}

template <typename Screen>
void tabs(Screen& screen, const fui::TabItem* items, int count, fui::ActionId action, fui::FontId font = 3) {
  fui::TabBarProps props;
  props.tabs = items;
  props.count = static_cast<uint8_t>(count);
  props.action = action;
  props.inputMask = fui::InputTouch;
  props.text.font = font;
  props.text.maxLines = 2;
  props.text.align = fui::TextAlign::Center;
  props.tabStyles = rowStyles();
  props.tabInset = {};
  props.contentInset = {};
  props.minTouchSize = 0;
  const auto band = screen.takeTop(52, 10);
  fui::tabBar(screen.frame(), band, props);
  screen.target().stroke(band, fui::Paint::solid(fui::Color::Black), kBorder);
}

// The same SDK row measurement as list(), including wrapped names and values.
template <typename Screen>
void list(Screen& screen, fui::ListProps& props, bool fillFrame = false, bool frameSections = false,
          uint8_t frameWidth = kBorder, bool trailingPreview = false) {
  props = screen.resolveListProps(props);
  props.rowRadius = 0;
  props.partialTrailingRow = trailingPreview;
  const auto band = screen.body();
  const auto rows = rowArea(band, props);
  fui::list(screen.frame(), band, props);
  int height = 0;
  int frameTop = -1;
  const auto finishFrame = [&]() {
    if (frameTop >= 0 && height > frameTop)
      screen.target().stroke(fui::makeRect(rows.x, rows.y + frameTop, rows.width, height - frameTop),
                             fui::Paint::solid(fui::Color::Black), frameWidth);
    frameTop = -1;
  };
  const int headingHeight =
      props.headerRowHeight > 0 ? props.headerRowHeight : screen.target().lineHeight(props.headerText.font) + 4;
  for (uint16_t n = props.topIndex; n < props.count; ++n) {
    fui::ListItem item;
    if (props.rowProvider) {
      props.rowProvider(props.rowProviderCtx, n, item);
    } else {
      if (!props.items || n < props.itemsWindowFirst ||
          (props.itemsWindowCount > 0 && n - props.itemsWindowFirst >= props.itemsWindowCount))
        break;
      item = props.items[n - props.itemsWindowFirst];
    }
    const bool heading = !item.isHeader && item.sectionHeading && *item.sectionHeading;
    const int sectionGap = (heading || item.isHeader) && n != props.topIndex ? props.sectionGap : 0;
    const int prefix = heading ? headingHeight + props.rowGap : 0;
    const int rowHeight =
        item.isHeader ? headingHeight
                      : fui::measureListRow(screen.target(), screen.frame().assets(), rows.width, props, item).height;
    const int next = sectionGap + prefix + rowHeight;
    if (height + next > band.height) {
      if (!frameSections || !trailingPreview || band.height - height - sectionGap < props.partialTrailingMinHeight)
        break;
      const auto previousClip = screen.target().clipRect();
      const int left = std::max(int(band.x), int(previousClip.x));
      const int top = std::max(int(band.y), int(previousClip.y));
      const int right = std::min(int(band.right()), int(previousClip.right()));
      const int bottom = std::min(int(band.bottom()), int(previousClip.bottom()));
      if (!screen.target().setClipRect(fui::makeRect(left, top, std::max(0, right - left), std::max(0, bottom - top))))
        break;
      if (heading || item.isHeader) finishFrame();
      if (!item.isHeader && frameTop < 0) frameTop = height + sectionGap + prefix;
      height += next + props.rowGap;
      finishFrame();
      screen.target().setClipRect(previousClip);
      return;
    }
    if (frameSections) {
      if (heading || item.isHeader) finishFrame();
      if (!item.isHeader && frameTop < 0) frameTop = height + sectionGap + prefix;
    }
    height += next + props.rowGap;
  }
  if (frameSections) {
    finishFrame();
    return;
  }
  if (fillFrame) height = band.height;
  if (height > 0)
    screen.target().stroke(fui::makeRect(rows.x, rows.y, rows.width, height), fui::Paint::solid(fui::Color::Black),
                           frameWidth);
}
}  // namespace pixelui
