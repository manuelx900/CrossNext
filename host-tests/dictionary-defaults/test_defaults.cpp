#include <CrossPointSettings.h>
#include <I18n.h>

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include "apps_local/words/DictionaryDefaults.h"
#include "util/DictionaryRegistry.h"

static std::vector<std::string> installed;
namespace DictionaryRegistry {
bool resolveBasePath(const char* name, std::string& base) {
  for (const auto& folder : installed) {
    if (folder == name) {
      base = "/dictionaries/" + folder + "/stardict";
      return true;
    }
  }
  return false;
}
}  // namespace DictionaryRegistry

int main() {
  CrossPointSettings s;
  assert(!dictionarydefaults::apply(s));  // No card/resources.
  installed = {"English Simple", "Spanish Wiktionary", "WikDict EN-ES", "WikDict ES-EN"};
  assert(dictionarydefaults::apply(s));  // Installed after initial boot.
  assert(strcmp(s.dictionaryName, "English Simple") == 0);
  assert(strcmp(s.translationDictionaryName, "WikDict ES-EN") == 0);
  assert(!dictionarydefaults::apply(s));  // No redundant save on next boot.
  s.language = static_cast<uint8_t>(Language::ES);
  assert(!dictionarydefaults::apply(s));  // Assigned names survive a language change.
  assert(strcmp(s.dictionaryName, "English Simple") == 0);
  assert(strcmp(s.translationDictionaryName, "WikDict ES-EN") == 0);

  // A saved name wins even when an older version left its automatic flag set.
  strcpy(s.dictionaryName, "My dictionary");
  strcpy(s.translationDictionaryName, "My translation");
  s.language = static_cast<uint8_t>(Language::EN);
  assert(!dictionarydefaults::apply(s));
  assert(strcmp(s.dictionaryName, "My dictionary") == 0);
  assert(strcmp(s.translationDictionaryName, "My translation") == 0);
  s.translationDictionaryAuto = false;
  s.translationDictionaryName[0] = '\0';  // Explicit None is retained.
  s.language = static_cast<uint8_t>(Language::ES);
  assert(!dictionarydefaults::apply(s));
  assert(s.translationDictionaryName[0] == '\0');
  assert(strcmp(s.dictionaryName, "My dictionary") == 0);

  s.dictionaryName[0] = '\0';
  s.dictionaryAuto = s.translationDictionaryAuto = true;
  installed = {"Espanol Wikcionario", "WikDict EN-ES"};
  assert(dictionarydefaults::apply(s));  // Legacy SD spelling remains usable.
  assert(strcmp(s.dictionaryName, "Espanol Wikcionario") == 0);
  installed.clear();
  assert(!dictionarydefaults::apply(s));  // A removed SD folder does not erase the saved choice.
  assert(strcmp(s.dictionaryName, "Espanol Wikcionario") == 0);
  assert(strcmp(s.translationDictionaryName, "WikDict EN-ES") == 0);
  s.dictionaryName[0] = s.translationDictionaryName[0] = '\0';
  installed = {"WikDict EN-ES"};
  assert(dictionarydefaults::apply(s));  // Partial resource installation.
  assert(!s.dictionaryName[0]);
  assert(strcmp(s.translationDictionaryName, "WikDict EN-ES") == 0);
  installed.push_back("Spanish Wiktionary");
  assert(dictionarydefaults::apply(s));  // Fill only the role that was still unassigned.
  assert(strcmp(s.dictionaryName, "Spanish Wiktionary") == 0);
  assert(strcmp(s.translationDictionaryName, "WikDict EN-ES") == 0);
  assert(!dictionarydefaults::apply(s));
  s.language = static_cast<uint8_t>(Language::FR);
  assert(!dictionarydefaults::apply(s));
  return 0;
}
