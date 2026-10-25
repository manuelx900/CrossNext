#include <I18n.h>
#include <Logging.h>

#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "WallpapersActivity.h"

namespace {
constexpr char kReadyPath[] = "/wallpapers/.thumbs/.ready-v3";
struct Ready {
  uint64_t stamp = 0;
  uint32_t magic = 0x33525057;
  uint16_t count = 0, width = 0, height = 0, errors = 0;
  uint8_t reserved[4]{};
  uint8_t usable[32]{};
};
static_assert(sizeof(Ready) == 56);
}  // namespace
void WallpapersActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  job_ = Job::Discover;
  view_ = View::Preparing;
  painted_ = false;
  fetchTotal_ = 0;
  progressAt_ = millis();
  requestUpdate();
}
bool WallpapersActivity::cachePrepared(bool write) {
  const auto geom = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext());
  Ready ready;
  if (!write) {
    HalFile file;
    if (!Storage.openFileForRead("WALL", kReadyPath, file) || file.fileSize64() != sizeof(ready) ||
        file.read(&ready, sizeof(ready)) != sizeof(ready))
      return false;
    if (ready.magic != 0x33525057 || ready.stamp != libraryStamp_ || ready.count != names_.size() ||
        ready.width != geom.cellW || ready.height != geom.cellH)
      return false;
    prepareErrors_ = ready.errors;
    usableSet_.reset();
    for (size_t n = 0; n < names_.size() && n < usableSet_.size(); ++n)
      usableSet_.set(n, (ready.usable[n / 8] & (1u << (n % 8))) != 0);
    return prepareErrors_ == 0;
  }
  ready.stamp = libraryStamp_;
  ready.count = uint16_t(names_.size());
  ready.width = geom.cellW;
  ready.height = geom.cellH;
  ready.errors = uint16_t(prepareErrors_);
  for (size_t n = 0; n < names_.size() && n < usableSet_.size(); ++n)
    if (usableSet_.test(n)) ready.usable[n / 8] |= uint8_t(1u << (n % 8));
  Storage.ensureDirectoryExists("/wallpapers/.thumbs");
  constexpr char part[] = "/wallpapers/.thumbs/.ready-v3.part";
  HalFile file;
  if (!Storage.openFileForWrite("WALL", part, file)) return false;
  const bool saved = file.write(&ready, sizeof(ready)) == sizeof(ready);
  file.close();
  if (!saved || !Storage.replaceFile(part, kReadyPath)) {
    Storage.remove(part);
    return false;
  }
  return true;
}
void WallpapersActivity::preparationStep() {
  switch (job_) {
    case Job::None:
      return;
    case Job::Discover: {
      initializeLibrary();
      prepareAt_ = prepareErrors_ = 0;
      fetchTotal_ = int(names_.size());
      const auto geom = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext());
      prepareW_ = geom.cellW;
      prepareH_ = geom.cellH;
      job_ = names_.empty() || cachePrepared(false) ? Job::Page : Job::Thumbnails;
      view_ = View::Preparing;
      if (job_ == Job::Thumbnails) {
        painted_ = false;
        ++animationFrame_;
        requestUpdate();
      }
      return;
    }
    case Job::Thumbnails: {
      const auto geom = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext());
      if (prepareW_ != geom.cellW || prepareH_ != geom.cellH) {
        prepareW_ = geom.cellW;
        prepareH_ = geom.cellH;
        prepareAt_ = prepareErrors_ = 0;
      }
      if (prepareAt_ < int(names_.size())) {
        const auto& name = names_[size_t(prepareAt_)];
        const auto path = sourcePathFor(name);
        const bool usable = !path.empty() && thumbFor(name, path, geom.cellW, geom.cellH).ok;
        usableSet_.set(size_t(prepareAt_), usable);
        if (!usable) ++prepareErrors_;
        ++prepareAt_;
        ++animationFrame_;
        if (prepareAt_ == 1 || prepareAt_ == fetchTotal_ || uint32_t(millis() - progressAt_) >= 200) {
          requestUpdateAndWait();
          progressAt_ = millis();
        }
        return;
      }
      cachePrepared(true);
      job_ = Job::Page;
      return;
    }
    case Job::Page: {
      ensureThumbsForPage();
      job_ = Job::None;
      pickView();
      interactionsReady_ = false;
      requestUpdate();
      return;
    }
    case Job::Apply: {
      const bool applied = commitSelection(useAll_ ? names_ : pendingNames_);
      job_ = Job::None;
      if (!applied) {
        loadSelection();
        showNotice("", tr(STR_WALL_SET_FAILED), tr(STR_DONE), wallpapersui::ActionDismiss);
        return;
      }
      pendingNames_.clear();
      selecting_ = false;
      pendingSet_.reset();
      pickView();
      interactionsReady_ = false;
      requestUpdate();
      return;
    }
  }
}
void WallpapersActivity::showGalleryPage(int next) {
  if (next == page_) return;
  RenderLock lock;
  page_ = next;
  interactionsReady_ = false;
  ensureThumbsForPage();
  requestUpdate();
}
void WallpapersActivity::beginSelection() {
  RenderLock lock;
  selecting_ = !selecting_;
  pendingSet_.reset();
  if (selecting_)
    for (size_t n = 0; n < names_.size() && n < pendingSet_.size(); ++n)
      if (usableSet_.test(n) && isChosen(int(n))) pendingSet_.set(n);
  ensureThumbsForPage();
  interactionsReady_ = false;
  requestUpdate();
}
void WallpapersActivity::toggleAllSelection() {
  RenderLock lock;
  if (pendingSet_ == usableSet_) {
    pendingSet_.reset();
  } else
    pendingSet_ = usableSet_;
  requestUpdate();
}
void WallpapersActivity::confirmSelection() {
  RenderLock lock;
  pendingSet_ &= usableSet_;
  view_ = View::ConfirmSet;
  interactionsReady_ = false;
  requestUpdate();
}
void WallpapersActivity::queueSelection() {
  RenderLock lock;
  pendingNames_.clear();
  if (!pendingSet_.any()) {
    if (!commitSelection(pendingNames_)) {
      showNotice("", tr(STR_WALL_SET_FAILED), tr(STR_DONE), wallpapersui::ActionDismiss);
      return;
    }
    selecting_ = false;
    pickView();
    interactionsReady_ = false;
    requestUpdate();
    return;
  }
  useAll_ = pendingSet_.count() == names_.size();
  // Only a partial set needs copied names: at most256, <=40KiB with128-byte names.
  // Retained activity storage reuses capacity; the ALL path borrows names_ directly.
  if (!useAll_) {
    pendingNames_.reserve(pendingSet_.count());
    for (size_t n = 0; n < names_.size() && n < pendingSet_.size(); ++n)
      if (pendingSet_.test(n)) pendingNames_.push_back(names_[n]);
  }
  prepareAt_ = 0;
  fetchTotal_ = int(pendingSet_.count());
  view_ = View::Preparing;
  job_ = Job::Apply;
  painted_ = false;
  interactionsReady_ = false;
  requestUpdate();
}
