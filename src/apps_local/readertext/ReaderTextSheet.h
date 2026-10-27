#pragma once
#include <I18n.h>

#include <algorithm>
#include <cstdint>

#include "../ui/PixelMenu.h"
namespace readertext {
enum Control : int {
  Family,
  SizeDown,
  SizeUp,
  SpacingDown,
  SpacingUp,
  Alignment,
  MarginDown,
  MarginUp,
  Paragraphs,
  Focus,
  Hyphenation,
  Embedded,
  Antialiasing,
  WeightDown,
  WeightUp,
  Placement = 20,
  Percentage,
  Night,
  Refresh,
  FontBack = 30,
  FontPrevious,
  FontNext,
  StyleBack,
  SheetPrevious = 40,
  SheetNext,
  AlignmentLeft = 50,
  AlignmentCenter,
  AlignmentJustify,
  ChapterPick = 2000,
  MorePick = 4000,
  FontPick = 100
};
enum class SheetView : uint8_t { Controls, Families, Style };
struct Model {
  SheetView view = SheetView::Controls;
  int fontPage = 0, fontSelected = 0, fontCount = 0;
  const void* fontContext = nullptr;
  const char* (*fontLabel)(const void*, int) = nullptr;
  int tab = 0, size = 12, sizeIndex = 0, sizeCount = 4, spacing = 1, margin = 5, marginIndex = 0, marginCount = 8;
  const char *family = "", *spacingLabel = "", *alignment = "", *placement = "", *refresh = "";
  int listPage = 0, listCount = 0, currentChapter = -1, alignmentIndex = 0;
  bool contentsAvailable = false;
  const void* listContext = nullptr;
  void (*listLabel)(const void*, int, char*, size_t) = nullptr;
  void (*listValue)(const void*, int, char*, size_t) = nullptr;
  // -1 is a plain row; 0/1 display an unchecked/checked option.
  int (*listCheck)(const void*, int) = nullptr;
  bool fixed = false, statusAvailable = true, paragraphs = false, focus = false, hyphenation = false, embedded = false,
       antialiasing = false, percentage = true, night = false, bold = false;
};
struct Work {
  freeink::ui::ButtonProps button;
  freeink::ui::TabItem tabs[4]{};
  char tabLabels[4][48]{};
  char label[128]{};
  char value[64]{};
};
freeink::ui::Rect sheetRect(const freeink::ui::DeviceContext& device, bool fixed = false);
inline constexpr int kFontRowHeight = 56;
inline int fontRowsPerPage(const freeink::ui::DeviceContext& device) {
  return std::max(1, (int(sheetRect(device).height) - 12 - 44 - 64 - 64 - 56) / kFontRowHeight);
}
inline int sheetRowsPerPage(const freeink::ui::DeviceContext& device, bool fixed = false, int tab = 2) {
  if (tab == 3 && !fixed) return 6;
  return device.width > device.height ? (tab == 2 ? 4 : 2) : (tab == 2 ? 4 : 5);
}
template <typename Screen>
void build(Screen& screen, const Model& model, Work& work);
}  // namespace readertext
#include "ReaderTextSheetImpl.h"
