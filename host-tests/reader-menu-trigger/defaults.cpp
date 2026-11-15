#include <I18n.h>

#include <cassert>
#include <cstdio>
#include <cstring>
I18n& I18n::getInstance() {
  static I18n instance;
  return instance;
}
const char* I18n::get(StrId id) const {
  if (id == StrId::STR_TEXT_DEFAULTS_HELP) return "help";
  if (id == StrId::STR_SDK_SETTINGS_SAVE_FAILED) return "failed";
  return "label";
}
struct Settings {
  int perBookTextSettings = 1;
} settings;
#define SETTINGS settings
namespace readertext {
enum { CopyDefaults, DefaultsInfo };
}
namespace booktext {
bool succeeds = true;
bool setPerBook(bool enabled) {
  if (!succeeds) return false;
  settings.perBookTextSettings = enabled;
  return true;
}
}  // namespace booktext
struct TextSettingsActivity {
  struct Popup {
    const char* body = nullptr;
    template <class Callback>
    void showMessage(const char*, const char* message, const char**, int count, int, Callback, bool = false) {
      assert(count == 1);
      body = message;
    }
  } optionPopup_;
  void choose(int action) {
    switch (action) {
#include "defaults-production.inc"
    }
  }
};
int main() {
  TextSettingsActivity activity;
  activity.choose(readertext::CopyDefaults);
  assert(!settings.perBookTextSettings && !activity.optionPopup_.body);
  activity.choose(readertext::DefaultsInfo);
  assert(std::strcmp(activity.optionPopup_.body, "help") == 0);
  activity.optionPopup_.body = nullptr;
  activity.choose(readertext::CopyDefaults);
  assert(settings.perBookTextSettings && !activity.optionPopup_.body);
  activity.choose(readertext::DefaultsInfo);
  assert(std::strcmp(activity.optionPopup_.body, "help") == 0);
  booktext::succeeds = false;
  activity.choose(readertext::CopyDefaults);
  assert(settings.perBookTextSettings && std::strcmp(activity.optionPopup_.body, "failed") == 0);
  std::puts("Production default-setting toggle: shared/per-book explanations and failed-save preservation passed");
}
