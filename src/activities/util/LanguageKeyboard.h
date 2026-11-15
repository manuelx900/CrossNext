#pragma once

#include <FreeInkUI.h>
#include <HalMemory.h>
#include <I18n.h>
class PinyinInput;

// Optional current-language layout. Tables and UTF-8 strings remain on the card;
// only the keyboard activity retains the selected layout in PSRAM.
class LanguageKeyboard {
 public:
  ~LanguageKeyboard() { clear(); }
  bool load(Language language);
  void clear();
  bool available() const { return data != nullptr; }
  const freeink::ui::KeyboardLayout& layout(bool shifted) const;
  void candidates(const PinyinInput& input);

 private:
  struct Data;
  HalMemory::PsramBuffer storage;
  Data* data = nullptr;
};
