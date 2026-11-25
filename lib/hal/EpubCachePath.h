#pragma once
#include <cstdint>
#include <string_view>

struct EpubCachePath {
  enum class Kind : uint8_t { None, Sections, Html, Events, MissingCover };
  uint64_t book = 0;
  Kind kind = Kind::None;
  bool valid() const { return kind != Kind::None; }
};
inline bool epubCacheLeaf(const char* path, uint64_t& book, std::string_view& leaf) {
  if (!path) return false;
  std::string_view value(path);
  constexpr std::string_view prefix = "/.crosspoint/epub_";
  if (!value.starts_with(prefix)) return false;
  value.remove_prefix(prefix.size());
  const auto slash = value.find('/');
  if (!slash || slash == std::string_view::npos || (slash > 1 && value[0] == '0')) return false;
  book = 0;
  for (size_t n = 0; n < slash; ++n) {
    if (value[n] < '0' || value[n] > '9') return false;
    const unsigned digit = unsigned(value[n] - '0');
    if (book > (UINT64_MAX - digit) / 10) return false;
    book = book * 10 + digit;
  }
  leaf = value.substr(slash + 1);
  return true;
}
inline bool cacheNumber(std::string_view& value) {
  size_t count = 0;
  while (count < value.size() && value[count] >= '0' && value[count] <= '9') ++count;
  if (!count || (count > 1 && value[0] == '0')) return false;
  value.remove_prefix(count);
  return true;
}
inline EpubCachePath epubDerivedPath(const char* path) {
  uint64_t book;
  std::string_view leaf;
  if (!epubCacheLeaf(path, book, leaf)) return {};
  if (leaf == "no-cover-v1.bin") return {book, EpubCachePath::Kind::MissingCover};
  auto kind = EpubCachePath::Kind::None;
  if (leaf.starts_with("sections/")) {
    leaf.remove_prefix(9);
    kind = EpubCachePath::Kind::Sections;
  } else if (leaf.starts_with("html/")) {
    leaf.remove_prefix(5);
    kind = EpubCachePath::Kind::Html;
  } else if (leaf.starts_with("events-v1/")) {
    leaf.remove_prefix(10);
    kind = EpubCachePath::Kind::Events;
  } else
    return {};
  if (kind == EpubCachePath::Kind::Html && leaf.starts_with(".tmp_")) leaf.remove_prefix(5);
  if (!cacheNumber(leaf)) return {};
  switch (kind) {
    case EpubCachePath::Kind::MissingCover:
    case EpubCachePath::Kind::None:
      return {};
    case EpubCachePath::Kind::Html:
      return leaf == ".html" ? EpubCachePath{book, kind} : EpubCachePath{};
    case EpubCachePath::Kind::Events:
      return leaf == ".evc" || leaf == ".evc.tmp" ? EpubCachePath{book, kind} : EpubCachePath{};
    case EpubCachePath::Kind::Sections:
      if (leaf.starts_with(".classic.bin"))
        leaf.remove_prefix(12);
      else if (leaf.starts_with(".bin"))
        leaf.remove_prefix(4);
      else {
        if (leaf.starts_with(".classic-p"))
          leaf.remove_prefix(10);
        else if (leaf.starts_with(".p"))
          leaf.remove_prefix(2);
        else
          return {};
        if (leaf.size() < 12) return {};
        for (size_t n = 0; n < 8; ++n)
          if (!((leaf[n] >= '0' && leaf[n] <= '9') || (leaf[n] >= 'a' && leaf[n] <= 'f'))) return {};
        leaf.remove_prefix(8);
        if (!leaf.starts_with(".bin")) return {};
        leaf.remove_prefix(4);
      }
      if (leaf.size() >= 2 && leaf[0] == '.' && leaf[1] >= '1' && leaf[1] <= '3') leaf.remove_prefix(2);
      return leaf.empty() || leaf == ".part" || leaf == ".swap" || leaf == ".tmp" || leaf == ".prep.tmp"
                 ? EpubCachePath{book, kind}
                 : EpubCachePath{};
  }
  return {};
}
inline bool sameDerivedOwner(const char* from, const char* to) {
  const auto a = epubDerivedPath(from), b = epubDerivedPath(to);
  return a.valid() && b.valid() && a.book == b.book && a.kind == b.kind;
}
inline bool isBookTextPreferences(const char* path) {
  uint64_t book;
  std::string_view leaf;
  return epubCacheLeaf(path, book, leaf) && (leaf == "text.0.bin" || leaf == "text.1.bin");
}
