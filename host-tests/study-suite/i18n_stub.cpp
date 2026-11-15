#include <I18n.h>
#include <I18nStrings.h>

#include <cstddef>

// Built-in catalogues only; external packs have their own parser tests.
I18n& I18n::getInstance() {
  static I18n instance;
  return instance;
}

void I18n::setLanguage(Language language) { _language = language; }

const char* I18n::get(StrId id) const {
  const auto strings = getLanguageStrings(_language);
  const auto offset = strings.offsets[static_cast<size_t>(id)];
  return offset & 0x80000000 ? i18n_strings::STRINGS_EN_DATA + (offset & 0x7FFFFFFF) : strings.data + offset;
}
