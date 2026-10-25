#include "Vocabulary.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace vocabulary {
namespace {
unsigned char lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
bool hasText(const char* s) {
  for (; *s; ++s)
    if (static_cast<unsigned char>(*s) > 32) return true;
  return false;
}
struct Hash {
  uint64_t a = 1469598103934665603ull, b = 7809847782465536322ull;
  void add(const char* s) {
    // Field separator participates in both hashes; the two strings cannot be
    // confused by moving a character across their boundary.
    for (; *s; ++s) {
      const auto c = lower(static_cast<unsigned char>(*s));
      a = (a ^ c) * 1099511628211ull;
      b = (b ^ c) * 14029467366897019727ull;
    }
    a = (a ^ 0xff) * 1099511628211ull;
    b = (b ^ 0xff) * 14029467366897019727ull;
  }
  void finish(char* out) {
    std::snprintf(out, kIdBytes, "%016llx%016llx", static_cast<unsigned long long>(a),
                  static_cast<unsigned long long>(b));
  }
};
struct Codec {
  uint8_t* out = nullptr;
  const uint8_t* in = nullptr;
  size_t size, at = 0;
  uint32_t version = 1;
  bool ok = true;
  void bytes(void* p, size_t n) {
    if (n > size - std::min(at, size)) {
      ok = false;
      return;
    }
    if (out)
      std::memcpy(out + at, p, n);
    else
      std::memcpy(p, in + at, n);
    at += n;
  }
  void number(uint32_t& n) {
    if (out) {
      for (int i = 0; i < 4; ++i) {
        uint8_t b = n >> (i * 8);
        bytes(&b, 1);
      }
    } else {
      n = 0;
      for (int i = 0; i < 4; ++i) {
        uint8_t b = 0;
        bytes(&b, 1);
        n |= uint32_t(b) << (i * 8);
      }
    }
  }
  void real(float& f) {
    uint32_t n;
    std::memcpy(&n, &f, 4);
    number(n);
    if (!out) std::memcpy(&f, &n, 4);
  }
  template <size_t N>
  void text(char (&s)[N]) {
    bytes(s, N);
    if (!out && !std::memchr(s, 0, N)) ok = false;
  }
  void flag(bool& b) {
    uint32_t n = b;
    number(n);
    if (n > 1) ok = false;
    if (!out) b = n != 0;
  }
  void small(uint16_t& n) {
    uint32_t v = n;
    number(v);
    if (v > UINT16_MAX) ok = false;
    if (!out) n = static_cast<uint16_t>(v);
  }
  void card(study::CardState& c) {
    uint32_t lo = static_cast<uint64_t>(c.ankiCardId), hi = static_cast<uint64_t>(c.ankiCardId) >> 32;
    number(lo);
    number(hi);
    if (!out) c.ankiCardId = int64_t(uint64_t(lo) | uint64_t(hi) << 32);
    real(c.stability);
    real(c.difficulty);
    uint32_t due = c.dueDay, last = c.lastReviewDay;
    number(due);
    number(last);
    if (!out) {
      c.dueDay = static_cast<int32_t>(due);
      c.lastReviewDay = static_cast<int32_t>(last);
    }
    small(c.reps);
    small(c.lapses);
    small(c.dueMinute);
    uint32_t state = c.state, step = c.stepIndex;
    number(state);
    number(step);
    if (state > 4 || step >= study::kMaxSteps || c.dueMinute >= 1440 || !std::isfinite(c.stability) ||
        !std::isfinite(c.difficulty) || c.stability < 0 || c.difficulty < 0 || c.difficulty > 10)
      ok = false;
    if (!out) {
      c.state = state;
      c.stepIndex = step;
    }
  }
  void fields(Word& w) {
    text(w.id);
    text(w.surface);
    text(w.headword);
    text(w.meaning);
    text(w.dictionary);
    text(w.language);
    text(w.latest);
    card(w.card);
    number(w.encounters);
    flag(w.known);
    flag(w.shortened);
    uint32_t rating = out ? w.lastRating : 0, failures = out ? w.failures : 0;
    if (version >= 2) {
      number(rating);
      number(failures);
    }
    if (rating > 4 || failures > UINT16_MAX) ok = false;
    if (!out) {
      w.lastRating = rating;
      w.failures = failures;
    }
    if (!validId(w.id) || !validId(w.latest) || !*w.headword || !*w.meaning || !*w.dictionary || !w.encounters)
      ok = false;
  }
  void fields(Context& c) {
    text(c.id);
    text(c.word);
    text(c.path);
    text(c.book);
    text(c.author);
    text(c.chapter);
    text(c.surface);
    text(c.sentence);
    number(c.offset);
    small(c.spine);
    small(c.page);
    flag(c.fragment);
    uint32_t purpose = 0;
    if (out) purpose = static_cast<uint8_t>(c.purpose);
    if (version >= 2) number(purpose);
    if (purpose > 1) ok = false;
    if (!out) c.purpose = static_cast<Purpose>(purpose);
    if (!validId(c.id) || !validId(c.word) || c.path[0] != '/' || !*c.sentence || !*c.surface) ok = false;
  }
};
template <typename T>
size_t writeRecord(const T& value, uint8_t* dst, size_t size, uint32_t kind) {
  if (size < 20) return 0;
  Codec c{dst, nullptr, size};
  uint32_t magic = 0x31564243, version = 2;
  c.version = version;
  c.number(magic);
  c.number(version);
  c.number(kind);
  // The codec's write path does not modify fields. Avoid a second multi-KiB
  // copy merely to share the bounded field traversal with its read path.
  c.fields(const_cast<T&>(value));
  if (!c.ok) return 0;
  uint32_t crc = checksum(dst, c.at);
  c.number(crc);
  return c.ok ? c.at : 0;
}
template <typename T>
bool readRecord(const uint8_t* src, size_t size, T& value, uint32_t kind) {
  if (size < 20) return false;
  Codec c{nullptr, src, size};
  uint32_t magic = 0, version = 0, type = 0;
  c.number(magic);
  c.number(version);
  c.number(type);
  if (magic != 0x31564243 || version < 1 || version > 2 || type != kind) return false;
  c.version = version;
  c.fields(value);
  uint32_t crc = 0;
  const auto end = c.at;
  c.number(crc);
  return c.ok && c.at == size && checksum(src, end) == crc;
}
}  // namespace
bool validId(const char* s) {
  for (size_t i = 0; i < 32; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
  return s[32] == 0;
}
bool copy(char* out, size_t cap, const char* in) {
  if (!cap) return false;
  const auto length = std::strlen(in);
  size_t n = std::min(cap - 1, length);
  if (n < length)
    while (n && (static_cast<unsigned char>(in[n]) & 0xc0) == 0x80) --n;
  std::memcpy(out, in, n);
  out[n] = 0;
  return n == length;
}
void identify(Word& w) {
  Hash h;
  h.add(w.dictionary);
  h.add(w.headword);
  h.finish(w.id);
}
void identify(Context& c) {
  Hash h;
  h.add(c.word);
  // Paths are case-sensitive identities even on host filesystems; keep their
  // bytes distinct rather than using the headword case-folding helper.
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(c.path); *p; ++p) {
    h.a = (h.a ^ *p) * 1099511628211ull;
    h.b = (h.b ^ *p) * 14029467366897019727ull;
  }
  h.add("");
  char spine[16];
  std::snprintf(spine, sizeof(spine), "%u", c.spine);
  h.add(spine);
  if (c.purpose == Purpose::Language) h.add("translation");
  h.finish(c.id);
}
Capture capture(Records& db, Word& incoming, Context& ctx, Word& old, Context& oldContext, Undo& token) {
  token = {};
  if (!hasText(incoming.headword) || !hasText(incoming.meaning) || !*incoming.dictionary ||
      !hasText(incoming.surface) || !hasText(ctx.sentence) || ctx.path[0] != '/')
    return Capture::Rejected;
  identify(incoming);
  copy(ctx.word, sizeof(ctx.word), incoming.id);
  copy(ctx.surface, sizeof(ctx.surface), incoming.surface);
  identify(ctx);
  const auto found = db.word(incoming.id, old);
  if (found == Read::Error) return Capture::Failed;
  if (found == Read::Ok &&
      (std::strcmp(old.dictionary, incoming.dictionary) || std::strcmp(old.headword, incoming.headword)))
    return Capture::Failed;
  const auto source = db.context(ctx.id, oldContext);
  if (source == Read::Error) return Capture::Failed;
  if (source == Read::Ok && (std::strcmp(oldContext.word, ctx.word) || std::strcmp(oldContext.path, ctx.path) ||
                             oldContext.spine != ctx.spine || oldContext.purpose != ctx.purpose))
    return Capture::Failed;
  copy(token.word, sizeof(token.word), incoming.id);
  copy(token.context, sizeof(token.context), ctx.id);
  token.newWord = found == Read::Missing;
  token.newContext = source == Read::Missing;
  if (!token.newWord) {
    copy(token.previousContext, sizeof(token.previousContext), old.latest);
    token.previousEncounters = old.encounters;
    incoming.card = old.card;
    incoming.lastRating = old.lastRating;
    incoming.failures = old.failures;
    incoming.known = old.known;
    // A dictionary update must not silently replace a meaning already studied.
    copy(incoming.meaning, sizeof(incoming.meaning), old.meaning);
    incoming.shortened = old.shortened;
  }
  incoming.encounters = token.previousEncounters == UINT32_MAX ? UINT32_MAX : token.previousEncounters + 1;
  copy(incoming.latest, sizeof(incoming.latest), ctx.id);
  if (token.newContext && !db.put(ctx)) return Capture::Failed;
  if (!db.put(incoming)) {
    if (token.newContext) db.removeContext(ctx.id);
    return Capture::Failed;
  }
  token.valid = true;
  return token.newWord ? Capture::Saved : Capture::Updated;
}
bool undo(Records& db, Undo& t, Word& scratch) {
  if (!t.valid) return false;
  if (t.newWord) {
    if (!db.removeWord(t.word)) return false;
  } else {
    if (db.word(t.word, scratch) != Read::Ok) return false;
    scratch.encounters = t.previousEncounters;
    copy(scratch.latest, sizeof(scratch.latest), t.previousContext);
    if (!db.put(scratch)) return false;
  }
  if (t.newContext && !db.removeContext(t.context)) return false;
  t.valid = false;
  return true;
}
bool sentenceEnd(const char* s) {
  size_t n = std::strlen(s);
  while (n && (s[n - 1] == '"' || s[n - 1] == '\'' || s[n - 1] == ')' || s[n - 1] == ']')) --n;
  if (n && (s[n - 1] == '.' || s[n - 1] == '!' || s[n - 1] == '?')) return true;
  return n >= 3 &&
         (!std::memcmp(s + n - 3, "。", 3) || !std::memcmp(s + n - 3, "！", 3) || !std::memcmp(s + n - 3, "？", 3));
}
void cloze(const char* sentence, const char* surface, char* out, size_t cap) {
  if (!cap) return;
  if (!*surface) {
    copy(out, cap, sentence);
    return;
  }
  const auto letter = [](unsigned char c) {
    return c >= 128 || (lower(c) >= 'a' && lower(c) <= 'z') || (c >= '0' && c <= '9') || c == '_';
  };
  const size_t n = std::strlen(surface);
  size_t used = 0, at = 0;
  while (sentence[at]) {
    const char* p = sentence + at;
    size_t match = 0;
    while (match < n && p[match] && lower(p[match]) == lower(surface[match])) ++match;
    const bool before = at == 0 || !letter(sentence[at - 1]) || !letter(surface[0]);
    const bool after = match == n && (!letter(p[n]) || !letter(surface[n - 1]));
    if (match == n && before && after) {
      if (used + 5 >= cap) break;
      std::memcpy(out + used, "[...]", 5);
      used += 5;
      at += n;
    } else {
      size_t bytes = 1;
      while (p[bytes] && (static_cast<unsigned char>(p[bytes]) & 0xc0) == 0x80) ++bytes;
      if (used + bytes >= cap) break;
      std::memcpy(out + used, p, bytes);
      used += bytes;
      at += bytes;
    }
  }
  out[used] = 0;
}
void reviewQuestion(const Context& context, const bool withGap, const char* fragmentLabel, char* out,
                    const size_t cap) {
  if (!cap) return;
  if (!withGap) {
    copy(out, cap, context.surface);
    return;
  }
  const size_t prefix = context.fragment ? std::strlen(fragmentLabel) : 0;
  if (prefix >= cap) {
    copy(out, cap, context.surface);
    return;
  }
  std::memcpy(out, fragmentLabel, prefix);
  cloze(context.sentence, context.surface, out + prefix, cap - prefix);
  if (!std::strstr(out + prefix, "[...]")) copy(out, cap, context.surface);
}
uint32_t checksum(const uint8_t* p, size_t size) {
  uint32_t crc = 2166136261u;
  for (size_t i = 0; i < size; ++i) crc = (crc ^ p[i]) * 16777619u;
  return crc;
}
size_t encode(const Word& w, uint8_t* p, size_t n) { return writeRecord(w, p, n, 1); }
size_t encode(const Context& c, uint8_t* p, size_t n) { return writeRecord(c, p, n, 2); }
bool decode(const uint8_t* p, size_t n, Word& w) { return readRecord(p, n, w, 1); }
bool decode(const uint8_t* p, size_t n, Context& c) { return readRecord(p, n, c, 2); }
}  // namespace vocabulary
