#include "I18n.h"

#include <cstddef>
#include <cstring>

#include "I18nPackKeys.h"
#include "I18nStrings.h"
#include "LanguagePack.h"

using namespace i18n_strings;
languagepack::Pack I18n::externalPack;

I18n& I18n::getInstance() {
  static I18n instance;
  return instance;
}

const char* I18n::get(StrId id) const {
  const auto index = static_cast<size_t>(id);
  if (index >= static_cast<size_t>(StrId::_COUNT)) {
    return "???";
  }

  if (externalActive) {
    const char* text = externalPack.get(index);
    return text ? text : STRINGS_EN_DATA + OFFSETS_EN[index];
  }

  // Use generated helper function - no hardcoded switch needed!
  const LangStrings lang = getLanguageStrings(_language);

  // If bit 31 of the offset is set, apply the offset to the English lookup table
  const uint32_t off = lang.offsets[index];
  if (off & 0x80000000) return STRINGS_EN_DATA + (off & 0x7FFFFFFF);
  return lang.data + off;
}

void I18n::setLanguage(Language lang) {
  if (lang >= Language::_COUNT) {
    return;
  }
  _language = lang;
  externalActive = false;
}

bool I18n::loadLanguagePack(const char* filename) {
  if (externalPack.loaded()) return false;
  if (!externalPack.load(filename)) {
    _language = Language::EN;
    externalActive = false;
    return false;
  }
  _language = languageFromCode(externalPack.info().code);
  externalActive = true;
  return true;
}

const char* I18n::getLanguageName() const {
  return externalActive ? externalPack.info().name : getLanguageName(_language);
}

const char* I18n::getLanguageName(Language lang) const {
  const auto index = static_cast<size_t>(lang);
  if (index >= static_cast<size_t>(Language::_COUNT)) {
    return "???";
  }
  return LANGUAGE_NAMES[index];
}

Language I18n::languageFromCode(const char* code) {
  if (!code) return Language::EN;
  for (uint8_t i = 0; i < getLanguageCount(); i++) {
    if (strcasecmp(code, LANGUAGE_CODES[i]) == 0 || strcasecmp(code, LANGUAGE_TAGS[i]) == 0)
      return static_cast<Language>(i);
  }
  const size_t primaryLength = strcspn(code, "-");
  if (code[primaryLength] == '-') {
    for (uint8_t i = 0; i < getLanguageCount(); ++i) {
      if ((strlen(LANGUAGE_TAGS[i]) == primaryLength && strncasecmp(code, LANGUAGE_TAGS[i], primaryLength) == 0) ||
          (strlen(LANGUAGE_CODES[i]) == primaryLength && strncasecmp(code, LANGUAGE_CODES[i], primaryLength) == 0))
        return static_cast<Language>(i);
    }
  }
  return Language::EN;
}

// Generate character set for a specific language
const char* I18n::getCharacterSet(Language lang) {
  const auto langIndex = static_cast<size_t>(lang);
  if (langIndex >= static_cast<size_t>(Language::_COUNT)) {
    lang = Language::EN;  // Fallback to first language
  }

  return CHARACTER_SETS[static_cast<size_t>(lang)];
}
