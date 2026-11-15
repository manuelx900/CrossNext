#pragma once
#include <FreeInkUI.h>
#include <HalMemory.h>
#include <I18n.h>

#include "../../apps_local/ui/WordleKeyTone.h"

class MappedInputManager;
namespace toybox {
class Screen;
}

// Activity-owned input surface: no child activity, additional framebuffer or task.
class EmbeddedKeyboard {
 public:
  enum class Event { None, Consumed, Changed, Submit, Cancel };
  struct NamePresentation {
    const char* prompt = nullptr;
    const char* subtitle = nullptr;
    const char* cardTitle = nullptr;
    const char* cardText = nullptr;
    const char* fieldLabel = nullptr;
    const char* placeholder = nullptr;
    const char* confirmLabel = nullptr;
    void* artContext = nullptr;
    bool (*drawArt)(void*, freeink::ui::DrawTarget&, freeink::ui::Rect, uint32_t, bool) = nullptr;
    uint32_t artId = 0;
    bool enabled = true;
  };
  ~EmbeddedKeyboard() { close(); }
  bool begin(Language language, unsigned maxBytes = 96, unsigned maxCharacters = 0, bool name = false,
             const char* initial = "");
  void close();
  bool available() const { return data != nullptr; }
  const char* text() const;
  void clear();
  void wordGame(unsigned units);
  void capture(const MappedInputManager& input);
  Event poll(const MappedInputManager& input, bool captureNow = true);
  void didPaint();
  void draw(freeink::ui::DrawTarget& target, const freeink::ui::DeviceContext& device, freeink::ui::Rect box);
  void drawName(toybox::Screen& screen, const char* title, const NamePresentation& presentation);
  void drawGame(freeink::ui::DrawTarget& target, const freeink::ui::DeviceContext& device, freeink::ui::Rect box,
                const void* context, wordlekeys::Tone (*keyTone)(const void*, const char*));
  unsigned rows() const;
  int preferredHeight() const;
  void unit(unsigned column, char out[16]) const;

 private:
  struct Data;
  HalMemory::PsramBuffer storage;
  Data* data = nullptr;
  Event activate(int value, bool held);
  Event edit(int value, bool held);
  bool insert(const char* value);
  const freeink::ui::KeyboardLayout& layout() const;
  const freeink::ui::KeyboardLayout& gameLayout() const;
  const freeink::ui::KeyboardLayout& nameLayout() const;
  void drawKeys(freeink::ui::DrawTarget&, const freeink::ui::DeviceContext&, freeink::ui::Rect,
                const NamePresentation* = nullptr, freeink::ui::Rect submit = {});
};
