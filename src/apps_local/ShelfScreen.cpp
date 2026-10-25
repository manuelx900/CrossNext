#include "ShelfScreen.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "ui/BackButton.h"
#include "ui/ExtraIcons.h"
#include "ui/PageIndicator.h"
#include "ui/PixelMenu.h"
#include "ui/ToyboxIcons.h"

namespace shelfui {
const UiLabel kDoneChip = trLabel(STR_UI_DONE_E782E7);
constexpr int kPagerHeight = 64;
constexpr int kPagerReserve = kPagerHeight + 16;

fui::Rect listBand(const fui::DeviceContext& device, bool hasPages) {
  return pixelui::geometry(device, hasPages ? kPagerReserve : 0).body;
}
Paging pagingFor(const fui::DeviceContext& device, int count) {
  const int whole = std::min(9, std::max(1, int(listBand(device, false).height) / pixelui::kRowHeight));
  if (count <= whole) return {whole, 1};
  const int paged = std::min(9, std::max(1, int(listBand(device, true).height) / pixelui::kRowHeight));
  return {paged, pageCountFor(count, paged)};
}

int pageFor(const int selected, const int rowsPerPage) {
  if (rowsPerPage <= 0 || selected <= 0) return 0;
  return selected / rowsPerPage;
}

int pageCountFor(const int itemCount, const int rowsPerPage) {
  if (rowsPerPage <= 0 || itemCount <= 0) return 1;
  return (itemCount + rowsPerPage - 1) / rowsPerPage;
}

int rowForPage(const int page, const int rowsPerPage) {
  if (rowsPerPage <= 0 || page <= 0) return 0;
  return page * rowsPerPage;
}

int resumeRowFor(const int rememberedRow, const int itemCount) {
  if (itemCount <= 0 || rememberedRow <= 0) return 0;
  return rememberedRow >= itemCount ? itemCount - 1 : rememberedRow;
}

int pageStep(const int page, const int pageCount, const int delta) {
  if (pageCount <= 1) return 0;
  // Modulo of a negative left operand is negative in C++, so the step is
  // normalised into 0..pageCount-1 before it is added.
  const int step = ((delta % pageCount) + pageCount) % pageCount;
  const int from = page < 0 ? 0 : page % pageCount;
  return (from + step) % pageCount;
}

int pageStepClamped(const int page, const int pageCount, const int delta) {
  if (pageCount <= 1) return 0;
  const int from = page < 0 ? 0 : (page >= pageCount ? pageCount - 1 : page);
  const int to = from + delta;
  if (to < 0) return 0;
  return to >= pageCount ? pageCount - 1 : to;
}

namespace {
struct Rows {
  const MenuModel& model;
  char label[96]{};
  static void provide(void* context, uint16_t index, fui::ListItem& item) {
    auto& self = *static_cast<Rows*>(context);
    item = self.model.items[index];
    if (self.model.icons && self.model.icons[index]) item.icon = fui::bitmapFromIcon(*self.model.icons[index]);
    item.toggle = self.model.checks != nullptr;
    if (item.toggle) {
      item.icon = fui::bitmapFromIcon(self.model.checks[index] ? extra_checked_32 : extra_unchecked_32);
      item.toggle = false;
    }
    pixelui::copyCaps(self.label, sizeof(self.label), item.label);
    item.label = self.label;
  }
};
}  // namespace

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  const bool choosing = model.checks != nullptr;
  const auto device = screen.device();
  pixelui::Header heading;
  heading.title = model.title;
  heading.back = device.hasTouch;
  char counter[32];
  if (model.pageCount > 1) {
    std::snprintf(counter, sizeof(counter), "%d/%d", model.page + 1, model.pageCount);
    heading.rightLabel = counter;
  }
  if (choosing) heading.trailingLabel = kDoneChip;
  if (!choosing) heading.mark = fui::bitmapFromIcon(icon_go_settings_32);
  pixelui::header(screen.target(), device, heading);
  const auto geometry = pixelui::geometry(device);
  backui::publish(heading.back, geometry.back);
  fui::ButtonProps headerHit;
  headerHit.styles.explicitlySet = true;
  headerHit.action = ActionChoose;
  headerHit.minTouchSize = 0;
  auto headerTarget = geometry.header;
  if (heading.back) {
    headerTarget.x = static_cast<int16_t>(geometry.back.right() + 4);
    headerTarget.width = static_cast<int16_t>(device.width - headerTarget.x);
  }
  screen.button(headerHit, headerTarget);

  pixelui::begin(screen, model.pageCount > 1 ? kPagerReserve : 0);
  if (model.pageCount > 1) {
    const auto body = pixelui::geometry(device).body;
    const int rowsBottom = body.y + std::min(9, int(screen.body().height) / pixelui::kRowHeight) * pixelui::kRowHeight;
    const auto bar = pageui::afterContent(body, rowsBottom, kPagerHeight);
    pageui::build(screen, bar, model.page + 1, model.pageCount, ActionGoToPage, model.page - 1, ActionGoToPage,
                  model.page + 1, pixelui::kBody, kPagerHeight);
  }
  if (model.count <= 0) {
    const auto body = screen.body();
    fui::ButtonProps empty;
    empty.styles.explicitlySet = true;
    empty.action = ActionChoose;
    empty.minTouchSize = 0;
    screen.button(empty, body);
    pixelui::text(screen.target(), fui::makeRect(body.x, body.y + body.height / 2 - 60, body.width, 50),
                  tr(STR_UI_NOTHING_HERE_98776D));
    fui::TextStyle hint;
    hint.font = pixelui::kSmall;
    hint.align = fui::TextAlign::Center;
    hint.maxLines = 3;
    screen.target().text(fui::makeRect(body.x, body.y + body.height / 2, body.width, 96),
                         tr(STR_UI_TAP_TO_CHOOSE_WHAT_THIS_FOLDER_40CD3E), hint);
    return;
  }
  Rows source{model, {}};
  fui::ListProps props;
  pixelui::styleList(props);
  props.rowProvider = &Rows::provide;
  props.rowProviderCtx = &source;
  props.count = static_cast<uint16_t>(model.count);
  props.topIndex = 0;
  props.selectedIndex = static_cast<int16_t>(model.selected < 0 ? -1 : std::min(model.count - 1, model.selected));
  props.action = choosing ? ActionToggleShown : ActionOpen;
  props.labelText.maxLines = 1;
  props.toggleCheckbox = true;
  props.toggleWidth = props.toggleHeight = 32;
  props.scrollIndicator = false;
  pixelui::list(screen, props);
  if (choosing && model.icons) {
    const auto body = screen.body();
    for (int n = 0; n < model.count; ++n)
      if (model.icons[n]) {
        const bool selected = n == props.selectedIndex;
        screen.target().bitmap(fui::makeRect(body.right() - 46, body.y + n * pixelui::kRowHeight + 17, 32, 32),
                               fui::bitmapFromIcon(*model.icons[n]), fui::BitmapMode::Contain,
                               fui::Paint::solid(selected ? fui::Color::White : fui::Color::Black));
      }
  }
}
}  // namespace shelfui
