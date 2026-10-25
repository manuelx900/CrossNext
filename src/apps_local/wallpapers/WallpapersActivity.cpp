#include "WallpapersActivity.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <I18n.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>
#include <Memory.h>
#include <PngToBmpConverter.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../../lib/GfxRenderer/FontCacheManager.h"
#include "../../CrossPointSettings.h"
#include "../Shelf.h"
#include "../ui/ExtraIcons.h"
#include "../ui/PageIndicator.h"
#include "../ui/PixelMenu.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxText.h"
#include "../ui/ToyboxTheme.h"
#include "Bitmap.h"
#include "WallpapersCore.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;

namespace {
// WallpapersCore mirrors CrossPointSettings::SLEEP_SCREEN_MODE so the sleep-
// reachability rules are freestanding and host-testable. This is the seam where
// the mirror is checked: a value that moves upstream fails the build here
// rather than silently teaching the picker to say the wrong sentence.
static_assert(wallpapers::kSleepDark == CrossPointSettings::SLEEP_SCREEN_MODE::DARK, "sleep mode mirror drifted");
static_assert(wallpapers::kSleepLight == CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT, "sleep mode mirror drifted");
static_assert(wallpapers::kSleepCustom == CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM, "sleep mode mirror drifted");
static_assert(wallpapers::kSleepCover == CrossPointSettings::SLEEP_SCREEN_MODE::COVER, "sleep mode mirror drifted");
static_assert(wallpapers::kSleepCoverCustom == CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM,
              "sleep mode mirror drifted");
static_assert(wallpapers::kSleepBlank == CrossPointSettings::SLEEP_SCREEN_MODE::BLANK, "sleep mode mirror drifted");
static_assert(wallpapers::kSleepQuickResume == CrossPointSettings::SLEEP_SCREEN_MODE::QUICK_RESUME,
              "sleep mode mirror drifted");
static_assert(wallpapers::kSleepTransparentCustom == CrossPointSettings::SLEEP_SCREEN_MODE::TRANSPARENT_CUSTOM,
              "sleep mode mirror drifted");
static_assert(wallpapers::kSleepNote == CrossPointSettings::SLEEP_SCREEN_MODE::NOTE, "sleep mode mirror drifted");
static_assert(wallpapers::kSleepModeCount == CrossPointSettings::SLEEP_SCREEN_MODE::SLEEP_SCREEN_MODE_COUNT,
              "a sleep screen mode was added upstream; WallpapersCore's mirror and its rules must be updated");

constexpr int kMaxLibrary = 256;
constexpr size_t kNameMax = 128;
constexpr size_t kCopyChunk = 4096;

// ---------------------------------------------------------------------------
// The thumbnail cache.
//
// A thumbnail is a full 480x800 1-bit BMP read off the card and box-downscaled
// (the renderer's 1-bit blit cannot scale). cachedPage_ resets on every
// onEnter, so before this the app re-did that work on EVERY open and on every
// page turn -- harmless on an empty card, and the whole cost once 21 wallpapers
// are on it.
//
// Cached per wallpaper, not per set, so one changed or deleted file invalidates
// its own thumb and nothing else. The key is the source's SIZE plus a sample of
// its bytes plus the cell size: every device wallpaper is exactly 48062 bytes,
// so size alone cannot see a replaced file, and a cell-size change (the hint
// gap moved it) must invalidate everything. The sample is three 64-byte reads
// rather than a full hash, because hashing the whole file would cost what the
// decode costs and save nothing.
constexpr char kThumbDir[] = "/wallpapers/.thumbs";
constexpr uint32_t kThumbMagic = 0x33485457;  // "WTH3": complete source decode

struct ThumbHeader {
  uint32_t magic;
  uint32_t sourceBytes;
  uint32_t sampleHash;
  uint32_t modified;
  int16_t cellW, cellH, w, h, ox, oy;
};

uint32_t sampleHashOf(HalFile& f, const uint64_t size) {
  uint32_t h = 2166136261u;
  const uint64_t offsets[3] = {62, size / 2, size > 128 ? size - 128 : 0};
  uint8_t buf[64];
  for (const uint64_t off : offsets) {
    if (!f.seekSet(static_cast<size_t>(off))) continue;
    const int got = f.read(buf, sizeof(buf));
    for (int i = 0; i < got; ++i) {
      h ^= buf[i];
      h *= 16777619u;
    }
  }
  return h;
}

// The selection marker's dimensions live in WallpapersScreens.cpp beside
// kMarkerRoom, the clearance they have to fit inside.

// A page dot: a small square, filled for the current page.
constexpr int16_t kDotSize = 12;
constexpr int16_t kDotGap = 10;

// Ordered 8x8 Bayer thresholds. A thumbnail is an AREA AVERAGE of the source
// re-dithered at thumbnail size: a 50% threshold collapses a dense engraving
// into a flat blob ("grey mush"), while re-dithering keeps its tone as texture,
// which is the difference between a picker that looks designed and one that
// looks broken. Ordered rather than error diffusion, the house rule: error
// diffusion is what makes flat fields look dirty.
constexpr uint8_t kBayer8[64] = {
    0,  48, 12, 60, 3,  51, 15, 63, 32, 16, 44, 28, 35, 19, 47, 31, 8,  56, 4,  52, 11, 59,
    7,  55, 40, 24, 36, 20, 43, 27, 39, 23, 2,  50, 14, 62, 1,  49, 13, 61, 34, 18, 46, 30,
    33, 17, 45, 29, 10, 58, 6,  54, 9,  57, 5,  53, 42, 26, 38, 22, 41, 25, 37, 21,
};

// Extract a 2-bpp pixel (0=black .. 3=white) from readNextRow's packed output.
inline uint8_t px2(const uint8_t* data, int x) {
  return static_cast<uint8_t>((data[x >> 2] >> (6 - ((x & 3) * 2))) & 0x3);
}
}  // namespace

std::unique_ptr<Activity> WallpapersActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<WallpapersActivity>(renderer, mappedInput);
}

void WallpapersActivity::initializeLibrary() {
  // Timed per phase because "the app takes ten seconds to open" is not a
  // diagnosis, and this fork's recurring failure is fixing the thing in front
  // of you rather than the thing that is slow. One line names the cost.
  const uint32_t tEnter = millis();

  const uint32_t tFonts = millis();
  Storage.mkdir(wallpapers::kLibraryDir);
  sweepPartFiles();
  const uint32_t tSweep = millis();
  scanLibrary();
  const uint32_t tScan = millis();
  loadSelection();
  const uint32_t tActive = millis();
  selectedThisSession_ = false;
  slideshowAt_ = 0;
  LOG_INF("WALL", "onEnter %ums: fonts=%u sweep=%u scan=%u active=%u", tActive - tEnter, tFonts - tEnter,
          tSweep - tFonts, tScan - tSweep, tActive - tScan);
  // Open on the page holding the set wallpaper, so the border is on screen.
  cachedPage_ = -1;
  page_ = 0;
  // The local card determines whether the gallery or empty state opens.
}

void WallpapersActivity::scanLibrary() {
  const uint64_t previousStamp = libraryStamp_;
  const auto previousUsable = usableSet_;
  // Library order can change after uploads/downloads; pending bits name the old order.
  selecting_ = false;
  pendingSet_.reset();
  usableSet_.reset();
  names_.clear();
  names_.reserve(kMaxLibrary);
  libraryStamp_ = 1469598103934665603ull;
  externalSources_.clear();
  externalSources_.reserve(kMaxLibrary);
  auto name = makeUniqueNoThrow<char[]>(kNameMax);
  if (!name) {
    LOG_ERR("WALL", "OOM: wallpaper name buffer");
    return;
  }
  for (const char* folder : {"/sleep", "/WALLPAPER", wallpapers::kLibraryDir}) {
    auto source = Storage.open(folder);
    if (!source || !source.isDirectory()) continue;
    for (auto entry = source.openNextFile(); entry; entry = source.openNextFile()) {
      if (entry.isDirectory() || names_.size() >= kMaxLibrary) continue;
      entry.getName(name.get(), kNameMax);
      if (name[0] == '.' || !FsHelpers::hasImageExtension(std::string_view(name.get()))) continue;
      if (!FsHelpers::hasBmpExtension(std::string_view(name.get())) &&
          !FsHelpers::hasPngExtension(std::string_view(name.get())) &&
          !FsHelpers::hasJpgExtension(std::string_view(name.get())))
        continue;
      uint64_t stamp = 1469598103934665603ull;
      for (const char* text : {folder, static_cast<const char*>(name.get())})
        for (const unsigned char* c = reinterpret_cast<const unsigned char*>(text); *c; ++c)
          stamp = (stamp ^ *c) * 1099511628211ull;
      libraryStamp_ += stamp ^ (uint64_t(entry.modificationTime()) << 32) ^ entry.fileSize64();
      if (!std::strcmp(folder, wallpapers::kLibraryDir) && FsHelpers::hasBmpExtension(std::string_view(name.get()))) {
        names_.emplace_back(name.get());
        continue;
      }
      std::string original = std::string(folder) + "/" + name.get();
      uint64_t hash = 1469598103934665603ull;
      for (unsigned char c : original) hash = (hash ^ c) * 1099511628211ull;
      hash ^= uint64_t(entry.modificationTime()) << 32;
      hash ^= entry.fileSize64();
      char alias[40];
      std::snprintf(alias, sizeof(alias), "external-%016llx.bmp", static_cast<unsigned long long>(hash));
      names_.emplace_back(alias);
      externalSources_.push_back({alias, std::move(original)});
    }
  }
  std::sort(externalSources_.begin(), externalSources_.end(),
            [](const ExternalSource& a, const ExternalSource& b) { return a.alias < b.alias; });
  // Every gallery entry is a file on the card, including phone uploads.
  const auto sortName = [this](const std::string& alias) -> std::string_view {
    const auto source =
        std::lower_bound(externalSources_.begin(), externalSources_.end(), alias,
                         [](const ExternalSource& item, const std::string& key) { return item.alias < key; });
    if (source != externalSources_.end() && source->alias == alias) {
      const std::string_view path = source->path;
      return path.substr(path.rfind('/') + 1);
    }
    return alias;
  };
  std::sort(names_.begin(), names_.end(), [&](const std::string& a, const std::string& b) {
    return wallpapers::sortsBefore(sortName(a), sortName(b));
  });

  if (libraryStamp_ == previousStamp) usableSet_ = previousUsable;
}

void WallpapersActivity::listShuffleDir(std::vector<std::string>& out) const {
  out.clear();
  auto dir = Storage.open(wallpapers::kShuffleDir);
  if (!dir || !dir.isDirectory()) return;
  out.reserve(kMaxLibrary);
  auto name = makeUniqueNoThrow<char[]>(kNameMax);
  if (!name) {
    LOG_ERR("WALL", "OOM: shuffle name buffer");
    return;
  }
  for (auto entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    if (entry.isDirectory()) continue;
    entry.getName(name.get(), kNameMax);
    if (!wallpapers::isSupportedWallpaper(std::string_view{name.get()})) continue;
    out.emplace_back(name.get());
    if (static_cast<int>(out.size()) >= kMaxLibrary) break;
  }
}

// What the CARD says is chosen. Not a remembered intention and not a hint file:
// /sleep.bmp and /.sleep are what SleepActivity reads, so they are what the
// picker reports. See docs/apps/wallpapers-shuffle.md.
void WallpapersActivity::loadSelection() {
  activeIndex_ = -1;
  chosen_.clear();
  shadowedSet_ = false;

  // Default/blank sleep screens do not use the retained custom image copies.
  if (SETTINGS.sleepScreen == CrossPointSettings::DARK || SETTINGS.sleepScreen == CrossPointSettings::LIGHT ||
      SETTINGS.sleepScreen == CrossPointSettings::BLANK)
    return;

  // The pin is checked FIRST because renderCustomSleepScreen checks it first:
  // while it is there it is the only thing that shows, whatever else is on the
  // card. Deliberately NOT gated on sleepScreen == CUSTOM (#354) -- what is
  // pinned is a fact about the card, and whether it reaches the glass is a fact
  // about two settings that the hint strip says in words.
  if (Storage.exists(wallpapers::kPinnedSleep)) {
    std::vector<std::string> shadowed;
    listShuffleDir(shadowed);
    // A set behind a pin. This app is not the only way to get here:
    // BmpViewerActivity's "set sleep cover" writes /sleep.bmp straight from the
    // file browser, the File Transfer page can drop one at the root, and a
    // power cut during a one-to-many transition leaves one behind. Reported
    // rather than silently repaired: those are the user's files, and the strip
    // has a sentence for exactly this.
    shadowedSet_ = !shadowed.empty();
    char marker[kNameMax] = {};
    if (Storage.readFileToBuffer(wallpapers::kActiveMarker, marker, sizeof(marker)) == 0) return;
    for (char* p = marker; *p; ++p) {
      if (*p == '\n' || *p == '\r') {
        *p = '\0';
        break;
      }
    }
    for (int i = 0; i < static_cast<int>(names_.size()); ++i) {
      if (wallpapers::sameFileName(names_[static_cast<size_t>(i)], marker)) {
        activeIndex_ = i;
        chosen_.push_back(names_[static_cast<size_t>(i)]);
        break;
      }
    }
    return;
  }

  // No pin: the set IS the directory. Every file counts, including one whose
  // library original has since been deleted -- that copy still takes its turn
  // on the glass, and a count that skipped it would understate what the sleep
  // screen does. Files the user put here themselves are adopted for the same
  // reason: they are what the sleep screen shows.
  listShuffleDir(chosen_);
  if (chosen_.size() == 1) {
    for (int i = 0; i < static_cast<int>(names_.size()); ++i) {
      if (wallpapers::sameFileName(names_[static_cast<size_t>(i)], chosen_[0])) {
        activeIndex_ = i;
        break;
      }
    }
  }
}

bool WallpapersActivity::isChosen(const int index) const {
  if (index < 0 || index >= static_cast<int>(names_.size())) return false;
  const std::string& name = names_[static_cast<size_t>(index)];
  for (const std::string& c : chosen_) {
    if (wallpapers::sameFileName(c, name)) return true;
  }
  return false;
}

wallpapers::Reach WallpapersActivity::sleepReach() const {
  const bool quickResumeOnTimeout =
      SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT;
  return wallpapers::reachOfPinnedSleep(SETTINGS.sleepScreen, quickResumeOnTimeout);
}

bool WallpapersActivity::sleepBlocked() const {
  const wallpapers::Reach reach = sleepReach();
  return reach == wallpapers::Reach::BlockedByMode || reach == wallpapers::Reach::BlockedByQuickResume;
}

const char* WallpapersActivity::currentSleepNote() {
  const wallpapers::Reach reach = sleepReach();
  const int chosen = static_cast<int>(chosen_.size());

  // What a selection changed BEHIND the user's back outranks everything below:
  // it is the only voice that fact has, and stripLineAfterSelection already
  // carries any standing caveat with it so nothing is lost by letting it win
  // (#354 was a caveat suppressed by a line that had no room for both).
  if (selectedThisSession_ && (lastChoice_.tookOverMode || lastChoice_.clearedQuickResume)) {
    return wallpapers::stripLineAfterSelection(lastChoice_, reach).text;
  }

  // Choosing, a real set, or a set hidden behind a pin: the set line owns the
  // strip, and it already orders the caveat and the contradiction ahead of any
  // count it might otherwise print.
  if (chosen >= 2 || shadowedSet_) {
    const wallpapers::ShuffleLine line = wallpapers::shuffleStripLine(false, chosen, shadowedSet_, reach);
    if (line.text == nullptr) return nullptr;
    if (!line.wantsCount) return line.text;
    // The only line that allocates, and it is a member so it outlives the
    // paint. host-tests/wallcaption measures the sentence with the widest count
    // this app can produce in front of it.
    note_ = std::to_string(chosen);
    note_ += ' ';
    note_ += line.text;
    return note_.c_str();
  }

  // One wallpaper, or none. Nothing pinned: the strip already carries "Tap one
  // to set your sleep screen.", and telling someone their non-existent
  // wallpaper is blocked would be noise.
  if (activeIndex_ < 0) return nullptr;
  if (selectedThisSession_) return wallpapers::stripLineAfterSelection(lastChoice_, reach).text;
  // "Sleep Screen is not set to Custom." is true and says nothing about why;
  // the strip's own note line does.
  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::NOTE) return nullptr;
  return wallpapers::reachHint(reach);
}

int WallpapersActivity::pageCount() const {
  const int per = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext(), selecting_).perPage;
  if (names_.empty() || per <= 0) return 1;
  // 0, not a literal 1. drawGrid and the hit-test both use it, and
  // it is 2 whenever any built-in is missing -- so a literal 1 here promised
  // fewer pages than the grid draws and the LAST wallpaper became unreachable
  // at every library size where (1 + N) % perPage == 0. Pre-existing, and this
  // app now has two brand-new routes into that state: deleting a built-in, and
  // a set the user pages through to build.
  //
  // The arithmetic is freestanding so the three readers of "how many tiles are
  // there" can be walked against each other rather than compared by eye.
  return wallpapersui::pageCountFor(0, static_cast<int>(names_.size()), per);
}

void WallpapersActivity::clampPage() {
  const int pages = pageCount();
  if (page_ < 0) page_ = 0;
  if (page_ >= pages) page_ = pages - 1;
}

// Copy one wallpaper onto the card, through a .part name renamed only when the
// whole file is written.
//
// Writing straight to the destination truncates whatever was there on the first
// byte, so a card that fills (or a power cut) halfway leaves a SHORT file where
// a good one used to be. That is not a file the sleep screen skips:
// Bitmap::parseHeaders seeks to bfOffBits and never checks that the pixel data
// is complete, and SleepActivity::findNextValidSleepImage accepts a file on
// exactly that check -- so a truncated 480x800 image is a VALID sleep image and
// gets drawn, half-rendered, on every sleep, with nothing on screen to say why.
// The rename is the only thing between a failed copy and a permanently broken
// sleep screen.
bool WallpapersActivity::copyWallpaper(const std::string& src, const std::string& dst) const {
  const std::string part = dst + ".part";
  Storage.remove(part.c_str());

  HalFile in;
  if (!Storage.openFileForRead("WALL", src, in)) {
    LOG_ERR("WALL", "Cannot open wallpaper %s", src.c_str());
    return false;
  }
  HalFile out;
  if (!Storage.openFileForWrite("WALL", part, out)) {
    LOG_ERR("WALL", "Cannot open %s for write", part.c_str());
    return false;
  }
  auto buffer = makeUniqueNoThrow<uint8_t[]>(kCopyChunk);
  if (!buffer) {
    LOG_ERR("WALL", "OOM: copy buffer");
    return false;
  }
  const uint64_t expectedBytes = in.fileSize64();
  uint64_t copied = 0;
  for (;;) {
    const int got = in.read(buffer.get(), kCopyChunk);
    if (got < 0) {
      LOG_ERR("WALL", "Read error copying wallpaper");
      out.close();
      in.close();
      Storage.remove(part.c_str());
      return false;
    }
    if (got == 0) break;
    if (out.write(buffer.get(), static_cast<size_t>(got)) != static_cast<size_t>(got)) {
      LOG_ERR("WALL", "Write error copying wallpaper (card full?)");
      out.close();
      in.close();
      Storage.remove(part.c_str());
      return false;
    }
    copied += static_cast<uint64_t>(got);
  }
  out.close();
  in.close();

  // The size is knowable and exact, so check it rather than trusting that the
  // writes returned what they claimed.
  if (copied == 0 || copied != expectedBytes) {
    LOG_ERR("WALL", "Copied wallpaper is %u bytes, expected %u -- not swapping it in", static_cast<unsigned>(copied),
            static_cast<unsigned>(expectedBytes));
    Storage.remove(part.c_str());
    return false;
  }

  Storage.remove(dst.c_str());
  if (!Storage.rename(part.c_str(), dst.c_str())) {
    LOG_ERR("WALL", "Card refused the final rename of %s", dst.c_str());
    Storage.remove(part.c_str());
    return false;
  }
  return true;
}

// Where a chosen name can be read from. The library first; a member whose
// library original was deleted is still on the card as its own copy, and that
// copy is what keeps it in the rotation.
wallpapers::DisplayName WallpapersActivity::displayNameFor(const std::string& name) const {
  for (const auto& item : externalSources_)
    if (item.alias == name) {
      const auto slash = item.path.rfind('/');
      return wallpapers::displayName(std::string_view(item.path).substr(slash + 1));
    }
  return wallpapers::displayName(name);
}
std::string WallpapersActivity::originalPathFor(const std::string& name) const {
  for (const auto& item : externalSources_)
    if (item.alias == name) return item.path;
  return std::string(wallpapers::kLibraryDir) + "/" + name;
}
std::string WallpapersActivity::sourcePathFor(const std::string& name) const {
  for (const auto& item : externalSources_)
    if (item.alias == name) {
      if (FsHelpers::hasBmpExtension(item.path)) return item.path;
      const std::string cache = std::string("/.crosspoint/wallpaper-source/") + name;
      if (Storage.exists(cache.c_str())) return cache;
      if (!Storage.ensureDirectoryExists("/.crosspoint/wallpaper-source")) return {};
      const std::string part = cache + ".part";
      HalFile input, output;
      if (!Storage.openFileForRead("WALL", item.path, input) || !Storage.openFileForWrite("WALL", part, output))
        return {};
      const bool converted = FsHelpers::hasPngExtension(item.path)
                                 ? PngToBmpConverter::pngFileTo1BitBmpStreamWithSize(
                                       input, output, std::min(renderer.getScreenWidth(), renderer.getScreenHeight()),
                                       std::max(renderer.getScreenWidth(), renderer.getScreenHeight()))
                                 : JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(
                                       input, output, std::min(renderer.getScreenWidth(), renderer.getScreenHeight()),
                                       std::max(renderer.getScreenWidth(), renderer.getScreenHeight()));
      output.flush();
      output.close();
      input.close();
      if (!converted || !Storage.replaceFile(part.c_str(), cache.c_str())) {
        Storage.remove(part.c_str());
        LOG_ERR("WALL", "Could not convert %s", item.path.c_str());
        return {};
      }
      return cache;
    }
  const std::string inLibrary = std::string(wallpapers::kLibraryDir) + "/" + name;
  if (Storage.exists(inLibrary.c_str())) return inLibrary;
  return std::string(wallpapers::kShuffleDir) + "/" + name;
}

void WallpapersActivity::clearShuffleDir() {
  std::vector<std::string> have;
  listShuffleDir(have);
  for (const std::string& n : have) {
    const std::string path = std::string(wallpapers::kShuffleDir) + "/" + n;
    Storage.remove(path.c_str());
    LOG_INF("WALL", "Removed from the set: %s", n.c_str());
  }
}

// Make /.sleep hold exactly `want`. ADDS first, so a copy that fails leaves the
// set exactly as it was rather than as a shorter one nobody chose.
bool WallpapersActivity::fillShuffleDir(const std::vector<std::string>& want) {
  if (want.size() > kMaxLibrary) {
    LOG_ERR("WALL", "Wallpaper set exceeds %d images", kMaxLibrary);
    return false;
  }
  Storage.mkdir(wallpapers::kShuffleDir);
  std::vector<std::string> have;
  listShuffleDir(have);
  const auto holds = [](const std::vector<std::string>& list, const std::string& n) {
    for (const std::string& s : list) {
      if (wallpapers::sameFileName(s, n)) return true;
    }
    return false;
  };
  std::bitset<256> added;
  for (size_t index = 0; index < want.size(); ++index) {
    const auto& n = want[index];
    if (!holds(have, n)) {
      if (!copyWallpaper(sourcePathFor(n), std::string(wallpapers::kShuffleDir) + "/" + n)) {
        for (size_t undo = 0; undo < index; ++undo)
          if (added.test(undo)) Storage.remove((std::string(wallpapers::kShuffleDir) + "/" + want[undo]).c_str());
        return false;
      }
      added.set(index);
    }
    if (job_ == Job::Apply) {
      prepareAt_ = int(index) + 1;
      ++animationFrame_;
      if (prepareAt_ == fetchTotal_ || uint32_t(millis() - progressAt_) >= 200) {
        requestUpdateAndWait();
        progressAt_ = millis();
      }
    }
  }
  for (const std::string& n : have) {
    if (holds(want, n)) continue;
    Storage.remove((std::string(wallpapers::kShuffleDir) + "/" + n).c_str());
  }
  return true;
}

// The two settings that decide whether the sleep system ever draws what this
// app wrote (#354). Both slots -- the pin and the set -- are reached through
// renderCustomSleepScreen, so one rule covers them.
//
// Keeping the timeout quick-resume flag ON used to be this app's way of not
// making wake slower, and the price was the whole feature: while that flag is
// on, the IDLE sleep -- the ordinary one -- short-circuits above the
// sleep-screen switch and no wallpaper can appear. The user tapped a picture;
// the picture wins, and the picker says what that cost.
//
// The rule and its consequences are proved in host-tests/wallpapers, which is
// where they can be walked over all eight modes rather than reasoned about.
void WallpapersActivity::applySleepSettings() {
  const bool quickResumeOnTimeout =
      SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT;
  const wallpapers::SleepChoice choice = wallpapers::choiceForSetWallpaper(SETTINGS.sleepScreen, quickResumeOnTimeout);
  // Written only when it actually moves. The set is committed one tap at a
  // time, and an unconditional SETTINGS.saveToFile() would put a JSON write on
  // every one of them for a value that has not changed since the first.
  if (SETTINGS.sleepScreen != choice.sleepScreenMode || quickResumeOnTimeout != choice.quickResumeAfterTimeout) {
    SETTINGS.sleepScreen = choice.sleepScreenMode;
    SETTINGS.quickResumeSleepScreen = choice.quickResumeAfterTimeout
                                          ? CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT
                                          : CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_NEVER;
    SETTINGS.saveToFile();
    LOG_INF("WALL", "sleep settings changed by a selection: mode %s -> %s, quick resume on timeout %d -> %d",
            wallpapers::sleepScreenModeName(choice.previousMode),
            wallpapers::sleepScreenModeName(choice.sleepScreenMode), quickResumeOnTimeout ? 1 : 0,
            choice.quickResumeAfterTimeout ? 1 : 0);
  }
  lastChoice_ = choice;
  selectedThisSession_ = true;
}

// Put the card into the ONE shape cardShapeFor() names for this many
// wallpapers. The only writer of /sleep.bmp, /.sleep and /wallpapers/.active,
// so "the pin and the set are never both live" has one place to hold.
//
// The order inside each branch is chosen so that a power cut mid-commit leaves
// a picture the user CHOSE on the glass rather than nothing: the slot that wins
// is written before the slot that loses is cleared. That can leave both
// populated for an instant, which is the same state BmpViewerActivity can
// create at any time, and loadSelection() reports it in words either way.
bool WallpapersActivity::commitSelection(const std::vector<std::string>& want) {
  const wallpapers::CardShape shape = wallpapers::cardShapeFor(static_cast<int>(want.size()));

  if (shape.pinned) {
    if (!copyWallpaper(sourcePathFor(want[0]), wallpapers::kPinnedSleep)) return false;
    clearShuffleDir();
    HalFile marker;
    if (Storage.openFileForWrite("WALL", wallpapers::kActiveMarker, marker)) {
      marker.write(reinterpret_cast<const uint8_t*>(want[0].c_str()), want[0].size());
      marker.close();
    }
  } else if (shape.shuffled) {
    if (!fillShuffleDir(want)) return false;
    Storage.remove(wallpapers::kPinnedSleep);
    // The hint has nothing left to name, and a stale name beside a set is the
    // second source that can disagree with the directory. Deleting it is
    // cheaper than reconciling it.
    Storage.remove(wallpapers::kActiveMarker);
  } else {
    const auto previousMode = SETTINGS.sleepScreen;
    const auto previousQuickResume = SETTINGS.quickResumeSleepScreen;
    if (previousMode != CrossPointSettings::DARK || previousQuickResume != CrossPointSettings::QUICK_RESUME_NEVER) {
      SETTINGS.sleepScreen = CrossPointSettings::DARK;
      SETTINGS.quickResumeSleepScreen = CrossPointSettings::QUICK_RESUME_NEVER;
      if (!SETTINGS.saveToFile()) {
        SETTINGS.sleepScreen = previousMode;
        SETTINGS.quickResumeSleepScreen = previousQuickResume;
        LOG_ERR("WALL", "Could not save default sleep screen");
        return false;
      }
    }
    selectedThisSession_ = false;
  }

  // Disabling custom sleep retains all originals and sleep-image copies.
  if (!want.empty()) applySleepSettings();
  loadSelection();
  LOG_INF("WALL", "selection committed: %d chosen (pin=%d set=%d)", static_cast<int>(want.size()), shape.pinned ? 1 : 0,
          shape.files);
  return true;
}

bool WallpapersActivity::setWallpaper(const int index) {
  if (index < 0 || index >= static_cast<int>(names_.size()) || !usableSet_.test(size_t(index))) return false;
  return commitSelection({names_[static_cast<size_t>(index)]});
}

WallpapersActivity::Thumb WallpapersActivity::decodeThumb(const std::string& path, int16_t cellW, int16_t cellH) const {
  Thumb t;
  HalFile file;
  if (!Storage.openFileForRead("WALL", path, file)) return t;
  Bitmap bmp(file);
  if (bmp.parseHeaders() != BmpReaderError::Ok) return t;

  const int sw = bmp.getWidth();
  const int sh = bmp.getHeight();
  if (sw <= 0 || sh <= 0) return t;
  const bool topDown = bmp.isTopDown();

  // Fit the wallpaper's aspect inside the cell (contain), centred.
  const float scale = std::min(static_cast<float>(cellW) / sw, static_cast<float>(cellH) / sh);
  int dw = std::max(1, static_cast<int>(sw * scale));
  int dh = std::max(1, static_cast<int>(sh * scale));
  if (dw > cellW) dw = cellW;
  if (dh > cellH) dh = cellH;
  t.w = static_cast<int16_t>(dw);
  t.h = static_cast<int16_t>(dh);
  t.ox = static_cast<int16_t>((cellW - dw) / 2);
  t.oy = static_cast<int16_t>((cellH - dh) / 2);

  const int bytesPerRow = (dw + 7) / 8;
  t.bits.assign(static_cast<size_t>(bytesPerRow) * dh, 0);

  auto data = makeUniqueNoThrow<uint8_t[]>((sw + 3) / 4);
  auto rowBuffer = makeUniqueNoThrow<uint8_t[]>(bmp.getRowBytes());
  std::vector<uint16_t> acc(static_cast<size_t>(dw), 0);
  std::vector<uint16_t> cnt(static_cast<size_t>(dw), 0);
  if (!data || !rowBuffer) return t;

  auto flush = [&](int dy) {
    if (dy < 0 || dy >= dh) return;
    uint8_t* outRow = t.bits.data() + static_cast<size_t>(bytesPerRow) * dy;
    for (int dx = 0; dx < dw; ++dx) {
      if (cnt[dx] == 0) continue;
      // How much of the source box was ink, 0..255, then dithered against the
      // ordered matrix so a half-dark box becomes texture rather than a blob.
      const int ink = static_cast<int>(acc[dx]) * 255 / static_cast<int>(cnt[dx]);
      const int threshold = (static_cast<int>(kBayer8[(dy & 7) * 8 + (dx & 7)]) * 255) / 64;
      if (ink > threshold) outRow[dx >> 3] |= static_cast<uint8_t>(0x80 >> (dx & 7));
    }
    std::fill(acc.begin(), acc.end(), 0);
    std::fill(cnt.begin(), cnt.end(), 0);
  };

  int curDy = -1;
  for (int sy = 0; sy < sh; ++sy) {
    if (bmp.readNextRow(data.get(), rowBuffer.get()) != BmpReaderError::Ok) return Thumb{};
    const int srcRow = topDown ? sy : (sh - 1 - sy);
    int dy = srcRow * dh / sh;
    if (dy >= dh) dy = dh - 1;
    if (dy != curDy) {
      if (curDy >= 0) flush(curDy);
      curDy = dy;
    }
    for (int sx = 0; sx < sw; ++sx) {
      const int dx = sx * dw / sw;
      if (dx >= dw) continue;
      // px2: 0=black .. 3=white; treat the darker half as ink.
      if (px2(data.get(), sx) <= 1) acc[static_cast<size_t>(dx)]++;
      cnt[static_cast<size_t>(dx)]++;
    }
  }
  if (curDy >= 0) flush(curDy);
  t.ok = true;
  return t;
}

void WallpapersActivity::ensureThumbsForPage() {
  const wallpapersui::GridGeom geom = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext(), selecting_);
  if (cachedPage_ == page_ && cachedPerPage_ == geom.perPage && cachedCellW_ == geom.cellW &&
      cachedCellH_ == geom.cellH && !thumbs_.empty())
    return;

  const uint32_t tThumbs = millis();
  int decoded = 0;
  thumbs_.resize(static_cast<size_t>(geom.perPage));
  for (auto& thumb : thumbs_) thumb.ok = false;
  const int base = page_ * geom.perPage;
  // 0, not a literal 1: the decode and the draw must agree about
  // which wallpaper is in a cell, and when they did not, one tile drew its
  // neighbour's picture and the next drew a decode-failure cross.
  const int specials = 0;
  for (int slot = 0; slot < geom.perPage; ++slot) {
    const int combined = base + slot;
    if (combined < specials) continue;  // the chrome tiles have no thumbnail
    const int idx = combined - specials;
    if (idx >= static_cast<int>(names_.size())) break;
    std::string path = sourcePathFor(names_[static_cast<size_t>(idx)]);
    thumbs_[static_cast<size_t>(slot)] = thumbFor(names_[static_cast<size_t>(idx)], path, geom.cellW, geom.cellH,
                                                  &decoded, &thumbs_[static_cast<size_t>(slot)]);
    usableSet_.set(size_t(idx), thumbs_[static_cast<size_t>(slot)].ok);
    if (!usableSet_.test(size_t(idx))) pendingSet_.reset(size_t(idx));
  }
  LOG_INF("WALL", "thumbs page=%d decoded=%d (rest from cache) in %ums", page_, decoded, millis() - tThumbs);
  cachedPage_ = page_;
  cachedPerPage_ = geom.perPage;
  cachedCellW_ = geom.cellW;
  cachedCellH_ = geom.cellH;
}

// Cache hit or a decode plus a write. `decoded` counts only the real decodes so
// the log says which of the two happened.
WallpapersActivity::Thumb WallpapersActivity::thumbFor(const std::string& name, const std::string& path,
                                                       const int16_t cellW, const int16_t cellH, int* decoded,
                                                       Thumb* reuse) {
  uint64_t sourceBytes = 0;
  uint32_t sample = 0;
  uint32_t modified = 0;
  {
    HalFile src;
    if (Storage.openFileForRead("WALL", path, src)) {
      sourceBytes = static_cast<uint64_t>(src.size());
      modified = src.modificationTime();
      sample = sampleHashOf(src, sourceBytes);
      src.close();
    }
  }

  const std::string cachePath = std::string(kThumbDir) + "/" + name + ".thb";
  HalFile cf;
  if (Storage.openFileForRead("WALL", cachePath, cf)) {
    ThumbHeader head{};
    if (cf.read(&head, sizeof(head)) == static_cast<int>(sizeof(head)) && head.magic == kThumbMagic &&
        head.sourceBytes == sourceBytes && head.sampleHash == sample && head.modified == modified &&
        head.cellW == cellW && head.cellH == cellH && head.w > 0 && head.h > 0 && head.w <= cellW && head.h <= cellH &&
        head.ox >= 0 && head.oy >= 0 && head.ox + head.w <= cellW && head.oy + head.h <= cellH) {
      Thumb t;
      if (reuse) t.bits = std::move(reuse->bits);
      t.w = head.w;
      t.h = head.h;
      t.ox = head.ox;
      t.oy = head.oy;
      const size_t bytes = static_cast<size_t>((head.w + 7) / 8) * static_cast<size_t>(head.h);
      t.bits.resize(bytes);
      if (cf.read(t.bits.data(), bytes) == static_cast<int>(bytes)) {
        t.ok = true;
        cf.close();
        return t;
      }
    }
    cf.close();
  }

  if (decoded != nullptr) ++(*decoded);
  Thumb t = decodeThumb(path, cellW, cellH);
  if (!t.ok) return t;

  // Best effort: a cache that cannot be written must never break the picture.
  if (!Storage.exists(kThumbDir)) Storage.mkdir(kThumbDir);
  const std::string part = cachePath + ".part";
  HalFile out;
  if (Storage.openFileForWrite("WALL", part, out)) {
    ThumbHeader head{};
    head.magic = kThumbMagic;
    head.sourceBytes = static_cast<uint32_t>(sourceBytes);
    head.sampleHash = sample;
    head.modified = modified;
    head.cellW = cellW;
    head.cellH = cellH;
    head.w = t.w;
    head.h = t.h;
    head.ox = t.ox;
    head.oy = t.oy;
    const bool wrote =
        out.write(&head, sizeof(head)) == sizeof(head) && out.write(t.bits.data(), t.bits.size()) == t.bits.size();
    out.close();
    if (wrote) {
      Storage.remove(cachePath.c_str());
      if (!Storage.rename(part.c_str(), cachePath.c_str())) Storage.remove(part.c_str());
    } else {
      Storage.remove(part.c_str());
    }
  }
  return t;
}

// Build every thumbnail now, while the user is already watching a progress bar
// and expects to wait. Without this the cost merely MOVES to the first open,
// which is the screen that has to feel instant. Cancelling is honoured: a
// half-warmed cache is not wrong, only less warm, and the rest fills in lazily.

// A set bit is ink, placed at the offset the aspect fit worked out. A thumbnail
// that would not decode gets the cross rather than a blank box: an empty frame
// where a picture belongs reads as a crash, which cold testers have twice
// reported on this fork.
void WallpapersActivity::drawThumbInto(const Thumb& t, const fui::Rect& box, bool crop) const {
  if (!t.ok) {
    renderer.drawLine(box.x, box.y, box.right() - 1, box.bottom() - 1, true);
    renderer.drawLine(box.x, box.bottom() - 1, box.right() - 1, box.y, true);
    return;
  }
  const int bytesPerRow = (t.w + 7) / 8;
  if (crop) {
    const fui::BitmapRef image{t.bits.data(), static_cast<uint16_t>(t.w), static_cast<uint16_t>(t.h),
                               fui::BitmapFormat::BW1, false};
    auto target = toybox::makeTarget(renderer);
    target.bitmap(box, image, fui::BitmapMode::Cover, fui::Paint::solid(fui::Color::Black));
    return;
  }
  for (int y = 0; y < t.h; ++y) {
    const uint8_t* row = t.bits.data() + static_cast<size_t>(bytesPerRow) * y;
    for (int x = 0; x < t.w; ++x) {
      if (row[x >> 3] & (0x80 >> (x & 7))) renderer.drawPixel(box.x + t.ox + x, box.y + t.oy + y, true);
    }
  }
}

void WallpapersActivity::drawGrid(const wallpapersui::GridGeom& geom) {
  const int base = page_ * geom.perPage;
  const int specials = 0;
  const int total = specials + static_cast<int>(names_.size());

  for (int slot = 0; slot < geom.perPage; ++slot) {
    const int combined = base + slot;
    if (combined >= total) break;
    const fui::Rect th = wallpapersui::thumbRect(geom, slot);
    const int idx = combined - specials;
    const Thumb empty;
    const Thumb& t = slot < int(thumbs_.size()) ? thumbs_[static_cast<size_t>(slot)] : empty;
    drawThumbInto(t, th, true);

    // A hairline on every cell so a mostly-white wallpaper still reads as a
    // framed tile. This is not the selection signal: it is on every cell.
    renderer.drawRect(th.x, th.y, th.width, th.height, 1, true);
    if (selecting_ ? pendingSet_.test(size_t(idx)) : isChosen(idx)) drawMarker(th);

    // Caption (variants with one): the file name, fitted so it never truncates
    // into a missing glyph.
    if (geom.captionH > 0) {
      const fui::Rect cap = wallpapersui::captionRect(geom, slot);
      fui::GfxRendererTarget target = toybox::makeTarget(renderer);
      fui::TextStyle style = toybox::themeTokens().smallText;
      // smallText maps to the BODY cut; the caption wants the actual small cut
      // (toybox_10) so it fits the caption row instead of towering over it.
      style.font = fui::FONT_SLOT_SMALL;
      style.align = fui::TextAlign::Center;
      style.color = fui::Color::Black;
      // A real name, not the file name: a picker showing "blake-door.bmp" cut
      // mid-word looks unfinished. When the long form does not fit the cell we
      // fall back to a shorter NAME rather than an ellipsis, and log which was
      // used so the fit is measured rather than eyeballed.
      const wallpapers::DisplayName name = displayNameFor(names_[static_cast<size_t>(idx)]);
      std::string fitted = toybox::fitLines(target, name.full.c_str(), cap.width, 1, style);
      if (fitted != name.full) {
        fitted = toybox::fitLines(target, name.brief.c_str(), cap.width, 1, style);
        LOG_DBG("WALL", "caption fell back to brief: %s -> %s", name.full.c_str(), fitted.c_str());
      }
      if (fitted != name.full && fitted != name.brief) {
        LOG_ERR("WALL", "caption STILL cut: %s", fitted.c_str());
      }
      target.text(cap, fitted.c_str(), style);
    }
  }
}

void WallpapersActivity::drawMarker(const fui::Rect& th) const {
  const auto selection = wallpapersui::selectionGeometry(th);
  renderer.drawRect(th.x, th.y, th.width, th.height, 4, true);
  const int side = selection.check.width, x = selection.check.x, y = selection.check.y;
  renderer.fillRect(x, y, side, side, true);
  renderer.drawRect(x, y, side, side, 2, false);
  auto target = toybox::makeTarget(renderer);
  target.bitmap(fui::makeRect(x + 3, y + 3, side - 6, side - 6), fui::bitmapFromIcon(extra_check_32),
                fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::White));
}

// The whole set as one asset. Twenty-one separate downloads would be 42 TLS
// handshakes, because GitHub redirects release assets to a CDN host and
// HttpDownloader opens a fresh connection per hop with no session reuse: about
// a minute of dead time against roughly ten seconds for one file. Per-file
// "resume" would not have paid for it either, since downloadToFile has no Range
// support and deletes its destination before the first byte arrives.
//
// The pack needs no format: every wallpaper is exactly kWallpaperFileBytes, so
// it is a bare concatenation, image i lives at i * kWallpaperFileBytes, and the
// count is the file size divided by it. Built by tools_local/wallpapers/build_pack.py
// in the same order as the built-in table, which host-tests/wallpack asserts.
// The name is devicehost::mdnsName(), shared with the two activities that
// advertise it, so this screen cannot print a name the device does not answer
// to. It used to be a third copy of the literal "crossplay" here.
//
// Sharing the NAME is not sharing the LIFECYCLE: restartMdns lives in an
// anonymous namespace in CrossPointWebServerActivity.cpp and the web-server
// activity runs MDNS.end() in its own onExit, so mDNS is definitively not
// running when this screen opens. PR 1 calls MDNS.begin itself.

// Version 4, ECC_LOW, BYTE mode. Not the 114 QrUtils believes (that is the
// alphanumeric figure, and every URL with a lowercase letter is byte mode).
constexpr size_t kQrByteSafeLen = 78;

constexpr const char* kPackUrl = "https://github.com/ma-r-s/crossplay/releases/download/wallpapers/wallpapers.dat";
constexpr const char* kPackPart = "/wallpapers.dat.part";

// The HAL exposes a size only through an open file, and the difference between
// "absent" and "there but the wrong length" is the whole resume rule, so it is
// worth the open. A short file is a torn write, and Bitmap would accept it.

uint64_t fileSizeOf(const std::string& path) {
  HalFile f;
  if (!Storage.openFileForRead("WALL", path, f)) return 0;
  const uint64_t n = static_cast<uint64_t>(f.size());
  f.close();
  return n;
}

// A power cut mid-unpack leaves <name>.bmp.part behind. They are unambiguously
// ours and unambiguously incomplete, so they go on entry. Nothing else is
// touched: a .bmp of an unexpected length might be a wallpaper the user made
// elsewhere, and deleting a user's file to tidy up is not this app's call.
// Incomplete copies left by a power cut. Swept from EVERY place this app writes
// one, which is three: the library (the download's unpack), the set directory,
// and the card root beside the pinned file. A .part is skipped by
// findNextValidSleepImage on its extension, so one is inert rather than
// dangerous -- but it is 48KB of a card whose free space this app warns about.
void WallpapersActivity::sweepPartFiles() {
  const std::string rootPart = std::string(wallpapers::kPinnedSleep) + ".part";
  if (Storage.exists(rootPart.c_str())) {
    Storage.remove(rootPart.c_str());
    LOG_INF("WALL", "Swept incomplete %s", rootPart.c_str());
  }
  sweepPartFilesIn(wallpapers::kShuffleDir);
  sweepPartFilesIn(wallpapers::kLibraryDir);
}

void WallpapersActivity::sweepPartFilesIn(const char* dirPath) {
  auto dir = Storage.open(dirPath);
  if (!dir) return;
  std::vector<std::string> stale;
  for (;;) {
    auto entry = dir.openNextFile();
    if (!entry) break;
    char nameBuf[kNameMax] = {};
    entry.getName(nameBuf, sizeof(nameBuf));
    entry.close();
    const std::string name(nameBuf);
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".part") == 0) stale.push_back(name);
  }
  dir.close();
  for (const std::string& name : stale) {
    const std::string path = std::string(dirPath) + "/" + name;
    Storage.remove(path.c_str());
    LOG_INF("WALL", "Swept incomplete %s", path.c_str());
  }
}

void WallpapersActivity::showNotice(const char* headline, const char* body, const char* actionLabel,
                                    const fui::ActionId action) {
  noticeHead_ = headline;
  noticeBody_ = body;
  noticeAction_ = actionLabel;
  noticeActionId_ = action;
  view_ = View::Notice;
  interactionsReady_ = false;
  requestUpdate();
}

// The screen is a function of the card, with no remembered "already offered"
// flag to go stale or to make two identical cards show different things.
void WallpapersActivity::pickView() {
  slideshowAt_ = 0;
  view_ = names_.empty() ? View::Empty : View::Grid;
  interactionsReady_ = false;
}

// A hold landed on a wallpaper. Everything the three screens behind this need is
// SNAPSHOT here, in loop(), while the index is still the one the finger meant:
// render() must not go looking things up in names_, and the delete must not
// trust an index that uploading, deleting or re-sorting can renumber underneath
// it. The file name is the identity that survives all three.
void WallpapersActivity::openSheet(const int index, View destination) {
  RenderLock lock(*this);
  if (index < 0 || index >= static_cast<int>(names_.size())) return;
  if (destination == View::Sheet) contextReturn_ = view_ == View::Preview ? View::Preview : View::Grid;
  sheetIndex_ = index;
  sheetFile_ = names_[static_cast<size_t>(index)];
  sheetName_ = displayNameFor(sheetFile_).full;
  slideshowAt_ = 0;
  const auto original = originalPathFor(sheetFile_);
  auto source = Storage.open(original.c_str());
  const char* extension = std::strrchr(original.c_str(), '.');
  char format[16];
  pixelui::copyCaps(format, sizeof(format), extension ? extension + 1 : "");
  std::snprintf(sheetMeta_, sizeof(sheetMeta_), "%s | %llu KB", format,
                static_cast<unsigned long long>(source ? (source.fileSize64() + 1023) / 1024 : 0));
  sheetIsActive_ = wallpapers::saysOnSleepScreen(isChosen(index), sleepReach());
  sheetDetail_ = tr(STR_WALLPAPER_DELETE_ASK);
  view_ = destination;
  interactionsReady_ = false;
  LOG_INF("WALL", "hold sheet for %s (index %d, active %d, chosen %d, blocked %d, shadowed %d)", sheetFile_.c_str(),
          index, activeIndex_, static_cast<int>(chosen_.size()), static_cast<int>(sleepBlocked()),
          static_cast<int>(shadowedSet_));
  requestUpdate();
}

// The one destructive thing this app does.
//
// Resolves the NAME rather than reusing sheetIndex_. An index is a position in
// a sorted list, and this app re-sorts: scanLibrary() runs on every onEnter and
// after every delete, and deleting a built-in also flips 0 1 -> 2
// so every cell renumbers. A name that no longer resolves means the file is
// already gone, and doing nothing is right -- deleting "whatever is at slot 4
// now" is exactly the bug this app is shaped to avoid.
//
bool WallpapersActivity::deleteWallpaper() {
  if (sheetFile_.empty()) return false;
  const auto found = std::find(names_.begin(), names_.end(), sheetFile_);
  if (found == names_.end()) {
    LOG_INF("WALL", "delete: %s is no longer in the library; nothing to do", sheetFile_.c_str());
    return false;
  }
  const std::string path = originalPathFor(sheetFile_);
  if (!Storage.remove(path.c_str())) {
    LOG_ERR("WALL", "Card refused to delete %s", path.c_str());
    return false;
  }
  // The thumbnail cache is keyed on the file NAME, so a later upload reusing the
  // name would otherwise be drawn from the deleted picture's cache entry until
  // something else invalidated it. thumbFor's staleness check is the source's
  // byte count plus a three-read content sample plus the cell size -- not the
  // mtime -- so a same-named replacement is caught by its CONTENT and this
  // removal is belt to that brace rather than the only guard. Removing it costs
  // one decode; leaving it costs the wrong picture.
  const std::string cache = std::string(kThumbDir) + "/" + sheetFile_ + ".thb";
  Storage.remove(cache.c_str());

  // The pin marker names a file that is gone. loadSelection() already fails to
  // match it, so the grid is right today -- but a later upload with the SAME
  // name would match it again and wear the "in use" border while /sleep.bmp
  // still holds the deleted picture. The marker is a hint about the library;
  // when its subject leaves the library the hint is stale, not just unmatched.
  char marker[kNameMax] = {};
  if (Storage.readFileToBuffer(wallpapers::kActiveMarker, marker, sizeof(marker)) > 0) {
    for (char* c = marker; *c; ++c) {
      if (*c == '\n' || *c == '\r') {
        *c = '\0';
        break;
      }
    }
    if (sheetFile_ == marker) Storage.remove(wallpapers::kActiveMarker);
  }
  LOG_INF("WALL", "deleted %s", path.c_str());

  // Re-derive the gallery from the card after deleting an image.
  // Rebuild indices and thumbnails before accepting another image action.
  sheetIndex_ = -1;
  sheetFile_.clear();
  scanLibrary();
  loadSelection();
  clampPage();
  cachedPage_ = -1;
  prepareAt_ = prepareErrors_ = 0;
  fetchTotal_ = int(names_.size());
  const auto geom = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext());
  prepareW_ = geom.cellW;
  prepareH_ = geom.cellH;
  job_ = Job::Thumbnails;
  view_ = View::Preparing;
  painted_ = false;
  progressAt_ = millis();
  return true;
}

// The wallpaper at 1:1, and nothing else. No chrome, no button hints, no border:
// what is on the panel here is what the sleep screen puts there, which is the
// whole question Mario asked ("a way to preview the wallpaper without turning
// off the device"). Anything drawn over it would be answering a different one.
//
// The placement arithmetic is SleepActivity's non-oversize branch, reproduced
// rather than called: calculateBitmapPlacement is file-local to SleepActivity
// (upstream's file, not ours to widen). It is the identity for every wallpaper
// this app ships or produces -- kWallpaperFileBytes pins them all at 480x800,
// exactly the panel -- so the two agree on every file that can be here. A
// stray hand-copied BMP of another size centres instead of cropping; it is
// still the picture, and it is not a file any path in this app creates.
void WallpapersActivity::renderPreview() {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();

  // drawBitmap is a SILENT NO-OP while the font cache is scanning
  // (GfxRenderer.cpp:1365). Asking first, because the alternative is a cleared
  // panel with nothing on it and a `drawn = true` derived from parseHeaders
  // rather than from the draw -- a blank screen this fork has twice had cold
  // testers read as a crash.
  const FontCacheManager* fonts = renderer.getFontCacheManager();
  const bool canDraw = fonts == nullptr || !fonts->isScanning();

  const std::string path = sourcePathFor(sheetFile_);
  HalFile file;
  bool drawn = false;
  if (canDraw && Storage.openFileForRead("WALL", path, file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      const int w = bitmap.getWidth();
      const int h = bitmap.getHeight();
      const int x = w <= pageWidth ? (pageWidth - w) / 2 : 0;
      const int y = h <= pageHeight ? (pageHeight - h) / 2 : 0;
      renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0.0f, 0.0f);
      // The cover filter, because the sheet PROMISES "exactly as the sleep
      // screen draws it" and renderBitmapSleepScreen inverts here
      // (SleepActivity.cpp:631). The setting is user-reachable, and without
      // this the one user who chose it is shown the negative of their
      // wallpaper and told it is the real thing.
      if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
        renderer.invertScreen();
      }
      drawn = true;
    }
    file.close();
  }

  HeaderBackTapTarget::clear();
  // A Frame with nothing built empties the hit table, so the sheet's DELETE
  // cannot still be routable while a full-screen picture covers it. It also
  // gives the one failure path here somewhere to put words: a blank panel reads
  // as a crash, twice confirmed by cold testers in this fork.
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  target.setFont(1, toybox::kButtonFontId);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, device, noInput, interactions_, nullptr, toybox::headerStatus(renderer));
  toybox::Screen surface(frame, toybox::themeTokens());
  if (!drawn) {
    LOG_ERR("WALL", "preview: cannot draw %s", path.c_str());
    if (view_ == View::Preview) {
      wallpapersui::NoticeModel model;
      model.headline = tr(STR_UI_CANNOT_PREVIEW_215D84);
      model.body = tr(STR_UI_THIS_FILE_COULD_NOT_BE_OPENED_495DB9);
      wallpapersui::buildNotice(surface, model);
    }
  }
  // File actions remain available when the image itself is unreadable.
  if (view_ == View::Sheet) {
    wallpapersui::SheetModel model;
    model.name = sheetName_.c_str();
    model.detail = sheetMeta_;
    model.isActive = sheetIsActive_;
    wallpapersui::buildSheet(surface, model);
  }
  interactionsReady_ = view_ == View::Sheet;
  toybox::reportOverflow(interactions_, tr(STR_WALLPAPERS_TITLE), &renderer);
  // HALF_REFRESH, the same waveform every sleep-screen paint uses
  // (SleepActivity.cpp:609,643). The default is FAST, and under FAST a dense
  // 1-bit plate ghosts the screen it replaced and reads lower-contrast than the
  // real thing -- so the preview would differ from the sleep screen in the one
  // way a preview exists to rule out. The notice path takes it too, so the
  // failure and the picture do not paint with different waveforms.
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  painted_ = true;
}

void WallpapersActivity::loop() {
  if (job_ != Job::None) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    int x = 0, y = 0;
    if (painted_ && mappedInput.wasScreenTapped(x, y) && interactionsReady_) {
      fui::InputSnapshot input{};
      input.touchReleased = true;
      input.touchX = x;
      input.touchY = y;
      if (interactions_.route(input).action == wallpapersui::ActionPrepareCancel) {
        shelf::leave(renderer, mappedInput);
        return;
      }
    }
    if (painted_) preparationStep();
    return;
  }
  if (view_ == View::Grid) {
    const auto geom = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext(), selecting_);
    if (cachedPage_ != page_ || cachedPerPage_ != geom.perPage || cachedCellW_ != geom.cellW ||
        cachedCellH_ != geom.cellH || thumbs_.empty()) {
      RenderLock lock;
      ensureThumbsForPage();
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::ConfirmSet) {
      view_ = View::Grid;
      interactionsReady_ = false;
      requestUpdate();
      return;
    }
    if (view_ == View::Grid && selecting_) {
      selecting_ = false;
      pendingSet_.reset();
      interactionsReady_ = false;
      requestUpdate();
      return;
    }
    // Back unwinds the hold branch one step at a time. From the confirm it goes
    // to the sheet, NOT to the grid: Back on a confirm means "not that", and
    // dropping the user two screens back would make them start the hold again
    // to reach the preview they were actually after.
    if (view_ == View::Confirm) {
      view_ = View::Sheet;
      interactionsReady_ = false;
      requestUpdate();
      return;
    }
    if (view_ == View::Sheet && contextReturn_ == View::Preview) {
      view_ = View::Preview;
      interactionsReady_ = false;
      requestUpdate();
      return;
    }
    if (view_ == View::Preview || view_ == View::Sheet) {
      pickView();
      requestUpdate();
      return;
    }
    if (view_ == View::Notice) {
      pickView();
      requestUpdate();
      return;
    }
    shelf::leave(renderer, mappedInput);
    return;
  }

  if (view_ == View::ConfirmSet) {
    int x = 0, y = 0;
    if (!interactionsReady_ || !surfaceRevealed() || !mappedInput.wasScreenTapped(x, y)) return;
    fui::InputSnapshot input{};
    input.touchReleased = true;
    input.touchX = x;
    input.touchY = y;
    const auto action = interactions_.route(input).action;
    if (action == wallpapersui::ActionSelectionCancel) {
      view_ = View::Grid;
      interactionsReady_ = false;
      requestUpdate();
    } else if (action == wallpapersui::ActionSelectionApply)
      queueSelection();
    return;
  }
  if (view_ == View::Preview) {
    int x = 0, y = 0;
    if (mappedInput.wasScreenLongPress(x, y)) {
      mappedInput.swallowCurrentTouch();
      openSheet(sheetIndex_);
      return;
    }
    if (mappedInput.wasScreenTapped(x, y)) {
      if (mappedInput.tapWasHeldLong())
        openSheet(sheetIndex_);
      else if (slideshowAt_) {
        slideshowAt_ = 0;
        requestUpdate();
      }
      return;
    }
    if (slideshowAt_ && static_cast<long>(millis() - slideshowAt_) >= 0 && !names_.empty()) {
      const int next = (sheetIndex_ + 1) % static_cast<int>(names_.size());
      openSheet(next, View::Preview);
      slideshowAt_ = millis() + 10000;
      requestUpdate();
    }
    return;
  }
  // Notice, Sheet and Confirm screens carry real buttons, so their
  // taps go through Interactions rather than the grid's geometry hit-test.
  // Interactions::route() refuses a tap routed against a table the panel has
  // not shown yet, which is what stops a tap aimed at the screen underneath
  // from landing on the one that replaced it during a 0.3-2s e-ink repaint.
  if (view_ == View::Notice || view_ == View::Sheet || view_ == View::Confirm) {
    int ax = 0;
    int ay = 0;
    if (!sidebuttons::poll(interactions_, mappedInput, ax, ay) || !interactionsReady_) return;
    fui::InputSnapshot input{};
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(ax);
    input.touchY = static_cast<int16_t>(ay);
    if (!surfaceRevealed()) return;
    const fui::ActionEvent action = sidebuttons::route(interactions_, input, mappedInput, *this);
    if (!action.action && view_ == View::Sheet && sidebuttons::hasActivation(ax, ay)) {
      const auto box = wallpapersui::contextRect(toybox::makeTarget(renderer).deviceContext());
      if (ax < box.x || ax >= box.right() || ay < box.y || ay >= box.bottom()) {
        if (contextReturn_ == View::Preview) {
          view_ = View::Preview;
          interactionsReady_ = false;
        } else {
          pickView();
        }
        requestUpdate();
      }
      return;
    }
    switch (action.action) {
      case wallpapersui::ActionDismiss:
        pickView();
        requestUpdate();
        return;
      case wallpapersui::ActionSet:
        if (setWallpaper(sheetIndex_)) {
          openSheet(sheetIndex_, View::Preview);
          interactionsReady_ = false;
          requestUpdate();
        } else {
          showNotice(tr(STR_UI_COULD_NOT_CHANGE_THE_SET_713147), tr(STR_UI_YOUR_SLEEP_SCREEN_IS_UNCHANGED_THE_CB239A),
                     tr(STR_OK_BUTTON), wallpapersui::ActionDismiss);
        }
        return;
      case wallpapersui::ActionSlideshow:
        openSheet(sheetIndex_, View::Preview);
        slideshowAt_ = millis() + 10000;
        interactionsReady_ = false;
        requestUpdate();
        return;
      case wallpapersui::ActionExitPreview:
        pickView();
        requestUpdate();
        return;
      case wallpapersui::ActionDelete:
        // Opens the question. Deletes nothing -- which is what makes this the
        // safe half of the pair the confirm reuses the pixels of.
        view_ = View::Confirm;
        interactionsReady_ = false;
        requestUpdate();
        return;
      case wallpapersui::ActionKeep:
        view_ = View::Sheet;
        interactionsReady_ = false;
        requestUpdate();
        return;
      case wallpapersui::ActionConfirmDelete:
        // A failed delete used to discard its bool and repaint the identical
        // confirm: the panel flashed and came back unchanged, with no way to
        // tell a card that refused from a touch that was dropped. Every other
        // failure in this app goes through showNotice, and this is the one the
        // user is watching hardest (a-silent-screen-reads-as-a-crash).
        if (!deleteWallpaper()) {
          showNotice(tr(STR_UI_NOT_DELETED_FD860F), tr(STR_UI_THE_CARD_WOULD_NOT_REMOVE_THIS_A66952), "OK",
                     wallpapersui::ActionDismiss);
          return;
        }
        interactionsReady_ = false;
        requestUpdate();
        return;
      default:
        return;
    }
  }
  if (view_ != View::Grid) return;

  const int pages = pageCount();
  if (interactionsReady_ && surfaceRevealed()) {
    if (const int direction = sidebuttons::horizontalStep(mappedInput)) {
      const int next = std::clamp(page_ + direction, 0, pages - 1);
      if (next != page_) {
        showGalleryPage(next);
      }
      return;
    }
  }
  if (const int direction = sidebuttons::shortStep(mappedInput)) {
    const int next = std::clamp(page_ + direction, 0, pages - 1);
    if (next != page_) {
      showGalleryPage(next);
    }
    return;
  }

  int tapX = 0;
  int tapY = 0;
  const bool longPress = mappedInput.wasScreenLongPress(tapX, tapY);
  if (pages > 1 && (mappedInput.wasLongPressed(MappedInputManager::Button::NavNext, 500) ||
                    mappedInput.wasLongPressed(MappedInputManager::Button::NavPrevious, 500))) {
    const int direction = mappedInput.isPressed(MappedInputManager::Button::NavNext) ? 1 : -1;
    const int next = std::clamp(page_ + direction, 0, pages - 1);
    if (next != page_) {
      showGalleryPage(next);
    }
    return;
  }
  if ((!longPress && !sidebuttons::poll(interactions_, mappedInput, tapX, tapY)) || !interactionsReady_) return;
  if (!longPress) {
    fui::InputSnapshot input{};
    input.touchReleased = true;
    input.touchX = tapX;
    input.touchY = tapY;
    const auto event = sidebuttons::route(interactions_, input, mappedInput, *this);
    switch (event.action) {
      case wallpapersui::ActionSelectSet:
        beginSelection();
        return;
      case wallpapersui::ActionSelectAll:
        toggleAllSelection();
        return;
      case wallpapersui::ActionSelectionDone:
        confirmSelection();
        return;
      case wallpapersui::ActionGalleryPage: {
        const int next = std::clamp(page_ + event.value, 0, pageCount() - 1);
        if (next != page_) showGalleryPage(next);
        return;
      }
      default:
        break;
    }
  }

  const wallpapersui::GridGeom geom = wallpapersui::gridGeom(toybox::makeTarget(renderer).deviceContext(), selecting_);

  if (pages > 1) {
    auto target = toybox::makeTarget(renderer);
    const auto safe = target.deviceContext().safeRect();
    const auto nav = pageui::layout(wallpapersui::galleryPager(target.deviceContext(), selecting_));
    const auto contains = [&](fui::Rect r) {
      return tapX >= r.x && tapX < r.right() && tapY >= r.y && tapY < r.bottom();
    };
    if (contains(nav.before) || contains(nav.after)) {
      const int next = std::clamp(page_ + (contains(nav.before) ? -1 : 1), 0, pages - 1);
      if (next != page_) {
        showGalleryPage(next);
      }
      return;
    }
  }

  // Resolve a cell on the currently displayed page.
  const int slot = wallpapersui::cellAt(geom, tapX, tapY);
  if (slot < 0) return;
  if (!surfaceRevealed()) {
    // Still refused for a REAL remap (a page turn or the library changing under
    // the finger), never for a moved selection any more. Logged because "my tap
    // did nothing" is otherwise indistinguishable from a dropped touch, and this
    // is the line that tells the two apart on hardware.
    LOG_INF("WALL", "tap refused: surface not yet seen (a real remap, not the marker)");
    return;
  }
  const int combined = page_ * geom.perPage + slot;
  const int specials = 0;
  const int total = specials + static_cast<int>(names_.size());
  if (combined >= total) return;
  const bool held = longPress || mappedInput.tapWasHeldLong();
  if (longPress) mappedInput.swallowCurrentTouch();
  const int idx = combined - specials;
  if (selecting_) {
    if (idx >= 0 && idx < int(names_.size()) && usableSet_.test(size_t(idx)) && !held) {
      pendingSet_.flip(size_t(idx));
      requestUpdate();
    }
    return;
  }
  if (!usableSet_.test(size_t(idx))) return;
  // Holding exposes actions; tapping only previews the image.
  switch (wallpapers::cellAction(held, idx)) {
    case wallpapers::CellAction::None:
      return;
    case wallpapers::CellAction::Sheet:
      openSheet(idx);
      return;
    case wallpapers::CellAction::Preview:
      openSheet(idx, View::Preview);
      interactionsReady_ = false;
      requestUpdate();
      return;
  }
}

void WallpapersActivity::render(RenderLock&&) {
  const uint32_t tPaint = millis();
  clampPage();
  // Before anything toybox: the preview draws no chrome and takes no theme.
  if (view_ == View::Preview || view_ == View::Sheet) {
    renderPreview();
    LOG_INF("WALL", "render preview took %ums", millis() - tPaint);
    return;
  }
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  // Keep paragraphs compact within the shared Jersey family.
  if (view_ != View::Grid && view_ != View::Preparing && view_ != View::ConfirmSet)
    target.setFont(1, toybox::kButtonFontId);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_, nullptr, toybox::headerStatus(renderer));
  toybox::Screen surface(frame);

  switch (view_) {
    case View::Preparing: {
      wallpapersui::preparation(surface, prepareAt_, fetchTotal_, job_ == Job::Apply, animationFrame_);
      break;
    }
    case View::ConfirmSet: {
      wallpapersui::selectionConfirm(surface, int(pendingSet_.count()));
      break;
    }
    case View::Confirm: {
      wallpapersui::ConfirmModel model;
      model.name = sheetName_.c_str();
      model.consequence = sheetDetail_.c_str();
      wallpapersui::buildConfirm(surface, model);
      break;
    }
    case View::Notice: {
      wallpapersui::NoticeModel model;
      model.headline = noticeHead_.c_str();
      model.body = noticeBody_.c_str();
      model.actionLabel = noticeAction_;
      model.action = noticeActionId_;
      wallpapersui::buildNotice(surface, model);
      break;
    }
    case View::Empty: {
      wallpapersui::EmptyModel model;
      model.title = tr(STR_WALLPAPERS_TITLE);
      wallpapersui::buildEmpty(surface, model);
      break;
    }
    case View::Grid: {
      const wallpapersui::GridGeom geom = wallpapersui::gridGeom(device, selecting_);
      wallpapersui::GridChromeModel model;
      model.rightLabel = nullptr;
      model.selecting = selecting_;
      model.selectedCount = int(pendingSet_.count());
      // The current SD set supplies the active marker.
      model.hasActive = !chosen_.empty();
      model.noteOn = SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::NOTE;
      // Rebuilt from SETTINGS and the card every paint rather than cached at
      // selection time: the reach half is only knowable from the live settings,
      // and a cached sentence is how #354's caveat came to be suppressed for a
      // whole app session. The pointer is a literal out of WallpapersCore except
      // for the one line that carries a count, which is built into note_ -- a
      // member, so it outlives the paint.
      model.note = currentSleepNote();
      wallpapersui::buildGridChrome(surface, model);
      const int total = static_cast<int>(names_.size());
      for (int slot = 0; slot < geom.perPage && page_ * geom.perPage + slot < total; ++slot)
        frame.navigationGrid(wallpapersui::cellRect(geom, slot), 1, 1);
      if (names_.empty()) {
        pixelui::text(surface.target(), surface.body(), tr(STR_WALLPAPER_FOLDERS), 0, freeink::ui::Color::Black,
                      freeink::ui::TextAlign::Center);
      }

      // The chrome is a screen tree; the grid is the app's own surface, drawn
      // after it into the body the same way chess draws its board.
      drawGrid(geom);
      wallpapersui::selectionFooter(surface, selecting_, page_, pageCount(),
                                    usableSet_.any() && pendingSet_ == usableSet_);
      break;
    }
    case View::Preview:
    case View::Sheet:
      break;
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, tr(STR_WALLPAPERS_TITLE), &renderer);

  const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  // The slot count, every paint. reportOverflow only speaks once the table is
  // already full, which is the moment a control that draws normally has stopped
  // being tappable with nothing on screen to say so; the number beside it is
  // what says how close a screen was getting before that happened.
  LOG_INF("WALL", "render view=%d took %ums, %d/%d slots", static_cast<int>(view_), millis() - tPaint,
          static_cast<int>(interactions_.count()), static_cast<int>(toybox::kMaxInteractions));
  renderer.displayBuffer();
  painted_ = true;
}

uint32_t WallpapersActivity::surfaceMeaning() const {
  // Page, view and library changes invalidate positions. Marking a wallpaper
  // changes no gallery action and therefore needs no extra input gate.
  return wallpapersui::gridMeaning(page_, static_cast<int>(view_), static_cast<int>(names_.size()), 0, selecting_);
}
