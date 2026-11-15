
#include <CrossPointSettings.h>
#include <FreeInkApp.h>
#include <HalStorage.h>

#include <cassert>
#include <set>

#include "FontTarget.h"
#include "src/activities/util/KeyboardLayoutSet.h"
#include "src/apps_local/ui/PixelMenu.h"
#include "src/components/LanguageUiFonts.h"
#include "src/fontIds.h"
namespace toybox {
inline fui::StyleSet rowStyles() { return pixelui::rowStyles(); }
}  // namespace toybox
#include "src/apps_local/ui/SettingsMenuStyle.h"
const char* I18n::getLanguageName(Language l) const { return LANGUAGE_NAMES[unsigned(l)]; }
struct RenderLock {};
static std::map<int, EpdFontFamily> registeredFonts;
class GfxRenderer {
 public:
  FontTarget& t;
  const std::map<int, EpdFontFamily>& getFontMap() const { return registeredFonts; }
  int getTextWidth(int, const char* s) const { return t.measureText(0, s, {}).width; }
};
struct MappedInputManager {
  enum class SwipeDir { None, Left, Right, Up, Down };
  SwipeDir swipe = SwipeDir::None;
  bool hasTouch() const { return true; }
  SwipeDir wasSwipe() const { return swipe; }
};
struct UITheme {
  struct Metrics {
    int buttonHintsHeight = 42;
  };
  static UITheme& getInstance() {
    static UITheme t;
    return t;
  }
  const Metrics& getMetrics() const {
    static Metrics m;
    return m;
  }
  void setCheckboxRow(fui::ListItem& i, bool b) {
    i.toggle = true;
    i.toggleChecked = b;
  }
};
#define GUI UITheme::getInstance()
namespace pixelui {
struct FontScope {
  FontScope(FontTarget&, GfxRenderer&, bool) {}
};
}  // namespace pixelui
namespace UiListActivity {
void onEnter() {}
}  // namespace UiListActivity
namespace Activity {
void onExit() {}
}  // namespace Activity
class KeyboardLayoutsActivity {
 public:
  using UiScreen = fui::FreeInkApp<24, 6>::ScreenType;
  static constexpr fui::ActionId ACTION_ROW = 1;
  FontTarget& uiTarget;
  GfxRenderer renderer;
  MappedInputManager mappedInput;
  fui::FreeInkApp<24, 6> app;
  fui::ListNav nav;
  Language languages[unsigned(Language::_COUNT)]{};
  uint8_t rowCount = 0;
  uint64_t installed = 0, workingLanguages = 0;
  uint16_t workingMask = 0;
  bool edited = false, ready = true;
  int updates = 0;
  KeyboardLayoutsActivity(FontTarget& t, fui::DeviceContext d) : uiTarget(t), renderer{t}, app(t, d) {
    app.on(
        ACTION_ROW,
        [](const fui::ActionEvent& e, void* p) { static_cast<KeyboardLayoutsActivity*>(p)->activateIndex(e.value); },
        this);
    app.setScreen([](UiScreen& s, void* p) { static_cast<KeyboardLayoutsActivity*>(p)->buildScreen(s); }, this);
  }
  void onEnter();
  void onExit();
  const char* headerTitle() const;
  bool checked(uint8_t) const;
  bool isLocked(uint8_t) const;
  bool isAvailable(uint8_t) const;
  void activateIndex(int);
  bool handleCustomInput();
  static void provideRow(void*, uint16_t, fui::ListItem&);
  void buildScreen(UiScreen&);
  void requestUpdate() { ++updates; }
  bool routingReady() const { return ready; }
};
PRODUCTION_FONT_SUPPORT
PRODUCTION_METHODS
static int checks = 0;
#define CHECK(x)                                           \
  do {                                                     \
    ++checks;                                              \
    if (!(x)) {                                            \
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                                        \
    }                                                      \
  } while (0)
static void install(const std::string& root, Language lang) {
  std::string code = LANGUAGE_CODES[unsigned(lang)];
  std::ifstream in(root + "/assets_local/language-keyboards/" + code + ".cpkbd", std::ios::binary);
  if (in)
    Storage.files["/languages/keyboards/" + code + ".cpkbd"] =
        std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}
static void image(const std::vector<uint8_t>& pixels, int w, int h, const std::string& name) {
  std::ofstream f(outputDir + "/" + name + ".pbm", std::ios::binary);
  f << "P4\n" << w << " " << h << "\n";
  for (auto value : pixels) {
    char b = char(~value);
    f.write(&b, 1);
  }
}
int main(int argc, char** argv) {
  CHECK(argc == 4);
  const std::string root = argv[1];
  outputDir = argv[2];
  int variant = std::atoi(argv[3]);
  report = std::fopen((outputDir + "/selector-glyph-issues-" + std::to_string(variant) + ".jsonl").c_str(), "w");
  for (int group = 0; group < 3; group++)
    for (int n = 0; n < 3; n++)
      sd[group * 3 + n].load(root + "/assets_local/language-fonts/Interface" +
                             (group == 1   ? "_SC"
                              : group == 2 ? "_TC"
                                           : "") +
                             "_" +
                             std::to_string(n == 0   ? 8
                                            : n == 1 ? 10
                                                     : 12) +
                             ".cpfont");
  for (currentLanguage = 0; currentLanguage < 39; currentLanguage++) {
    auto bytes = read(outputDir + "/fixtures/" + std::to_string(currentLanguage) + ".bin");
    catalogue.assign(bytes.begin(), bytes.end());
    labels.clear();
    for (size_t pos = 0; pos < catalogue.size();) {
      labels.push_back(catalogue.data() + pos);
      pos += std::strlen(catalogue.data() + pos) + 1;
    }
    I18N.setLanguage(Language(currentLanguage));
    bind(currentLanguage);
    for (int rotation = 0; rotation < 4; rotation++)
      for (bool light : {false, true})
        for (bool present : {false, true})
          for (bool fontInstalled : {true, false}) {
            if (!fontInstalled && currentLanguage != 0 && currentLanguage != 11) continue;
            bind(currentLanguage);
            registeredFonts.clear();
            registeredFonts.emplace(UI_10_FONT_ID, f10);
            if (fontInstalled)
              registeredFonts.emplace(0x4c470000 + 10, fonts.at(101));
            else {
              fallbacks.clear();
              fonts.erase(100);
              fonts.erase(101);
              fonts.erase(102);
            }
            chrome::lightHeader = light;
            Storage.files.clear();
            if (present)
              for (int n = 0; n < 39; n++) install(root, Language(n));
            SETTINGS.keyboardLayouts = 0;
            SETTINGS.keyboardLanguageLayouts = 0;
            SETTINGS.saves = 0;
            int w = rotation % 2 ? 800 : 480, h = rotation % 2 ? 480 : 800;
            std::vector<uint8_t> pixels((w + 7) / 8 * h, 255);
            FontTarget target(pixels, w, h, currentLanguage == 11 && rotation < 2);
            auto device = target.base.deviceContext();
            device.hasTouch = true;
            device.orientation = static_cast<fui::Orientation>(rotation);
            static constexpr fui::Insets areas[] = {{10, 1, 0, 1}, {1, 10, 1, 0}, {0, 1, 10, 1}, {1, 0, 1, 10}};
            device.safeArea = areas[rotation];
            KeyboardLayoutsActivity a(target, device);
            a.onEnter();
            CHECK(a.rowCount == 38);
            std::set<unsigned> seen;
            for (int n = 0; n < a.rowCount; n++) {
              CHECK(seen.insert(unsigned(a.languages[n])).second);
              fui::ListItem item;
              a.provideRow(&a, n, item);
              CHECK(item.actionValue == n);
              if (!fontInstalled &&
                  !LanguageUiFonts::supportsText(a.renderer, I18N.getLanguageName(a.languages[n]), 10))
                CHECK(std::string(item.label) == LANGUAGE_CODES[unsigned(a.languages[n])]);
              CHECK(item.enabled == (n < 10 || present));
              if (!item.enabled) CHECK(!item.toggle && item.value);
            }
            currentCase = std::to_string(variant) + "-" + std::to_string(rotation) + "-" + (light ? "light" : "dark") +
                          "-" + (present ? "installed" : "missing");
            a.app.render();
            if (currentLanguage == 11 && rotation < 2 && fontInstalled) image(pixels, w, h, "selector-" + currentCase);
            CHECK(a.nav.top == 0);
            CHECK(a.nav.inputPageRows() >= 4);
            auto originalMask = a.workingMask;
            auto originalLanguages = a.workingLanguages;
            a.ready = false;
            a.mappedInput.swipe = MappedInputManager::SwipeDir::Up;
            CHECK(a.handleCustomInput());
            a.app.render();
            CHECK(a.nav.top == 0);
            a.ready = true;
            const auto band = pixelui::geometry(device).body;
            fui::InputSnapshot press;
            press.touchPressed = true;
            press.touchX = band.x + 30;
            press.touchY = band.y + 52 + 26;
            a.app.route(press);
            a.handleCustomInput();
            fui::InputSnapshot release = press;
            release.touchPressed = false;
            release.touchReleased = true;
            // UiAppHelpers publishes a swipe release off-target, never as a tap.
            release.touchX = release.touchY = -1;
            a.app.route(release);
            CHECK(a.workingMask == originalMask && a.workingLanguages == originalLanguages);
            a.app.render();
            std::set<int> tops;
            bool last = false;
            for (int step = 0; step < 25; step++) {
              tops.insert(a.nav.top);
              CHECK(a.nav.selected >= a.nav.top && a.nav.selected < a.nav.top + a.nav.visibleRows);
              last = last || a.nav.top + a.nav.drawnRows >= a.rowCount;
              a.handleCustomInput();
              a.app.render();
            }
            CHECK(last);
            CHECK(tops.count(1) || tops.count(2) || tops.count(3));
            CHECK(a.workingMask == originalMask && a.workingLanguages == originalLanguages);
            a.mappedInput.swipe = MappedInputManager::SwipeDir::Down;
            for (int step = 0; step < 25; step++) {
              a.handleCustomInput();
              a.app.render();
            }
            CHECK(a.nav.top == 0);
            a.mappedInput.swipe = MappedInputManager::SwipeDir::Left;
            CHECK(!a.handleCustomInput());
            // Every menu choice reaches the actual production globe cycle when enabled.
            if (present) {
              for (int n = 0; n < a.rowCount; n++) {
                a.activateIndex(n);
                if (!a.checked(n)) a.activateIndex(n);
                CHECK(a.checked(n));
              }
              auto s = keyboard_layouts::startingSelection(a.installed);
              const auto configuredMask = SETTINGS.keyboardLayouts;
              const auto configuredLanguages = SETTINGS.keyboardLanguageLayouts;
              SETTINGS.keyboardLayouts = a.workingMask;
              SETTINGS.keyboardLanguageLayouts = a.workingLanguages;
              std::set<unsigned> ring;
              for (int n = 0; n < 120; n++) {
                ring.insert(unsigned(s.language));
                s = keyboard_layouts::nextSelection(s, a.installed);
              }
              CHECK(ring.size() >= 38);
              SETTINGS.keyboardLayouts = configuredMask;
              SETTINGS.keyboardLanguageLayouts = configuredLanguages;
            } else {
              for (int n = 10; n < a.rowCount; n++) {
                a.activateIndex(n);
                CHECK(a.workingLanguages == originalLanguages);
              }
            }
            a.onExit();
            CHECK(SETTINGS.saves == (present ? 1 : 0));
            if (present)
              CHECK(SETTINGS.keyboardLayouts == a.workingMask &&
                    SETTINGS.keyboardLanguageLayouts == a.workingLanguages);
            frames++;
          }
  }
  std::fclose(report);
  CHECK(issues == 0);
  std::printf(
      "Selector: 39 languages, 4 rotations, 2 headers, installed/missing keyboard files plus resident-only font cases: "
      "%d frames, %d checks, zero "
      "glyph/overflow issues.\n",
      frames, checks);
}
