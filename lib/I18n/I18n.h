#pragma once

#include <cstdint>

#include "I18nKeys.h"
/**
 * Internationalization (i18n) system for CrossPoint Reader
 */

class I18n {
 public:
  static I18n& getInstance();

  // Disable copy
  I18n(const I18n&) = delete;
  I18n& operator=(const I18n&) = delete;

  // Get localized string by ID
  const char* get(StrId id) const;

  const char* operator[](StrId id) const { return get(id); }

  Language getLanguage() const { return _language; }
  void setLanguage(Language lang);
  const char* getLanguageName(Language lang) const;
  static Language languageFromCode(const char* code);

  // Get all unique characters used in a specific language
  // Returns a sorted string of unique characters
  static const char* getCharacterSet(Language lang);

 private:
  I18n() : _language(Language::EN) {}

  Language _language;
};

// Convenience macros
#define tr(id) I18n::getInstance().get(StrId::id)
#define I18N I18n::getInstance()

// Constant UI catalogues resolve their label at use time, after language changes.
// Dynamic model text can still pass through as a borrowed C string.
class UiLabel {
 public:
  constexpr UiLabel() = default;
  constexpr UiLabel(StrId id) : id(id) {}
  constexpr UiLabel(const char* text) : fallback(text) {}
  operator const char*() const { return fallback ? fallback : id == StrId::_COUNT ? nullptr : I18N.get(id); }
  const char* get() const { return static_cast<const char*>(*this); }

 private:
  StrId id = StrId::_COUNT;
  const char* fallback = nullptr;
};
#define trLabel(id) UiLabel(StrId::id)
