#pragma once
#include <I18n.h>
#include <Icon.h>

#include "ui/ToyboxScreen.h"

namespace shelfui {
namespace fui = freeink::ui;
enum : fui::ActionId {
  ActionOpen = 1,
  ActionGoToPage = 3,
  ActionChoose = 4,
  ActionToggleShown = 5,
};
extern const UiLabel kDoneChip;
struct Paging {
  int rowsPerPage;
  int pageCount;
};
struct MenuModel {
  const freeink::Icon* const* icons = nullptr;
  const char* title = "";
  const freeink::Icon* mark = nullptr;
  const bool* checks = nullptr;
  // A single page; actionValue remains the absolute row in the current mode.
  const fui::ListItem* items = nullptr;
  int count = 0;
  int selected = 0;
  int page = 0;
  int pageCount = 1;
};
Paging pagingFor(const fui::DeviceContext& device, int count);
fui::Rect listBand(const fui::DeviceContext& device, bool hasPages);
int pageFor(int selected, int rowsPerPage);
int pageCountFor(int itemCount, int rowsPerPage);
int rowForPage(int page, int rowsPerPage);
int resumeRowFor(int rememberedRow, int itemCount);
int pageStep(int page, int pageCount, int delta);
int pageStepClamped(int page, int pageCount, int delta);
void buildMenu(toybox::Screen& screen, const MenuModel& model);
}  // namespace shelfui
