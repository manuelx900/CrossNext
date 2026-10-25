#pragma once
#include <cstddef>
namespace flashtext {
// Bounded plain text for the e-ink card. False means unsupported/oversized;
// callers keep the source and report that card rather than clipping UTF-8.
bool unescape(const char*, char*, size_t);
bool plain(const char*, char*, size_t);
bool cloze(const char*, unsigned, bool, char*, size_t);
}  // namespace flashtext
