#include "DictionaryLookupWord.h"

#include <cstring>

namespace dictionaryLookup {
namespace {
size_t fragmentLength(const Fragment& fragment) {
  const size_t length = strlen(fragment.text);
  return length - (fragment.insertedHyphen && length && fragment.text[length - 1] == '-');
}

bool lexicalFragment(const Fragment& fragment) {
  if (!fragment.text || !fragment.text[0]) return false;
  bool numberedMarker = fragment.paragraphStart && fragment.text[0] >= '0' && fragment.text[0] <= '9' &&
                        fragment.text[strlen(fragment.text) - 1] == '.';
  for (const auto* p = reinterpret_cast<const uint8_t*>(fragment.text); *p; ++p) {
    if (*p <= ' ' || (*p == 0xc2 && p[1] == 0xa0) || (*p == 0xe2 && p[1] == 0x80 && p[2] == 0xaf)) return false;
    if (!(*p >= '0' && *p <= '9') && !(*p == '.' && !p[1])) numberedMarker = false;
  }
  return !numberedMarker;
}
}  // namespace

std::string joinWord(Fragment hit, const void* context, Neighbor neighbor) {
  if (!lexicalFragment(hit)) return {};
  char word[MAX_WORD_BYTES];
  size_t length = fragmentLength(hit);
  if (length >= sizeof(word)) return {};
  memcpy(word, hit.text, length);
  Fragment edge = hit;
  while (!edge.paragraphStart && edge.start != UINT32_MAX) {
    edge = neighbor(context, edge.start, true);
    if (!lexicalFragment(edge) || edge.end <= edge.start) break;
    const size_t bytes = fragmentLength(edge);
    if (bytes + length >= sizeof(word)) return {};
    memmove(word + bytes, word, length);
    memcpy(word, edge.text, bytes);
    length += bytes;
  }
  while (hit.end != UINT32_MAX) {
    hit = neighbor(context, hit.end, false);
    if (!lexicalFragment(hit) || hit.paragraphStart || hit.end <= hit.start) break;
    const size_t bytes = fragmentLength(hit);
    if (bytes + length >= sizeof(word)) return {};
    memcpy(word + length, hit.text, bytes);
    length += bytes;
  }
  return std::string(word, length);
}

std::string plainWord(const std::string_view definition, size_t offset) {
  if (offset >= definition.size()) return {};
  size_t end = offset;
  while (offset && static_cast<uint8_t>(definition[offset - 1]) > ' ') --offset;
  while (end < definition.size() && static_cast<uint8_t>(definition[end]) > ' ') ++end;
  if (end - offset >= MAX_WORD_BYTES) return {};
  return std::string(definition.substr(offset, end - offset));
}

}  // namespace dictionaryLookup
