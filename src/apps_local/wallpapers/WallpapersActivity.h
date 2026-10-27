#pragma once

// SD image gallery with paged thumbnails, full-screen preview and a hold
// menu. Setting a wallpaper reuses the transactional sleep-image store; gallery
// taps and slideshow never change that selection. Thumbnail rows are decoded
// once per page and reused while the activity is alive.

#include <bitset>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "WallpapersCore.h"
#include "WallpapersScreens.h"
#include "WallpapersSelection.h"

class WallpapersActivity final : public Activity {
 public:
  WallpapersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Wallpapers", renderer, mappedInput) {}
  ~WallpapersActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // A wallpaper scaled down to a cell: a 1-bpp bit buffer (set bit = ink) plus
  // where it sits inside the cell once its aspect is fitted.
  struct Thumb {
    int16_t w = 0;
    int16_t h = 0;
    int16_t ox = 0;
    int16_t oy = 0;
    std::vector<uint8_t> bits;  // packed, (w+7)/8 bytes per row
    bool ok = false;
  };

  enum class View : uint8_t { Grid, Notice, Sheet, Confirm, Preview, Empty, Preparing, ConfirmSet };
  View view_ = View::Grid;
  View contextReturn_ = View::Grid;

  enum class Job : uint8_t { None, Discover, Thumbnails, Page, Apply };
  Job job_ = Job::None;
  uint64_t libraryStamp_ = 0;
  int prepareAt_ = 0, prepareErrors_ = 0;
  int16_t prepareW_ = 0, prepareH_ = 0;
  unsigned animationFrame_ = 0;
  uint32_t progressAt_ = 0;
  bool selecting_ = false;
  std::bitset<256> pendingSet_;
  std::bitset<256> usableSet_;
  // Bounded cold-path subset for the existing selection writer (<=256 names).
  std::vector<std::string> pendingNames_;
  bool useAll_ = false;
  void initializeLibrary();
  void preparationStep();
  bool cachePrepared(bool write);
  void beginSelection();
  void toggleAllSelection();
  void confirmSelection();
  void queueSelection();
  void showGalleryPage(int next);
  void scanLibrary();
  void pickView();        // Grid or Offer, from the card alone
  void sweepPartFiles();  // drop incomplete copies left by a power cut
  void sweepPartFilesIn(const char* dirPath);
  void showNotice(const char* headline, const char* body, const char* actionLabel, freeink::ui::ActionId action);
  // What the CARD says is chosen, re-read after every commit. Never a
  // remembered intention: /sleep.bmp and /.sleep are what the sleep screen
  // reads, so they are what the picker reports (see docs/apps/wallpapers-shuffle.md).
  void loadSelection();
  bool isChosen(int index) const;
  void listShuffleDir(std::vector<std::string>& out) const;
  // Put the card into the one shape cardShapeFor() names for this many
  // wallpapers. The ONLY writer of /sleep.bmp, /.sleep and /wallpapers/.active,
  // so the invariant that they are never both live has one place to hold.
  bool commitSelection(const std::vector<std::string>& want);
  bool copyWallpaper(const std::string& src, const std::string& dst) const;
  std::string sourcePathFor(const std::string& name) const;
  bool fillShuffleDir(const std::vector<std::string>& want);
  void clearShuffleDir();
  void applySleepSettings();
  // Whether the pinned wallpaper can reach the sleep screen under the settings
  // as they are RIGHT NOW, and the one line that says so. Both read SETTINGS
  // live, because the two settings involved are changed elsewhere (Settings,
  // and the web settings page) while this app is not looking. See #354.
  wallpapers::Reach sleepReach() const;
  bool sleepBlocked() const;
  // Not const: a line carrying a count is built into note_, which has to
  // outlive the paint. Every other line is a literal out of WallpapersCore.
  const char* currentSleepNote();
  bool setWallpaper(int index);  // choose exactly this one
  void openSheet(int index, View destination = View::Sheet);
  bool deleteWallpaper();  // remove the sheet's wallpaper from the card
  void renderPreview();    // the wallpaper at 1:1, nothing else on the panel
  int pageCount() const;   // over the whole library
  void clampPage();
  void ensureThumbsForPage();  // decode this page's cells if not cached
  Thumb decodeThumb(const std::string& path, int16_t cellW, int16_t cellH) const;
  // Cached decode: reads /wallpapers/.thumbs/<name>.thb when it still matches
  // the source and the cell size, otherwise decodes and writes it.
  Thumb thumbFor(const std::string& name, const std::string& path, int16_t cellW, int16_t cellH, int* decoded = nullptr,
                 Thumb* reuse = nullptr);
  void drawThumbInto(const Thumb& t, const freeink::ui::Rect& box, bool crop = false) const;
  void drawGrid(const wallpapersui::GridGeom& geom);
  bool gridCacheMatches(const wallpapersui::GridGeom& geom) const;
  void drawMarker(const freeink::ui::Rect& th) const;

  // Which wallpaper the sheet, the confirm and the preview are about. Held as a
  // NAME as well as an index because the index is a position in a list that
  // deleting, uploading and page-turning all renumber, and a stale index would
  // delete the wrong picture. The name is re-resolved to an index at the moment
  // of the delete and the delete refuses if it no longer resolves.
  unsigned long slideshowAt_ = 0;
  char sheetMeta_[64]{};
  int sheetIndex_ = -1;
  std::string sheetFile_;    // the file name, the identity that survives a re-sort
  std::string sheetName_;    // its display name, for the two screens' headline
  std::string sheetDetail_;  // the confirm's consequence sentence(s)
  // Settled by openSheet on the LOOP task. render() runs on the other task with
  // no lock between them, so it reads this rather than indexing names_, which
  // deleteWallpaper clears and reallocates underneath it.
  bool sheetIsActive_ = false;

  struct ExternalSource {
    std::string alias, path;
  };
  std::vector<ExternalSource> externalSources_;
  wallpapers::DisplayName displayNameFor(const std::string&) const;
  std::string originalPathFor(const std::string&) const;
  std::vector<std::string> names_;  // library file names, sorted
  // The chosen set, as it is on the card. Holds the pinned name when one
  // wallpaper is chosen and the contents of /.sleep when several are -- INCLUDING
  // any whose library file has since been deleted, because those copies still
  // take their turn on the glass and a count that skipped them would understate
  // what the sleep screen does.
  std::vector<std::string> chosen_;
  int activeIndex_ = -1;  // which name is pinned, or -1
  bool painted_ = false;  // the panel has shown something at least once
  int page_ = 0;
  int fetchTotal_ = 0;
  std::string noticeHead_;
  std::string noticeBody_;
  const char* noticeAction_ = nullptr;
  freeink::ui::ActionId noticeActionId_ = 0;
  bool preventAutoSleep() override { return job_ != Job::None; }
  // The last selection's outcome, kept so the strip can report what it changed
  // behind the user's back. The CHOICE, not a rendered sentence: the sentence
  // also has to carry any standing caveat, and a caveat is only knowable from
  // SETTINGS at paint time. Holding a string here is what let the first version
  // suppress a caveat for a whole app session (#354, twice over).
  bool selectedThisSession_ = false;
  wallpapers::SleepChoice lastChoice_;
  // /sleep.bmp present while /.sleep also holds files: one picture is showing
  // and a set is inert behind it. Read off the card on entry, because nothing
  // in SETTINGS can see it and this app is not its only cause.
  bool shadowedSet_ = false;
  std::string note_;  // the hint strip's line, when it carries a number

  // The current page's thumbnails, one per on-page slot (perPage entries;
  // trailing empty slots have ok = false).
  std::vector<Thumb> thumbs_;
  int cachedPage_ = -1;
  int cachedPerPage_ = -1;
  int cachedCellW_ = -1, cachedCellH_ = -1;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;

  // The grid is hit-tested against geometry and never reaches route(), so its
  // taps are gated on the page and the pinned wallpaper -- state loop() settles
  // before render() runs. See Activity::surfaceMeaning().
  uint32_t surfaceMeaning() const override;
};
