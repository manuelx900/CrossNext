#pragma once
#include <Bitmap.h>
#include <FreeInkUI.h>
#include <HalStorage.h>

#include <memory>
class GfxRenderer;
struct RecentBook;
class BookCoverPainter {
 public:
  BookCoverPainter(GfxRenderer& renderer, freeink::ui::DrawTarget& target) : renderer(renderer), target(target) {}
  void paint(const RecentBook& book, freeink::ui::Rect slot, int taper = 0);

 private:
  GfxRenderer& renderer;
  freeink::ui::DrawTarget& target;
  HalFile coverFile;
  Bitmap coverBitmap{coverFile};
  std::unique_ptr<uint8_t[]> row;
  size_t rowCapacity = 0;
  bool paintBitmap(freeink::ui::Rect slot, int taper);
};
