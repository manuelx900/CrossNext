"""Compile the production touch path and Wi-Fi password handoff with host input/display counters."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / '.pio/host-tests/keyboard-response'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/activities/util/KeyboardEntryActivity.cpp').read_text(encoding='utf-8')
header = (root / 'src/activities/util/KeyboardEntryActivity.h').read_text(encoding='utf-8')
wifi = (root / 'src/activities/network/WifiSelectionActivity.cpp').read_text(encoding='utf-8')


def block(text, signature):
    first = text.index(signature)
    start = text.index('{', first)
    depth, end = 1, start + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[first:end]


program = r'''
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include "FreeInkUI.h"
#include "Utf8.h"
#include "src/activities/ActivityResult.h"
namespace fui = freeink::ui;
enum class InputType { Text, Password, Url };
enum class Language { EN };
struct Activity { void onEnter() {} };
namespace keyboard_layouts {
  struct Selection { int builtin = 0; Language language = Language::EN; bool external = false; };
  uint64_t scanCardLayouts() { return 0; }
  Selection startingSelection(uint64_t) { return {}; }
  uint16_t enabled() { return 1; }
}
static struct { uint64_t keyboardLanguageLayouts = 0; } SETTINGS;
static uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
namespace haptic_feedback { void touchAction(bool) {} }
struct MappedInputManager {
  enum class Button { Down };
  bool pressed = false, tapped = false, contact = false, down = false;
  bool wasScreenTouchDown(int& x, int& y) { x = y = 20; return pressed; }
  bool wasScreenTapped(int& x, int& y) { x = y = 20; return tapped; }
  bool isScreenTouchHeld(int& x, int& y) { x = y = 20; return contact; }
  bool wasPressed(Button) { return down; }
};
struct KeyboardEntryActivity : Activity {
  MappedInputManager& mappedInput;
  std::string text;
  InputType inputType;
  bool passwordVisible = false, cursorMode = false, togglePos = false, hintVisible = false;
  bool downHeld = false, downLongHandled = false, interactionsReady = true;
  size_t cursorPos = 0;
  fui::InteractionBuffer<80> interactions;
  fui::TouchHoldRouter touchRouter;
  uint64_t cardLayouts = 0;
  int layoutId = 0, selRow = 0, selCol = 0, delPressCount = 0;
  Language layoutLanguage = Language::EN;
  bool useLanguageKeyboard = false, showLangKey = false, shifted = false, symbols = false, urlPanel = false;
  bool rightHeld = false, rightLongHandled = false;
  size_t savedCursorPos = 0, rightStartCursorPos = 0;
  unsigned long hintShowTime = 0;
  struct Pinyin { bool begin(Language) { return false; } } pinyin;
  struct Layout { bool load(Language) { return false; } void candidates(Pinyin&) {} } languageKeyboard;
  struct Alternates { void reset() {} } alternates;
  struct RevealGate { void arm() {} } revealGate;
  @@THRESHOLDS@@
  unsigned paints = 0, actions = 0, longActions = 0;
  bool finished = false;
  ActivityResult result;
  KeyboardEntryActivity(int&, MappedInputManager& input, const char*, std::string initial, size_t, InputType type)
      : mappedInput(input), text(std::move(initial)), inputType(type) {}
  @@SETTER@@
  void onEnter();
  void requestUpdate() { ++paints; }
  void syncSelectionToValue(int16_t) {}
  bool activateValue(int16_t, bool held) { ++actions; longActions += held; return true; }
  void touch() { const bool revealed = true; int tx = 0, ty = 0; @@TOUCH@@ }
  void down() { @@DOWN@@ }
  std::string displayTextForCurrentState() const;
  void setResult(KeyboardResult value) { result = ActivityResult(std::move(value)); }
  void finish() { finished = true; }
  void onComplete(std::string text);
};
@@DISPLAY@@
@@COMPLETE@@
@@ENTER@@
enum class WifiSelectionState { PASSWORD_REVIEW, PASSWORD_ENTRY };
constexpr int STR_ENTER_WIFI_PASSWORD = 0;
const char* tr(int) { return "Wi-Fi password"; }
#define LOG_ERR(...) ((void)0)
template<class T, class... Args> auto makeUniqueNoThrow(Args&&... args) {
  return std::unique_ptr<T>(new (std::nothrow) T(std::forward<Args>(args)...));
}
struct WifiSelectionActivity {
  int renderer = 0;
  MappedInputManager mappedInput;
  bool passwordVisible = false;
  std::string enteredPassword;
  WifiSelectionState state = WifiSelectionState::PASSWORD_REVIEW;
  std::unique_ptr<KeyboardEntryActivity> child;
  ActivityResultHandler callback;
  void closeRouting() {}
  void requestUpdate() {}
  void startActivityForResult(std::unique_ptr<KeyboardEntryActivity> keyboard, ActivityResultHandler handler) {
    child = std::move(keyboard); callback = std::move(handler);
    child->onEnter();
  }
  void promptPasswordEntry();
};
@@WIFI@@
int main() {
  int renderer = 0;
  unsigned cases = 0;
  for (int value : {int('a'), 1001, 2001, int(fui::QWERTY_KEY_BACKSPACE)}) {
    MappedInputManager input;
    KeyboardEntryActivity keyboard(renderer,input,"", "",64,InputType::Text);
    fui::Interaction key;
    key.rect = fui::makeRect(10,10,40,40); key.action = 1; key.value = int16_t(value);
    key.inputMask = uint16_t(fui::InputTouch|fui::InputLongPress);
    keyboard.interactions.addInteraction(key);
    keyboard.touchRouter.holdMs = 350;
    for (unsigned tap = 0; tap < 5; ++tap) {
      nowMs += 20; input.pressed = input.contact = true; input.tapped = false;
      keyboard.touch(); assert(keyboard.paints == tap); assert(keyboard.actions == tap);
      nowMs += 20; input.pressed = input.contact = false; input.tapped = true;
      keyboard.touch(); assert(keyboard.actions == tap + 1); assert(keyboard.paints == tap + 1);
    }
    nowMs += 20; input.pressed = input.contact = true; input.tapped = false; keyboard.touch();
    nowMs += 1000; keyboard.touch(); assert(keyboard.actions == 6 && keyboard.longActions == 1);
    input.pressed = input.contact = false; input.tapped = true; keyboard.touch();
    assert(keyboard.actions == 6 && keyboard.paints == 6);
    ++cases;
  }
  for (bool visible : {false,true}) {
    WifiSelectionActivity wifi;
    wifi.passwordVisible = visible; wifi.enteredPassword = "Pass-ñ-كلمة-123";
    wifi.promptPasswordEntry(); assert(wifi.child && wifi.child->passwordVisible == visible);
    auto& keyboard = *wifi.child;
    keyboard.cursorMode = true; keyboard.togglePos = true; keyboard.mappedInput.down = true;
    keyboard.down(); assert(!keyboard.cursorMode && keyboard.passwordVisible == visible);
    keyboard.cursorPos = keyboard.text.size();
    const auto shown = keyboard.displayTextForCurrentState();
    assert(visible ? shown == keyboard.text : shown != keyboard.text);
    keyboard.onComplete("cafe\xCC\x81");
    assert(keyboard.finished);
    const auto& result = std::get<KeyboardResult>(keyboard.result.data);
    assert(result.text == "cafe\xCC\x81"); // passwords must not be normalized
    wifi.callback(keyboard.result); assert(wifi.passwordVisible == visible && wifi.enteredPassword == result.text);
    wifi.promptPasswordEntry(); wifi.child->passwordVisible = !visible;
    wifi.child->onComplete("new password"); wifi.callback(wifi.child->result);
    assert(wifi.passwordVisible == !visible && wifi.enteredPassword == "new password");
    wifi.promptPasswordEntry(); ActivityResult cancelled; cancelled.isCancelled = true;
    wifi.callback(cancelled); assert(wifi.passwordVisible == !visible && wifi.enteredPassword == "new password");
    ++cases;
  }
  MappedInputManager input;
  KeyboardEntryActivity normal(renderer,input,"", "Text",64,InputType::Text);
  normal.setPasswordVisible(true); assert(!normal.passwordVisible);
  normal.onComplete("cafe\xCC\x81"); assert(std::get<KeyboardResult>(normal.result.data).text == "café");
  std::printf("PASS %u input sequences: five quick taps, no press-only repaint, one action/paint per tap, hold/release, password visibility round-trip, cancel, UTF-8 and exact password bytes.\n",cases);
}
'''
for token, content in {
    'SETTER': block(header, 'void setPasswordVisible(bool visible)'),
    'THRESHOLDS': '\n'.join(line for line in header.splitlines() if 'static constexpr uint16_t TOUCH_' in line),
    'TOUCH': block(source, 'if (!cursorMode && interactionsReady && revealed)'),
    'DOWN': block(source, 'if (mappedInput.wasPressed(MappedInputManager::Button::Down))'),
    'DISPLAY': block(source, 'std::string KeyboardEntryActivity::displayTextForCurrentState() const'),
    'COMPLETE': block(source, 'void KeyboardEntryActivity::onComplete(std::string text)'),
    'ENTER': block(source, 'void KeyboardEntryActivity::onEnter()'),
    'WIFI': block(wifi, 'void WifiSelectionActivity::promptPasswordEntry()'),
}.items():
    program = program.replace('@@' + token + '@@', content)
cpp = out / 'test.cpp'
cpp.write_text(program, encoding='utf-8')
exe = out / 'test.cjs'
compiler = root / '.venv/emsdk/upstream/emscripten/em++.exe'
subprocess.run([str(compiler), '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror', '-I.', '-Ilib/Utf8',
                '-Ifreeink-sdk/libs/ui/FreeInkUI/include', str(cpp), 'lib/Utf8/Utf8.cpp',
                '-sENVIRONMENT=node', '-sEXIT_RUNTIME=1', '-o', str(exe)], cwd=root, check=True)
subprocess.run(['node', str(exe)], cwd=root, check=True)
