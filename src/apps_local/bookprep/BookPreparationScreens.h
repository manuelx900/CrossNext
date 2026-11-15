#pragma once
#include <Epub/ReaderRenderSpec.h>

#include "../ui/ToyboxScreen.h"
#include "FontSelection.h"

namespace bookprepui {
enum class View : uint8_t { Options, Fonts, Sizes, ConfirmMany, Progress };
enum : freeink::ui::ActionId {
  Start = 1,
  Back,
  Cancel,
  ToggleFont,
  CurrentFont,
  AllFonts,
  NoFonts,
  Page,
  ConfirmStart,
  ToggleFormat,
  ChooseFonts,
  ChooseSizes,
  ChooseSize,
  AllSizes,
  Done
};
struct Model {
  EpubEngine engine = EpubEngine::Modern;
  View view = View::Progress;
  const bookprep::FontSelection* fonts = nullptr;
  int currentFont = 0, page = 0;
  uint8_t pointSize = 14;
  uint8_t formats = bookprep::AllFormats;
  bookprep::SizeMode sizeMode = bookprep::SizeMode::Single;
  bookprep::PointSizes sizes;
  const char* status = nullptr;
  const char* detail = nullptr;
  const char* note = nullptr;
  const char* counts = nullptr;
  const char* font = nullptr;
  uint64_t done = 0, total = 0;
  bool running = false;
};
int pageSize(const freeink::ui::DeviceContext& device, View view = View::Fonts);
void build(toybox::Screen& screen, const Model& model);
void openedProgress(toybox::Screen& screen, const Model& model);
}  // namespace bookprepui
