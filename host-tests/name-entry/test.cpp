#include <HalStorage.h>
#include <MappedInputManager.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../../src/activities/util/EmbeddedKeyboard.h"
#include "../../src/activities/util/KeyboardLayoutSet.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/WordleKeyboard.h"

namespace fui = freeink::ui;
struct Trace : fui::DrawTarget {
  struct Label {
    fui::Rect rect;
    std::string text;
  };
  std::vector<Label> labels;
  std::vector<fui::Rect> outlines;
  std::vector<fui::Rect> clearedRows;
  struct Row {
    fui::Rect rect;
    fui::Paint background;
  };
  std::vector<Row> restoredRows;
  fui::Rect art{};
  fui::Size measureText(fui::FontId, const char* text, fui::TextStyle) const override {
    unsigned count = 0;
    for (const unsigned char c : std::string(text))
      if ((c & 0xc0) != 0x80) ++count;
    return {int16_t(count * 11), 24};
  }
  int16_t lineHeight(fui::FontId font) const override { return font == 3 ? 16 : 24; }
  void fill(fui::Rect rect, fui::Paint paint, uint8_t, uint8_t) override {
    if (rect.height == 1) restoredRows.push_back({rect, paint});
    if (rect.height == 1 && paint.kind == fui::PaintKind::Solid && paint.color == fui::Color::White)
      clearedRows.push_back(rect);
  }
  void stroke(fui::Rect rect, fui::Paint, uint8_t, uint8_t, uint8_t) override { outlines.push_back(rect); }
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override {}
  void text(fui::Rect rect, const char* text, fui::TextStyle) override {
    if (text) labels.push_back({rect, text});
  }
  fui::Rect find(const char* value) const {
    for (const auto& label : labels)
      if (label.text == value) return label.rect;
    return {};
  }
};

#include <WordleOwnerTest.generated.h>

#include "AlternatesTest.h"
#include "WordleCasingTest.h"

static void builtInFeedback() {
  using wordlekeys::Tone;
  for (bool spanish : {false, true})
    for (bool wide : {false, true}) {
      Trace target;
      fui::DeviceContext device;
      device.width = wide ? 800 : 480;
      device.height = wide ? 480 : 800;
      const fui::InputSnapshot input;
      toybox::Interactions interactions;
      toybox::Frame frame(target, device, input, interactions);
      toybox::Screen screen(frame);
      const auto controls = fui::makeRect(12, 240, device.width - 24, 208);
      const auto keys = wordlekeys::keyboardLayout(controls, controls.y, spanish);
      Tone tones[27]{};
      tones['C' - 'A'] = Tone::Absent;
      tones['A' - 'A'] = Tone::Present;
      tones['T' - 'A'] = Tone::Correct;
      if (spanish) tones[26] = Tone::Absent;
      wordlekeys::draw(screen, controls, keys, 101, tones);
      const auto c = wordlekeys::keyRect(keys, 2, 2);
      const auto expected = c.height / 2 * (spanish ? 2 : 1);
      assert(int(target.clearedRows.size()) == expected);
      assert(wordlekeys::keyAt(keys, c.x + c.width / 2, c.y + c.height / 2) == 'C');
      if (spanish) {
        const auto enye = wordlekeys::keyRect(keys, 2, 7);
        assert(wordlekeys::keyAt(keys, enye.x + enye.width / 2, enye.y + enye.height / 2) == '[');
      }
    }
}

static void embeddedFeedback(Language language, const fui::DeviceContext& device) {
  using wordlekeys::Tone;
  EmbeddedKeyboard keyboard;
  assert(keyboard.begin(language));
  keyboard.wordGame(5);
  const auto box = fui::makeRect(12, 200, device.width - 24, device.height - 212);
  std::vector<fui::Rect> normalOutlines;
  for (Tone tone : {Tone::Normal, Tone::Present, Tone::Correct, Tone::Absent}) {
    Trace target;
    keyboard.drawGame(target, device, box, &tone,
                      [](const void* context, const char*) { return *static_cast<const Tone*>(context); });
    paintclock::notePainted();
    keyboard.didPaint();
    if (tone == Tone::Normal) normalOutlines = target.outlines;
    assert(target.outlines.size() == normalOutlines.size());
    for (size_t i = 0; i < target.outlines.size(); ++i) {
      const auto a = target.outlines[i], b = normalOutlines[i];
      assert(a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height);
    }
    if (tone != Tone::Absent) {
      assert(target.clearedRows.empty());
      continue;
    }
    assert(!target.clearedRows.empty());
    const auto stripe = target.clearedRows.front();
    for (const auto& rect : target.outlines) {
      if (rect.x != stripe.x || rect.width != stripe.width || !rect.contains(stripe.x, stripe.y)) continue;
      MappedInputManager input;
      input.tap = true;
      input.x = rect.x + rect.width / 2;
      input.y = rect.y + rect.height / 2;
      const auto event = keyboard.poll(input);
      if (event != EmbeddedKeyboard::Event::Changed)
        std::printf("Faded key: %s %dx%d event %d at %d,%d\n", LANGUAGE_CODES[unsigned(language)], device.width,
                    device.height, int(event), input.x, input.y);
      assert(event == EmbeddedKeyboard::Event::Changed);
      return;
    }
    assert(false && "faded key has no touch target");
  }
}

static void alternateFeedback() {
  using wordlekeys::Tone;
  for (bool wide : {false, true})
    for (Tone primary : {Tone::Normal, Tone::Present, Tone::Correct}) {
      testI18n.language = Language::FR;
      EmbeddedKeyboard keyboard;
      assert(keyboard.begin(Language::FR));
      keyboard.wordGame(4);
      Trace target;
      fui::DeviceContext device;
      device.width = wide ? 800 : 480;
      device.height = wide ? 480 : 800;
      keyboard.drawGame(target, device, fui::makeRect(12, 200, device.width - 24, device.height - 212), &primary,
                        [](const void* context, const char* value) {
                          if (std::string(value) == "é") return Tone::Absent;
                          return std::string(value) == "e" ? *static_cast<const Tone*>(context) : Tone::Normal;
                        });
      const auto expected = primary == Tone::Correct   ? fui::Paint::solid(fui::Color::Black)
                            : primary == Tone::Present ? fui::Paint::dither(fui::Color::LightGray)
                                                       : fui::Paint::solid(fui::Color::White);
      assert(target.restoredRows.size() == 8);
      for (const auto& row : target.restoredRows) {
        assert(row.rect.width == 16);
        assert(row.background.kind == expected.kind && row.background.color == expected.color);
      }
    }
}

static void install(const std::string& source, const std::string& destination);
static void wordleDisplayCasing() {
  struct Case {
    Language language;
    const char* typed;
    const char* shown;
  };
  for (const auto item : {Case{Language::FR, "a", "A"}, Case{Language::DE, "q", "Q"}, Case{Language::UK, "й", "Й"},
                          Case{Language::RU, "й", "Й"}, Case{Language::TR, "i", "İ"}, Case{Language::AZ, "i", "İ"}}) {
    testI18n.language = item.language;
    install("assets_local/language-keyboards/" + std::string(LANGUAGE_CODES[unsigned(item.language)]) + ".cpkbd",
            "/languages/keyboards/" + std::string(LANGUAGE_CODES[unsigned(item.language)]) + ".cpkbd");
    EmbeddedKeyboard keyboard;
    assert(keyboard.begin(item.language));
    keyboard.wordGame(5);
    Trace target;
    fui::DeviceContext device;
    device.width = 480;
    device.height = 800;
    keyboard.drawGame(target, device, fui::makeRect(12, 580, 456, 208), nullptr, nullptr);
    const auto key = target.find(item.shown);
    assert(!key.empty());
    assert(target.find(item.typed).empty());
    paintclock::notePainted();
    keyboard.didPaint();
    MappedInputManager input;
    input.tap = true;
    input.x = key.x + key.width / 2;
    input.y = key.y + key.height / 2;
    assert(keyboard.poll(input) == EmbeddedKeyboard::Event::Changed);
    assert(std::string(keyboard.text()) == item.typed);
  }
  std::puts(
      "Wordle labels: native uppercase presentation preserves original typed values, including Turkish/Azeri dotted I");
}

static void install(const std::string& source, const std::string& destination) {
  std::ifstream file(source, std::ios::binary);
  if (file) testStorage.files[destination] = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), {});
}

static void canonicalBuiltins() {
  for (auto language : {Language::FR, Language::BE}) {
    const auto code = std::string(LANGUAGE_CODES[unsigned(language)]);
    install("assets_local/language-keyboards/" + code + ".cpkbd", "/languages/keyboards/" + code + ".cpkbd");
    testI18n.language = language;
    assert(!(keyboard_layouts::scanCardLayouts() & keyboard_layouts::languageBit(language)));
    const auto selection = keyboard_layouts::startingSelection(~uint64_t{0});
    assert(selection.language == language && !selection.external);
  }
  std::puts("Built-in language selection ignores stale SD overrides without changing stored mask bits");
}

int main() {
  canonicalBuiltins();
  alternateChooser();
  embeddedAccents();
  embeddedUkrainianApostrophe();
  nameUnitDeletion();
  wordleOwner();
  builtInFeedback();
  install("assets_local/language-keyboards/pinyin-sc.bin", "/languages/keyboards/pinyin-sc.bin");
  install("assets_local/language-keyboards/pinyin-tc.bin", "/languages/keyboards/pinyin-tc.bin");
  unsigned scenarios = 0;
  for (unsigned language = 0; language < unsigned(Language::_COUNT); ++language) {
    const auto code = std::string(LANGUAGE_CODES[language]);
    install("assets_local/language-keyboards/" + code + ".cpkbd", "/languages/keyboards/" + code + ".cpkbd");
    testI18n.language = Language(language);
    for (bool landscape : {false, true}) {
      EmbeddedKeyboard keyboard;
      assert(keyboard.begin(Language(language), 24, 12, true));
      fui::DeviceContext device;
      device.width = landscape ? 800 : 480;
      device.height = landscape ? 480 : 800;
      device.hasTouch = true;
      device.safeArea = {10, 0, 0, 0};
      Trace target;
      EmbeddedKeyboard::NamePresentation name;
      name.prompt = "Give it a name";
      name.subtitle = "A name for your companion.";
      name.cardTitle = "Forest egg";
      name.cardText = "Your journey starts together.";
      name.fieldLabel = "Name";
      name.placeholder = "Write here";
      name.confirmLabel = "Confirm name";
      name.artContext = &target;
      name.drawArt = [](void* ctx, fui::DrawTarget&, fui::Rect rect, uint32_t, bool) {
        static_cast<Trace*>(ctx)->art = rect;
        return true;
      };
      const auto paint = [&] {
        target.labels.clear();
        target.outlines.clear();
        const fui::InputSnapshot empty;
        toybox::Interactions outer;
        toybox::Frame frame(target, device, empty, outer);
        toybox::Screen screen(frame);
        keyboard.drawName(screen, "Pet", name);
        paintclock::notePainted();
        keyboard.didPaint();
      };
      const auto tap = [&](fui::Rect rect) {
        assert(!rect.empty());
        MappedInputManager input;
        input.tap = true;
        input.x = rect.x + rect.width / 2;
        input.y = rect.y + rect.height / 2;
        return keyboard.poll(input);
      };
      paint();
      assert(!target.find("0 / 12").empty());
      assert(!target.find(name.placeholder).empty());
      assert(target.art.height >= 70 && target.art.width >= 70);
      for (const auto& rect : target.outlines) {
        assert(rect.width > 0 && rect.height > 0);
        assert(rect.x >= 0 && rect.y >= 0 && rect.right() <= device.width && rect.bottom() <= device.height);
      }
      assert(tap(target.find(name.confirmLabel)) != EmbeddedKeyboard::Event::Submit);
      assert(keyboard.begin(Language(language), 24, 12, true, "Luna"));
      paint();
      assert(!target.find("4 / 12").empty());
      assert(tap(target.find(name.confirmLabel)) == EmbeddedKeyboard::Event::Submit);
      assert(std::string(keyboard.text()) == "Luna");
      assert(keyboard.begin(Language(language), 24, 12, true, "José"));
      paint();
      assert(!target.find("4 / 12").empty());
      if (Language(language) == Language::EN || Language(language) == Language::ES) {
        assert(tap(target.find("a")) == EmbeddedKeyboard::Event::Changed);
        paint();
        assert(std::string(keyboard.text()) == "Joséa");
        assert(!target.find("5 / 12").empty());
        assert(keyboard.begin(Language(language), 24, 12, true, "ABCDEFGHIJKL"));
        paint();
        assert(tap(target.find("a")) != EmbeddedKeyboard::Event::Changed);
        assert(std::string(keyboard.text()) == "ABCDEFGHIJKL");
      }
      name.enabled = false;
      paint();
      assert(tap(target.find(name.confirmLabel)) != EmbeddedKeyboard::Event::Submit);
      embeddedFeedback(Language(language), device);
      ++scenarios;
    }
  }
  std::printf("Naming and Wordle keyboard feedback: %u language/orientation scenarios passed\n", scenarios);
  alternateFeedback();
  std::puts("Mixed main/alternate Wordle tones: 6 cases passed");
  wordleDisplayCasing();
  wordleCaseCoverage();
}
