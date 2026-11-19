#include "PixelMenu.h"

#include <components/controls/header-clock.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../../lib/GfxRenderer/ChromeRegion.h"
#include "BackButton.h"
#include "FitText.h"
#include "LightUi.h"
#include "StatusBattery.h"
#include "ToyboxScreen.h"
#include "ToyboxTokens.h"

namespace pixelui {
namespace {
constexpr auto black = fui::Paint::solid(fui::Color::Black);
constexpr auto white = fui::Paint::solid(fui::Color::White);

bool labelFits(const fui::DrawTarget& target, const char* label, int width, fui::TextStyle style) {
  const char* next = label;
  bool complete = true;
  fui::layoutText(target, fui::makeRect(0, 0, width, 1), label, style, [&](const char* run, fui::Rect) {
    while (*next == ' ' || *next == '\n') ++next;
    const size_t length = std::strlen(run);
    if (std::strncmp(next, run, length) == 0)
      next += length;
    else
      complete = false;
  });
  while (*next == ' ' || *next == '\n') ++next;
  return complete && !*next;
}
}  // namespace

Geometry geometry(const fui::DeviceContext& device, int bottomReserve) {
  const auto safe = device.safeRect();
  Geometry out;
  out.header = fui::makeRect(0, 0, device.width, safe.y + toybox::kMenuHeaderVisibleHeight);
  out.back = fui::makeRect(safe.x + 6, safe.y + 14, 44, 44);
  const int top = out.header.bottom() + 14;
  out.body = fui::makeRect(safe.x + kMargin, top, safe.width - kMargin * 2,
                           std::max(0, safe.bottom() - 8 - bottomReserve - top));
  return out;
}

void fitLabel(const fui::DrawTarget& target, char* label, const int width, fui::TextStyle& style) {
  if (!label || !*label || width <= 0 || labelFits(target, label, width, style)) return;
  const int ceiling = target.lineHeight(style.font);
  // Prose keeps its regular face; display labels descend through the shared cuts.
  if (style.font == 4) return;
  int height = ceiling;
  while (height > 0) {
    fui::FontId next = style.font;
    int nextHeight = 0;
    for (fui::FontId font = 0; font < 4; ++font) {
      const int candidate = target.lineHeight(font);
      if (candidate < height && candidate > nextHeight) {
        next = font;
        nextHeight = candidate;
      }
    }
    if (!nextHeight) return;
    style.font = next;
    if (labelFits(target, label, width, style)) return;
    height = nextHeight;
  }
}

void text(fui::DrawTarget& target, fui::Rect box, const char* label, fui::FontId font, fui::Color color,
          fui::TextAlign align, bool emphasis, LabelCase casing) {
  char copy[192];
  const char* value = label;
  if (!label || std::strlen(label) < sizeof(copy) / 2) {
    copyNatural(copy, sizeof(copy), label, casing);
    value = copy;
  }
  fui::TextStyle style;
  style.color = color;
  style.align = align;
  style.font = font;
  style.bold = emphasis || (!chrome::lightHeader && font == kTitle);
  fittext::draw(target, box, value, style, 2);
}

static fui::Rect paintHeader(fui::DrawTarget&, const fui::DeviceContext&, const Header&, bool light,
                             fui::Rect* = nullptr);

void header(toybox::Screen& screen, Header model) {
  const auto& status = screen.headerStatus();
  if (model.battery < 0) {
    model.battery = status.percent;
    model.charging = status.charging;
    model.showPercent = status.showPercent;
  }
  model.compactBattery = true;
  if (!model.clockText && status.clock[0]) model.clockText = status.clock;
  fui::Rect secondary;
  const auto mark = paintHeader(screen.target(), screen.device(), model, false, &secondary);
  if (model.markAction && !mark.empty()) {
    screen.frame().hit(mark, model.markAction, 0, model.actionInputMask);
    if ((model.actionInputMask & fui::InputFocus) && screen.frame().buttonSelected(model.markAction, 0))
      screen.target().stroke(mark, white, 2);
  }
  if (model.secondaryMarkAction && !secondary.empty()) {
    screen.frame().hit(secondary, model.secondaryMarkAction, 0, model.actionInputMask);
    if ((model.actionInputMask & fui::InputFocus) && screen.frame().buttonSelected(model.secondaryMarkAction, 0))
      screen.target().stroke(secondary, white, 2);
  }
}

void header(fui::DrawTarget& target, const fui::DeviceContext& device, const Header& model) {
  paintHeader(target, device, model, false);
}

void headerWithSelectedColor(fui::DrawTarget& target, const fui::DeviceContext& device, const Header& model,
                             bool outputInverted) {
  paintHeader(target, device, model, chrome::headerIsLight() != outputInverted);
}

static fui::Rect paintHeader(fui::DrawTarget& target, const fui::DeviceContext& device, const Header& model, bool light,
                             fui::Rect* secondary) {
  fui::Rect mark{};
  const auto geometry = pixelui::geometry(device);
  backui::publish(model.back && device.hasTouch, geometry.back);
  const auto safe = device.safeRect();
  const int top = geometry.header.bottom() - 72;
  const auto ink = light ? black : white;
  chrome::recordPaintedHeader(geometry.header.height, light);
  target.fill(geometry.header, light ? white : black);
  int right = safe.right() - (model.homeBattery ? 20 : 14);
  if (model.battery >= 0) {
    const auto icon = statusbattery::headerIcon(right, top);
    const int percent = std::clamp(model.battery, 0, 100);
    statusbattery::draw(target, icon, percent, model.charging, ink.color);
    right = icon.x - 8;
    if (model.showPercent) {
      char percentLabel[8];
      std::snprintf(percentLabel, sizeof(percentLabel), "%d%%", percent);
      const auto font = model.compactBattery && !model.homeBattery ? 3 : kSmall;
      const int width = model.compactPercent ? target.measureText(font, percentLabel, {}).width : 62;
      const auto label =
          model.homeBattery ? statusbattery::headerPercent(icon, top) : fui::makeRect(right - width, top, width, 72);
      if (model.homeBattery) {
        fui::TextStyle style;
        style.font = kSmall;
        style.color = ink.color;
        style.align = fui::TextAlign::Right;
        style.maxLines = 1;
        target.text(label, percentLabel, style);
      } else {
        text(target, label, percentLabel, font, ink.color, fui::TextAlign::Right);
      }
      right = model.homeBattery ? label.x - 10 : right - width - 10;
    }
  }
  if (model.trailingLabel) {
    fui::TextStyle style;
    style.font = kSmall;
    const int width = target.measureText(style.font, model.trailingLabel, style).width + 12;
    text(target, fui::makeRect(right - width, top, width, 72), model.trailingLabel, kSmall, ink.color,
         fui::TextAlign::Right);
    right -= width + 12;
  }
  if (model.mark) {
    const auto icon = fui::makeRect(right - 32, top + 20, 32, 32);
    mark = icon.inset({-6, -6, -6, -6});
    target.bitmap(icon, model.mark, fui::BitmapMode::Contain, ink);
    right = icon.x - 12;
  }
  if (model.secondaryLabel) {
    fui::TextStyle style;
    style.font = kSmall;
    const int width = std::clamp(int(target.measureText(style.font, model.secondaryLabel, style).width) + 24, 44, 160);
    const auto button = fui::makeRect(right - width, top + 14, width, 44);
    if (secondary) *secondary = button;
    target.stroke(button, ink, 1, chrome::lightHeader ? lightui::kControlRadius : 0);
    text(target, button.inset({2, 8, 2, 8}), model.secondaryLabel, kSmall, ink.color, fui::TextAlign::Center);
    right = button.x - 12;
  } else if (model.secondaryMark) {
    const auto icon = fui::makeRect(right - 32, top + 20, 32, 32);
    if (secondary) *secondary = icon.inset({-6, -6, -6, -6});
    target.bitmap(icon, model.secondaryMark, fui::BitmapMode::Contain, ink);
    right = icon.x - 12;
  }
  if (model.rightLabel) {
    fui::TextStyle style;
    style.font = kBody;
    const int width = target.measureText(style.font, model.rightLabel, style).width + 8;
    text(target, fui::makeRect(right - width, top, width, 72), model.rightLabel, kBody, ink.color,
         fui::TextAlign::Right);
    right -= width + 12;
  }
  int left = safe.x + 14;
  if (model.back) {
    backui::paint(target, geometry.back, ink.color);
    left = geometry.back.right() + 4;
  }
  right -= model.titleReserve;
  fui::TextStyle titleStyle;
  titleStyle.font = kBody;
  titleStyle.bold = !chrome::lightHeader && !model.regularTitle;
  char title[192];
  copyNatural(title, sizeof(title), model.title, model.titleCase);
  const int titleEnd = left + target.measureText(titleStyle.font, title, titleStyle).width + 14;
  fui::TextStyle clockStyle;
  clockStyle.font = fui::HEADER_CLOCK_FONT;
  clockStyle.color = ink.color;
  const auto clock = fui::headerClockRect(target, fui::makeRect(titleEnd, top, std::max(0, right - titleEnd), 72),
                                          model.clockText, clockStyle);
  if (!clock.empty()) {
    target.text(clock, model.clockText, clockStyle);
    right = clock.x - 14;
  }
  titleStyle.color = ink.color;
  fittext::draw(target, fui::makeRect(left, top, std::max(0, right - left), 72), title, titleStyle, 2);
  return mark;
}

fui::StyleSet rowStyles() {
  if (chrome::lightHeader) return lightui::controlStyles(true);
  auto styles = toybox::rowStyles();
  styles.normal.borderWidth = 1;
  styles.normal.radius = 0;
  styles.selected.radius = 0;
  styles.focused = styles.active = styles.selected;
  return styles;
}

fui::StyleSet buttonStyles() { return chrome::lightHeader ? lightui::controlStyles() : toybox::buttonStyles(); }

void styleList(fui::ListProps& props, bool dense) {
  props.rowStyles = rowStyles();
  props.rowHeight = kRowHeight;
  props.rowGap = 0;
  props.rowInset = 0;
  props.rowRadius = 0;
  props.rowPaddingY = 8;
  props.sidePadding = 18;
  props.textGap = 18;
  props.iconSize = 40;
  props.labelText.font = dense ? kSmall : kBody;
  props.labelText.bold = true;
  props.subtitleText.bold = false;
  props.labelText.maxLines = 2;
  props.subtitleText.font = kSmall;
  props.subtitleText.maxLines = 2;
  props.valueText.font = kSmall;
  props.valueText.align = fui::TextAlign::Right;
  props.valueInset = 0;
  props.balanceWrappedLabelWithValue = false;
  props.selectionMarker = fui::SelectionMarker::None;
  props.separatorPaint = fui::Paint::solid(fui::Color::Transparent);
  props.partialTrailingRow = false;
  props.scrollIndicatorWidth = 3;
  props.scrollIndicatorInset = 0;
  props.scrollIndicatorSide = 0;
  if (chrome::lightHeader) {
    props.labelText.font = 0;
    props.labelText.bold = false;
    props.subtitleText.font = 4;
    props.rowHeight = 72;
    props.selectionMarker = fui::SelectionMarker::Underline;
    props.markerThickness = 1;
    props.markerPaint = lightui::rule;
    props.separatorPaint = lightui::rule;
  }
}

fui::Rect rowArea(fui::Rect band, const fui::ListProps& props) {
  // The SDK reserves the track plus 2px of air for every nav-managed list.
  const bool reserved = props.nav || props.count > fui::listVisibleRows(band, props.rowHeight, props.rowGap);
  if (props.scrollIndicator && reserved) band.width = static_cast<int16_t>(band.width - 5);
  return band;
}

namespace {
bool copyMenuLabel(char* out, size_t size, const char* source) {
  return copyNatural(out, size, source, LabelCase::Preserve);
}
}  // namespace

void RowSource::bind(fui::ListProps& props, fui::DrawTarget& drawTarget, fui::Rect band) {
  items = props.items;
  provider = props.rowProvider;
  context = props.rowProviderCtx;
  target = &drawTarget;
  labelStyle = props.labelText;
  subtitleStyle = props.subtitleText;
  valueStyle = props.valueText;
  width = rowArea(band, props).width;
  padding = props.sidePadding;
  iconSize = props.iconSize;
  gap = props.textGap;
  valueInset = props.valueInset;
  toggleWidth = props.toggleWidth;
  const int minimum = std::max(1, int(drawTarget.lineHeight(3)));
  const uint16_t end = std::min(int(props.count), int(props.topIndex) + int(band.height) / minimum + 1);
  for (uint16_t n = props.topIndex; n < end; ++n) {
    fui::ListItem item;
    if (provider)
      provider(context, n, item);
    else if (items && n >= props.itemsWindowFirst &&
             (!props.itemsWindowCount || n - props.itemsWindowFirst < props.itemsWindowCount))
      item = items[n - props.itemsWindowFirst];
    else
      break;
    int available = width - padding * 2 - (item.icon ? iconSize + gap : 0);
    if (item.value) {
      copyMenuLabel(value, sizeof(value), item.value);
      fitLabel(drawTarget, value, std::max(24, available * 2 / 5), valueStyle);
      available -= drawTarget.measureText(valueStyle.font, value, valueStyle).width + valueInset + gap;
    }
    if (item.toggle) available -= toggleWidth + valueInset + gap;
    if (item.label) {
      copyMenuLabel(label, sizeof(label), item.label);
      fitLabel(drawTarget, label, available, labelStyle);
    }
    if (item.subtitle) {
      copyMenuLabel(subtitle, sizeof(subtitle), item.subtitle);
      fitLabel(drawTarget, subtitle, width - padding * 2, subtitleStyle);
    }
  }
  props.labelText = labelStyle;
  props.subtitleText = subtitleStyle;
  props.valueText = valueStyle;
  props.items = nullptr;
  props.rowProvider = &RowSource::provide;
  props.rowProviderCtx = this;
}

void RowSource::provide(void* ctx, uint16_t index, fui::ListItem& item) {
  auto& self = *static_cast<RowSource*>(ctx);
  if (self.provider)
    self.provider(self.context, index, item);
  else
    item = self.items[index];
  // Font choices belong to bind(), after the caller finalizes its viewport.
  if (item.value && std::strlen(item.value) < sizeof(self.value) / 2) {
    copyMenuLabel(self.value, sizeof(self.value), item.value);
    item.value = self.value;
  }
  if (item.label && std::strlen(item.label) < sizeof(self.label) / 2) {
    copyMenuLabel(self.label, sizeof(self.label), item.label);
    item.label = self.label;
  }
  if (item.subtitle && std::strlen(item.subtitle) < sizeof(self.subtitle) / 2) {
    copyMenuLabel(self.subtitle, sizeof(self.subtitle), item.subtitle);
    item.subtitle = self.subtitle;
  }
}
}  // namespace pixelui
