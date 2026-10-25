#include "DictionaryDefaults.h"

#include <I18n.h>

#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "util/DictionaryRegistry.h"

namespace dictionarydefaults {
namespace {

bool select(char* selected, const char* preferred, const char* legacy = nullptr) {
  // The registry validates the index/data pair through HalStorage. This bounded,
  // cold-path string is released before the next selection is checked.
  std::string base;
  const char* name = "";
  if (DictionaryRegistry::resolveBasePath(preferred, base)) {
    name = preferred;
  } else if (legacy && DictionaryRegistry::resolveBasePath(legacy, base)) {
    name = legacy;
  }
  if (strcmp(selected, name) == 0) return false;
  strncpy(selected, name, 31);
  selected[31] = '\0';
  return true;
}

}  // namespace

bool apply(CrossPointSettings& settings) {
  const char* definition;
  const char* translation;
  const char* legacy = nullptr;
  switch (static_cast<Language>(settings.language)) {
    case Language::ES:
      definition = "Spanish Wiktionary";
      legacy = "Espanol Wikcionario";
      translation = "WikDict EN-ES";
      break;
    case Language::EN:
      definition = "English Simple";
      translation = "WikDict ES-EN";
      break;
    default:
      // Other interface languages have no bundled monolingual default.
      return false;
  }
  bool changed = false;
  if (settings.dictionaryAuto && !settings.dictionaryName[0])
    changed |= select(settings.dictionaryName, definition, legacy);
  if (settings.translationDictionaryAuto && !settings.translationDictionaryName[0])
    changed |= select(settings.translationDictionaryName, translation);
  return changed;
}

}  // namespace dictionarydefaults
