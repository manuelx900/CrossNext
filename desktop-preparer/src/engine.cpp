#include <ArduinoJson.h>
#include <Epub/Page.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <I18n.h>
#include <SdCardFont.h>
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>
#include <emscripten/emscripten.h>

#include <bit>

#include "CrossPointSettings.h"
#include "DeviceProfile.h"
#include "ReaderFontSizes.h"
#include "SdCardFontSystem.h"
#include "apps_local/bookprep/BookPreparation.h"
#include "apps_local/bookprep/BookPreparationGeometry.h"
#include "apps_local/bookprep/PreparationKey.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ParagraphIndentMigration.h"

static_assert(sizeof(size_t) == 4 && sizeof(long) == 4 && sizeof(float) == 4 && sizeof(bool) == 1);
static_assert(std::endian::native == std::endian::little);

GfxRenderer renderer(display);
SdCardFontSystem sdFontSystem;
FontDecompressor fontDecompressor;
FontCacheManager fontCacheManager(renderer.getFontMap(), renderer.getSdCardFonts(), renderer.getTtfFonts());
namespace {
EpdFont small(&notosans_8_regular), reg10(&ubuntu_10_regular), bold10(&ubuntu_10_bold);
EpdFont reg12(&ubuntu_12_regular), bold12(&ubuntu_12_bold);
EpdFontFamily smallFamily(&small), family10(&reg10, &bold10), family12(&reg12, &bold12);
std::unique_ptr<bookprep::Job> job;
ReaderRenderSpec active;
std::string output;
int coverHeights[bookprep::Job::kMaxThumbnailHeights]{};
freeink::ui::Rect nativeAreas[2]{};
}  // namespace

namespace toybox {
void ensureFonts(GfxRenderer&) {}
}  // namespace toybox

extern "C" {
EMSCRIPTEN_KEEPALIVE const char* prepare_orientation(int orientation) {
  if (orientation < 0 || orientation > 3) return "{\"error\":\"Invalid orientation\"}";
  SETTINGS.orientation = static_cast<uint8_t>(orientation);
  renderer.setOrientation(static_cast<GfxRenderer::Orientation>(orientation));
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int margin = SETTINGS.screenMargin;
  active.viewportWidth = renderer.getScreenWidth() - left - right - margin * 2;
  active.viewportHeight = renderer.getScreenHeight() - top - bottom - margin -
                          std::max(margin, UITheme::getInstance().getStatusBarHeight());
  bookprep::Job geometry;
  bookprep::collectCoverSizes(geometry, renderer);
  std::copy(std::begin(geometry.thumbnailHeights), std::end(geometry.thumbnailHeights), std::begin(coverHeights));
  std::copy(std::begin(geometry.nativeAreas), std::end(geometry.nativeAreas), std::begin(nativeAreas));
  JsonDocument reply;
  auto heights = reply["covers"].to<JsonArray>();
  for (auto h : coverHeights)
    if (h > 0) heights.add(h);
  auto native = reply["nativeCovers"].to<JsonArray>();
  for (const auto& area : nativeAreas) {
    const auto key = libraryui::PreparedCoverCache::key(renderer, area);
    if (!key) continue;
    auto row = native.add<JsonObject>();
    row["height"] = area.height;
    row["width"] = area.width;
    row["bytes"] = renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height);
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(key));
    row["key"] = hex;
  }
  output.clear();
  serializeJson(reply, output);
  return output.c_str();
}
EMSCRIPTEN_KEEPALIVE const char* prepare_initialize(const char* configuration) {
  desktopdevice::apply();
  JsonDocument doc;
  if (deserializeJson(doc, configuration)) return "{\"error\":\"Invalid profile\"}";
  const auto read = [&](const char* key, uint8_t& value) {
    if (doc[key].is<bool>())
      value = doc[key].as<bool>() ? 1 : 0;
    else if (doc[key].is<unsigned>() && doc[key].as<unsigned>() <= 255)
      value = doc[key];
  };
  read("fontPointSize", SETTINGS.fontPointSize);
  read("orientation", SETTINGS.orientation);
  read("screenMargin", SETTINGS.screenMargin);
  read("lineSpacing", SETTINGS.lineSpacing);
  read("characterSpacing", SETTINGS.characterSpacing);
  read("wordSpacing", SETTINGS.wordSpacing);
  read("paragraphAlignment", SETTINGS.paragraphAlignment);
  read("extraParagraphSpacing", SETTINGS.extraParagraphSpacing);
  const bool savedIndent = doc["paragraphIndentSpaces"].is<int>();
  SETTINGS.paragraphIndentSpaces = migrateParagraphIndentSpaces(
      savedIndent, savedIndent ? doc["paragraphIndentSpaces"].as<int>() : 0, SETTINGS.extraParagraphSpacing != 0);
  read("hyphenationEnabled", SETTINGS.hyphenationEnabled);
  read("embeddedStyle", SETTINGS.embeddedStyle);
  read("imageRendering", SETTINGS.imageRendering);
  read("focusReadingEnabled", SETTINGS.focusReadingEnabled);
  read("fontWeight", SETTINGS.fontWeight);
  read("epubEngine", SETTINGS.epubEngine);
  read("xtcCoverRotation", SETTINGS.xtcCoverRotation);
  read("uiTheme", SETTINGS.uiTheme);
  read("homeLayout", SETTINGS.homeLayout);
  if (SETTINGS.homeLayout == CrossPointSettings::HOME_BOOX && (doc["homeLayoutVersion"] | 0) < 2)
    SETTINGS.homeLayout = CrossPointSettings::HOME_CAROUSEL;
  else if (SETTINGS.homeLayout >= CrossPointSettings::HOME_LAYOUT_COUNT)
    SETTINGS.homeLayout = CrossPointSettings::HOME_FRAMED;
  read("interfaceThemeStyle", SETTINGS.interfaceThemeStyle);
  read("menuFontStyle", SETTINGS.menuFontStyle);
  UITheme::getInstance().reload();
  read("statusBarChapterPageCount", SETTINGS.statusBarChapterPageCount);
  read("statusBarBookProgressPercentage", SETTINGS.statusBarBookProgressPercentage);
  read("statusBarTitle", SETTINGS.statusBarTitle);
  read("statusBarBattery", SETTINGS.statusBarBattery);
  read("statusBarClock", SETTINGS.statusBarClock);
  read("statusBarProgressBar", SETTINGS.statusBarProgressBar);
  read("statusBarProgressBarThickness", SETTINGS.statusBarProgressBarThickness);
  snprintf(SETTINGS.sdFontFamilyName, sizeof(SETTINGS.sdFontFamilyName), "%s", doc["sdFontFamilyName"] | "");
  Storage.begin();
  I18N.setLanguage(I18n::languageFromCode(doc["language"] | "en"));
  const char* languagePack = doc["languagePack"] | "";
  if (*languagePack && !I18N.loadLanguagePack(languagePack)) return "{\"error\":\"Language resources unavailable\"}";
  display.begin();
  renderer.begin();
  if (!fontDecompressor.init()) return "{\"error\":\"Font decompressor unavailable\"}";
  fontCacheManager.setFontDecompressor(&fontDecompressor);
  renderer.setFontCacheManager(&fontCacheManager);
  renderer.insertFont(SMALL_FONT_ID, smallFamily);
  renderer.insertFont(UI_10_FONT_ID, family10);
  renderer.insertFont(UI_12_FONT_ID, family12);
  sdFontSystem.begin(renderer);
  renderer.setOrientation(static_cast<GfxRenderer::Orientation>(SETTINGS.orientation));
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int margin = SETTINGS.screenMargin;
  active = SETTINGS.readerRenderSpec(renderer.getScreenWidth() - left - right - margin * 2,
                                     renderer.getScreenHeight() - top - bottom - margin -
                                         std::max(margin, UITheme::getInstance().getStatusBarHeight()));
  JsonDocument reply;
  reply["device"] = BoardConfig::ACTIVE.name;
  auto insets = reply["viewableInsets"].to<JsonArray>();
  const auto& bezel = BoardConfig::ACTIVE.viewableInsets;
  for (auto value : {bezel.top, bezel.right, bezel.bottom, bezel.left}) insets.add(value);
  reply["cacheVersion"] = Section::cacheVersion();
  reply["paragraphIndentSpaces"] = SETTINGS.paragraphIndentSpaces;
  reply["viewportWidth"] = active.viewportWidth;
  reply["viewportHeight"] = active.viewportHeight;
  auto catalog = reply["fonts"].to<JsonArray>();
  for (int n = 0; n < readerFontFamilyCount(&sdFontSystem.registry()); ++n) {
    auto row = catalog.add<JsonObject>();
    const auto* font = readerFontFamilyAt(&sdFontSystem.registry(), n);
    row["name"] = font ? font->name.c_str() : "Ubuntu";
    auto sizes = row["sizes"].to<JsonArray>();
    for (auto point : readerFontPointSizes(&sdFontSystem.registry(), font ? font->name.c_str() : "")) sizes.add(point);
  }
  bookprep::Job geometry;
  bookprep::collectCoverSizes(geometry, renderer);
  std::copy(std::begin(geometry.thumbnailHeights), std::end(geometry.thumbnailHeights), std::begin(coverHeights));
  std::copy(std::begin(geometry.nativeAreas), std::end(geometry.nativeAreas), std::begin(nativeAreas));
  auto heights = reply["covers"].to<JsonArray>();
  for (auto h : geometry.thumbnailHeights)
    if (h > 0) heights.add(h);
  auto native = reply["nativeCovers"].to<JsonArray>();
  for (const auto& area : nativeAreas) {
    const auto key = libraryui::PreparedCoverCache::key(renderer, area);
    if (!key) continue;
    auto row = native.add<JsonObject>();
    row["height"] = area.height;
    row["width"] = area.width;
    row["bytes"] = renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height);
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(key));
    row["key"] = hex;
  }
  output.clear();
  serializeJson(reply, output);
  return output.c_str();
}

EMSCRIPTEN_KEEPALIVE int prepare_font(const char* family, int points) {
  uint8_t size = points;
  const int id = sdFontSystem.loadForPreparation(renderer, family, size);
  if (!id || size != points) return 0;
  active.fontId = id;
  active.fontContentHash = *family && SETTINGS.sdFontHashResolver
                               ? SETTINGS.sdFontHashResolver(SETTINGS.sdFontResolverCtx, family, size)
                               : 0;
  return 1;
}

EMSCRIPTEN_KEEPALIVE int prepare_open(const char* path, int rotate) {
  job = makeUniqueNoThrow<bookprep::Job>();
  if (!job) return 0;
  job->spec = active;
  job->rotateXtc = rotate != 0;
  bookprep::collectCoverSizes(*job, renderer);
  if (strrchr(path, '.') && !strcasecmp(strrchr(path, '.'), ".epub")) {
    std::shared_ptr<Epub> book = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
    GfxRenderer::FrameBufferLoan loan(renderer);
    if (!book || !book->load(true, !active.embeddedStyle)) return 0;
    return job->beginBook(std::move(book));
  }
  std::shared_ptr<Xtc> book = makeUniqueNoThrow<Xtc>(path, "/.crosspoint", rotate != 0);
  if (!book || !book->load()) return 0;
  book->setupCacheDir();
  return job->beginBook(std::move(book));
}
EMSCRIPTEN_KEEPALIVE int prepare_step() {
  if (!job) return -1;
  job->step(renderer);
  if (job->phase == bookprep::Phase::Preparing) return 1;
  return job->phase == bookprep::Phase::Failed ? -1 : 0;
}
EMSCRIPTEN_KEEPALIVE const char* prepare_status() {
  JsonDocument doc;
  if (job) {
    doc["phase"] = int(job->phase);
    doc["done"] = job->progressDone();
    doc["total"] = job->progressTotal();
    doc["chapter"] = job->chapter;
    doc["chapters"] = job->chapterCount;
    doc["failed"] = job->failed;
  }
  output.clear();
  serializeJson(doc, output);
  return output.c_str();
}
EMSCRIPTEN_KEEPALIVE void prepare_cancel() {
  if (job) job->cancel();
  job.reset();
}
EMSCRIPTEN_KEEPALIVE int prepare_native_covers(const char* path, int rotate) {
  renderer.setRenderMode(GfxRenderer::BW);
  const bool epub = strrchr(path, '.') && !strcasecmp(strrchr(path, '.'), ".epub");
  libraryui::PreparedCoverCache cache;
  for (const auto& area : nativeAreas) {
    const auto key = libraryui::PreparedCoverCache::key(renderer, area);
    if (!key) continue;
    char filename[112];
    std::snprintf(filename, sizeof(filename), "/.crosspoint/%s_%lu/thumb_%s%d.bmp", epub ? "epub" : "xtc",
                  static_cast<unsigned long>(deviceStringHash(path)), !epub && rotate ? "ccw3_" : "", area.height);
    HalFile file;
    if (!Storage.openFileForRead("PREP", filename, file)) return 0;
    if (!file.fileSize64()) continue;
    Bitmap bitmap(file);
    if (cache.load(file, renderer, area, key, false)) continue;
    if (!cache.prepare(renderer, bitmap, area, key, filename)) return 0;
    file.close();
    while (cache.writing()) cache.step();
    if (!Storage.openFileForRead("PREP", filename, file) || !cache.load(file, renderer, area, key, false)) return 0;
  }
  return 1;
}
EMSCRIPTEN_KEEPALIVE uint32_t prepare_hash(const char* path) { return deviceStringHash(path); }
EMSCRIPTEN_KEEPALIVE const char* prepare_stamp(int epub, int rotate) {
  const auto key =
      bookprep::preparationSignature(active, epub != 0, rotate != 0, coverHeights, bookprep::Job::kMaxThumbnailHeights);
  char name[64];
  std::snprintf(name, sizeof(name), "%sready-v1-%08lx.bin", epub ? "sections/" : "", static_cast<unsigned long>(key));
  output = name;
  return output.c_str();
}
EMSCRIPTEN_KEEPALIVE int prepare_verify(const char* path) {
  std::shared_ptr<Epub> book = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
  if (!book || !book->load(false, !active.embeddedStyle)) return -1;
  int pages = 0;
  for (int index = 0; index < book->getSpineItemsCount(); ++index) {
    auto section = makeUniqueNoThrow<Section>(book, index, renderer);
    if (!section || !section->loadPreparedSectionFile(active) || section->isPartial()) return -2;
    for (int page = 0; page < section->pageCount; ++page) {
      auto loaded = section->loadPage(page);
      if (!loaded) return -3;
      ++pages;
    }
  }
  return pages;
}
}
int main() { return 0; }
