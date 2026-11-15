#pragma once
#include <I18n.h>

#include <algorithm>
#include <cstdint>

#include "../ui/PixelMenu.h"
namespace readertext {
enum Control : int {
  NoPage = -1,
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
  AllChapters,
  AlignmentLeft = 50,
  AlignmentCenter,
  AlignmentJustify,
  AlignmentRight,
  AlignmentBook,
  LayoutPrevious = 60,
  LayoutNext,
  IndentDown,
  IndentUp,
  WordDown,
  WordUp,
  CharacterDown,
  CharacterUp,
  AlignmentDown,
  AlignmentUp,
  CopyDefaults = 80,
  DefaultsInfo,
  ChapterPick = 2000,
  MorePick = 4000,
  FontPick = 100
};
enum class SheetView : uint8_t { Controls, Families, Style };
inline Control pageSwipeControl(int tab, SheetView view, int direction) {
  if (!direction) return NoPage;
  if (view == SheetView::Families) return direction > 0 ? FontNext : FontPrevious;
  if (view != SheetView::Controls) return NoPage;
  if (tab == 1) return direction > 0 ? LayoutNext : LayoutPrevious;
  if (tab == 2 || tab == 3) return direction > 0 ? SheetNext : SheetPrevious;
  return NoPage;
}

struct Model {
  SheetView view = SheetView::Controls;
  int fontPage = 0, fontSelected = 0, fontCount = 0;
  const void* fontContext = nullptr;
  const char* (*fontLabel)(const void*, int) = nullptr;
  int tab = 0, size = 12, sizeIndex = 0, sizeCount = 4, spacing = 2, spacingCount = 7, margin = 5, marginIndex = 0,
      marginCount = 8;
  const char *family = "", *spacingLabel = "", *alignment = "", *placement = "", *refresh = "";
  int listPage = 0, listCount = 0, currentChapter = -1, alignmentIndex = 0;
  bool contentsAvailable = false;
  bool globalSettings = false;
  bool standalone = false;
  bool useDefaults = true;
  int minTop = 0;
  int layoutPage = 0, indent = 2, wordSpacing = 100, characterSpacing = 2;
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
freeink::ui::Rect sheetRect(const freeink::ui::DeviceContext& device, bool fixed = false, int minTop = 0);
inline constexpr int kFontRowHeight = 56;
inline int fontRowHeight() { return chrome::lightHeader ? 50 : kFontRowHeight; }
inline int fontColumns(const freeink::ui::DeviceContext& device) {
  return chrome::lightHeader && device.width > device.height ? 2 : 1;
}
inline int fontRowsPerPage(const freeink::ui::DeviceContext& device, int minTop = 0) {
  if (chrome::lightHeader) {
    const int grip = device.width > device.height ? 24 : 36;
    const int available = int(sheetRect(device, false, minTop).height) - 12 - grip - 64 - 64 - 56;
    return std::max(1, std::min(6, available / fontRowHeight() * fontColumns(device)));
  }
  return std::max(1, (int(sheetRect(device, false, minTop).height) - 12 - 44 - 64 - 64 - 56) / kFontRowHeight);
}
inline int sheetRowsPerPage(const freeink::ui::DeviceContext& device, bool fixed = false, int tab = 2) {
  if (tab == 3 && !fixed) return 5;
  return device.width > device.height ? 2 : (tab == 2 ? 4 : 5);
}
template <typename Screen>
void build(Screen& screen, const Model& model, Work& work);
}  // namespace readertext
#include "ReaderTextSheetImpl.h"
