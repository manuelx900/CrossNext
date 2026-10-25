#pragma once
#include "../ui/ToyboxScreen.h"
namespace wallpapersui {
enum : freeink::ui::ActionId {
  ActionSelectSet = 30,
  ActionSelectAll,
  ActionSelectionDone,
  ActionSelectionApply,
  ActionSelectionCancel,
  ActionPrepareCancel,
  ActionGalleryPage
};
freeink::ui::Rect galleryPager(const freeink::ui::DeviceContext&, bool selecting);
void selectionFooter(toybox::Screen&, bool selecting, int page, int pages, bool allSelected);
void selectionConfirm(toybox::Screen&, int count);
void preparation(toybox::Screen&, int done, int total, bool applying, unsigned frame);
}  // namespace wallpapersui
