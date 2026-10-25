#include "DictionaryHtml.h"

#include <cctype>
#include <cstring>
#include <string_view>

namespace vocabulary {
namespace {
struct Tag {
  size_t end = 0;
  std::string_view name;
  bool closing = false;
};

bool equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t n = 0; n < a.size(); ++n)
    if (std::tolower(static_cast<unsigned char>(a[n])) != std::tolower(static_cast<unsigned char>(b[n]))) return false;
  return true;
}

Tag tagAt(const std::string& html, size_t at) {
  Tag tag;
  if (at >= html.size() || html[at] != '<') return tag;
  size_t n = at + 1;
  if (n < html.size() && html[n] == '/') {
    tag.closing = true;
    ++n;
  }
  const size_t start = n;
  while (n < html.size() && std::isalnum(static_cast<unsigned char>(html[n]))) ++n;
  if (n == start) return tag;
  tag.name = std::string_view(html.data() + start, n - start);
  char quote = 0;
  for (; n < html.size(); ++n) {
    const char c = html[n];
    if (quote) {
      if (c == quote) quote = 0;
    } else if (c == '\'' || c == '"') {
      quote = c;
    } else if (c == '>') {
      tag.end = n + 1;
      break;
    }
  }
  return tag;
}

// Do not remove an incomplete element: the normal fallback can still show it.
size_t elementEnd(const std::string& html, const Tag& open) {
  unsigned depth = 1;
  for (size_t n = open.end; n < html.size();) {
    const size_t at = html.find('<', n);
    if (at == std::string::npos) break;
    const Tag tag = tagAt(html, at);
    if (!tag.end) {
      n = at + 1;
      continue;
    }
    if (equal(tag.name, open.name)) {
      if (tag.closing) {
        if (--depth == 0) return tag.end;
      } else if (html[tag.end - 2] != '/') {
        ++depth;
      }
    }
    n = tag.end;
  }
  return 0;
}

bool phonetics(std::string_view body) {
  return body.find("class=\"IPA\"") != body.npos || body.find("class='IPA'") != body.npos ||
         body.find("SAMPA pronunciation") != body.npos;
}

bool pronunciationRow(const std::string& html, const Tag& item, size_t end) {
  const auto body = std::string_view(html).substr(item.end, end - item.end);
  if (!phonetics(body)) return false;
  // Mixed metadata lists also contain syllabification. Remove only the rows
  // explicitly labelled as phonetic transcriptions, never a meaning/example.
  if (body.find("Alfabeto Fonético Internacional") == body.npos && body.find("IPA:") == body.npos &&
      body.find("SAMPA pronunciation") == body.npos)
    return false;
  for (size_t p = item.end; p < end;) {
    const size_t at = html.find('<', p);
    if (at == std::string::npos || at >= end) break;
    const Tag nested = tagAt(html, at);
    if (equal(nested.name, "ul") || equal(nested.name, "ol") || equal(nested.name, "dl")) return false;
    p = nested.end ? nested.end : at + 1;
  }
  return true;
}

bool pronunciationList(const std::string& html, const Tag& list, size_t end) {
  bool found = false;
  for (size_t n = list.end; n < end;) {
    const size_t at = html.find('<', n);
    if (at == std::string::npos || at >= end) break;
    const Tag item = tagAt(html, at);
    n = item.end ? item.end : at + 1;
    if (!item.closing && equal(item.name, "li")) {
      const size_t itemEnd = elementEnd(html, item);
      if (!itemEnd || itemEnd > end) return false;
      const auto body = std::string_view(html).substr(item.end, itemEnd - item.end);
      if (!phonetics(body)) return false;
      // A pronunciation annotation inside a semantic/example list is content,
      // not a pronunciation-only row. Preserve the containing list.
      for (size_t p = item.end; p < itemEnd;) {
        const size_t inner = html.find('<', p);
        if (inner == std::string::npos || inner >= itemEnd) break;
        const Tag nested = tagAt(html, inner);
        if (equal(nested.name, "ul") || equal(nested.name, "ol") || equal(nested.name, "dl")) return false;
        p = nested.end ? nested.end : inner + 1;
      }
      found = true;
      n = itemEnd;
    }
  }
  return found;
}
}  // namespace

void prepareDictionaryHtml(std::string& html) {
  size_t read = 0, write = 0;
  while (read < html.size()) {
    const Tag tag = tagAt(html, read);
    if (tag.end) {
      if (!tag.closing) {
        if (equal(tag.name, "li")) {
          const size_t end = elementEnd(html, tag);
          if (end && pronunciationRow(html, tag, end)) {
            read = end;
            continue;
          }
        }
        if (equal(tag.name, "ul") || equal(tag.name, "script") || equal(tag.name, "style")) {
          const size_t end = elementEnd(html, tag);
          if (end && (equal(tag.name, "script") || equal(tag.name, "style") || pronunciationList(html, tag, end))) {
            read = end;
            continue;
          }
        }
        // WikDict starts an entry with slash-delimited grey-font transcriptions.
        // Keep the containing div and the grammar/meanings after its first br.
        size_t first = tag.end;
        while (first < html.size() && std::isspace(static_cast<unsigned char>(html[first]))) ++first;
        if (equal(tag.name, "div") && first < html.size() && html[first] == '/') {
          const size_t br = html.find("<br", first);
          const size_t end = elementEnd(html, tag);
          if (br != std::string::npos && end && br < end) {
            const auto pronunciation = std::string_view(html).substr(first, br - first);
            const Tag lineBreak = tagAt(html, br);
            if (lineBreak.end && pronunciation.find("<font") != pronunciation.npos &&
                (pronunciation.find("\"gray\"") != pronunciation.npos ||
                 pronunciation.find("'gray'") != pronunciation.npos)) {
              html[write++] = '<';
              html[write++] = 'p';
              html[write++] = '>';
              read = lineBreak.end;
              continue;
            }
          }
        }
      }
      if (equal(tag.name, "div") || equal(tag.name, "dd") || equal(tag.name, "dt")) {
        html[write++] = '<';
        if (tag.closing) html[write++] = '/';
        html[write++] = 'p';
        html[write++] = '>';
      } else {
        const size_t length = tag.end - read;
        std::memmove(html.data() + write, html.data() + read, length);
        write += length;
      }
      read = tag.end;
    } else {
      html[write++] = html[read++];
    }
  }
  html.resize(write);
}
}  // namespace vocabulary
