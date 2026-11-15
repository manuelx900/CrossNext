#include "LanguagePack.h"

// PET screen checks use built-in translations; external packs have their own suite.
bool languagepack::Pack::load(const char*) { return false; }
const char* languagepack::Pack::get(size_t) const { return nullptr; }
