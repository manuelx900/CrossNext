#include "FlashText.h"

#include <Epub/htmlEntities.h>
#include <strings.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace flashtext {
namespace {
bool append(char* out, size_t capacity, size_t& at, const char* text, size_t length) {
  if (length >= capacity - at) return false;
  std::memcpy(out + at, text, length);
  at += length;
  out[at] = 0;
  return true;
}
struct Writer {
  char* out;
  size_t capacity, at = 0;
  bool text(const char* s, size_t n) { return append(out, capacity, at, s, n); }
  bool text(const char* s) { return text(s, std::strlen(s)); }
  bool line() {
    while (at && out[at - 1] == ' ') out[--at] = 0;
    return !at || out[at - 1] == '\n' || text("\n");
  }
};
bool same(const char* a, size_t n, const char* b) { return n == std::strlen(b) && !strncasecmp(a, b, n); }
bool word(char c) { return std::isalpha(static_cast<unsigned char>(c)); }

// The device displays a bounded textual equivalent; it does not execute TeX.
bool math(const char*& s, Writer& w, int depth, bool group = false) {
  if (depth > 8) return false;
  while (*s) {
    if (*s == '}') {
      ++s;
      if (group) return true;
      continue;
    }
    if (*s == '{') {
      ++s;
      if (!math(s, w, depth + 1, true)) return false;
      continue;
    }
    if (*s == '^' || *s == '_') {
      if (!w.text(s++, 1)) return false;
      if (*s == '{') {
        ++s;
        if (!w.text("(") || !math(s, w, depth + 1, true) || !w.text(")")) return false;
      }
      continue;
    }
    if (*s == '\\') {
      ++s;
      const char* command = s;
      while (word(*s)) ++s;
      const size_t n = s - command;
      if (!n) {
        if (*s == '\\') {
          ++s;
          if (!w.line()) return false;
        } else if (*s == ',' || *s == ';' || *s == '!') {
          ++s;
          if (!w.text(" ")) return false;
        } else if (*s && !w.text(s++, 1))
          return false;
        continue;
      }
      if (same(command, n, "newline")) {
        if (!w.line()) return false;
        continue;
      }
      if (same(command, n, "sqrt")) {
        while (*s == ' ') ++s;
        if (!w.text("sqrt(")) return false;
        if (*s == '{') {
          ++s;
          if (!math(s, w, depth + 1, true)) return false;
        } else if (*s && !w.text(s++, 1))
          return false;
        if (!w.text(")")) return false;
        continue;
      }
      if (same(command, n, "frac") || same(command, n, "dfrac") || same(command, n, "tfrac")) {
        while (*s == ' ') ++s;
        if (*s != '{') return false;
        ++s;
        if (!w.text("(") || !math(s, w, depth + 1, true) || !w.text(")/(")) return false;
        while (*s == ' ') ++s;
        if (*s != '{') return false;
        ++s;
        if (!math(s, w, depth + 1, true) || !w.text(")")) return false;
        continue;
      }
      static constexpr const char* styles[] = {"textbf",    "mathbf",  "mathrm", "text", "emph",
                                               "centering", "caption", "ce",     "left", "right"};
      bool style = false;
      for (auto name : styles)
        if (same(command, n, name)) style = true;
      if (style) continue;
      if (same(command, n, "begin") || same(command, n, "end")) {
        if (*s == '{') {
          while (*s && *s != '}') ++s;
          if (*s) ++s;
        }
        continue;
      }
      struct Command {
        const char* from;
        const char* to;
      };
      static constexpr Command commands[] = {{"rightarrow", " -> "},
                                             {"leftarrow", " <- "},
                                             {"cdot", " * "},
                                             {"times", " x "},
                                             {"propto", " proportional to "},
                                             {"therefore", "therefore "},
                                             {"approx", " ~= "},
                                             {"infty", "infinity"},
                                             {"pm", " +/- "},
                                             {"int", "integral "},
                                             {"sum", "sum "},
                                             {"prod", "product "},
                                             {"alpha", "alpha"},
                                             {"beta", "beta"},
                                             {"gamma", "gamma"},
                                             {"delta", "delta"},
                                             {"Delta", "Delta"},
                                             {"sigma", "sigma"},
                                             {"Sigma", "Sigma"},
                                             {"lambda", "lambda"},
                                             {"mu", "mu"},
                                             {"Omega", "Omega"},
                                             {"omega", "omega"},
                                             {"theta", "theta"},
                                             {"rho", "rho"},
                                             {"pi", "pi"},
                                             {"epsilon", "epsilon"},
                                             {"varphi", "phi"},
                                             {"hbar", "h-bar"}};
      bool known = false;
      for (const auto& cmd : commands)
        if (n == std::strlen(cmd.from) && !std::memcmp(command, cmd.from, n)) {
          if (!w.text(cmd.to)) return false;
          known = true;
          break;
        }
      if (!known && (!w.text(command, n) || !w.text(" "))) return false;
      continue;
    }
    if (*s == '&') {
      ++s;
      if (!w.text(" | ")) return false;
    } else if (!w.text(s++, 1))
      return false;
  }
  return true;
}

bool renderMath(const char* begin, const char* end, Writer& w) {
  const size_t n = end - begin;
  if (n + 1 >= w.capacity - w.at) return false;
  char* scratch = w.out + w.capacity - n - 1;
  std::memmove(scratch, begin, n);
  scratch[n] = 0;
  const char* expression = scratch;
  Writer bounded{w.out, static_cast<size_t>(scratch - w.out), w.at};
  if (!math(expression, bounded, 0)) return false;
  w.at = bounded.at;
  return true;
}

bool entity(const char*& s, Writer& w) {
  const char* end = s + 1;
  while (*end && *end != ';' && end - s < 32) ++end;
  if (*end != ';') return w.text(s++, 1);
  const size_t length = end - s + 1;
  if (s[1] == '#') {
    char* stop = nullptr;
    const bool hex = s[2] == 'x' || s[2] == 'X';
    const unsigned long code = std::strtoul(s + (hex ? 3 : 2), &stop, hex ? 16 : 10);
    if (stop == end && stop > s + (hex ? 3 : 2) && code && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff)) {
      char utf[4];
      size_t n = 0;
      if (code < 128)
        utf[n++] = char(code);
      else if (code < 2048) {
        utf[n++] = char(0xc0 | (code >> 6));
        utf[n++] = char(0x80 | (code & 63));
      } else if (code < 65536) {
        utf[n++] = char(0xe0 | (code >> 12));
        utf[n++] = char(0x80 | ((code >> 6) & 63));
        utf[n++] = char(0x80 | (code & 63));
      } else {
        utf[n++] = char(0xf0 | (code >> 18));
        utf[n++] = char(0x80 | ((code >> 12) & 63));
        utf[n++] = char(0x80 | ((code >> 6) & 63));
        utf[n++] = char(0x80 | (code & 63));
      }
      s = end + 1;
      return w.text(utf, n);
    }
  }
  if (same(s, length, "&nbsp;")) {
    s = end + 1;
    return w.text(" ");
  }
  const char* value = lookupHtmlEntity(s, length);
  if (!value) return w.text(s++, 1);
  s = end + 1;
  return w.text(value);
}
}  // namespace

bool unescape(const char* source, char* out, size_t capacity) {
  if (!source || !out || !capacity) return false;
  out[0] = 0;
  Writer w{out, capacity};
  while (*source) {
    if (*source == '&') {
      if (!entity(source, w)) return false;
    } else if (!w.text(source++, 1))
      return false;
  }
  return true;
}
bool plain(const char* source, char* out, size_t capacity) {
  if (!source || !out || !capacity) return false;
  out[0] = 0;
  Writer w{out, capacity};
  uint16_t lists[12]{};
  int level = 0;
  bool preformatted = false;
  const char* s = source;
  while (*s) {
    const bool escaped = !std::strncmp(s, "&lt;", 4);
    if (*s == '<' || escaped) {
      const char* begin = s + (escaped ? 4 : 1);
      const bool closing = *begin == '/';
      if (closing) ++begin;
      const char* name = begin;
      while (word(*begin) || std::isdigit(static_cast<unsigned char>(*begin))) ++begin;
      const size_t n = begin - name;
      const bool list = same(name, n, "ol") || same(name, n, "ul");
      const bool block = same(name, n, "br") || same(name, n, "p") || same(name, n, "div") || same(name, n, "li") ||
                         same(name, n, "hr") || same(name, n, "tr") || same(name, n, "h1") || same(name, n, "h2");
      if (!escaped || list || block) {
        const char* end = begin;
        char quote = 0;
        while (*end) {
          if (quote) {
            if (*end == quote) quote = 0;
          } else if (*end == '\'' || *end == '"')
            quote = *end;
          else if ((!escaped && *end == '>') || (escaped && !std::strncmp(end, "&gt;", 4)))
            break;
          ++end;
        }
        if (!*end) return false;
        s = end + (escaped ? 4 : 1);
        if (same(name, n, "script") || same(name, n, "style")) {
          if (!closing) {
            while (*s && strncasecmp(s, "</", 2)) ++s;
          }
          continue;
        }
        if (same(name, n, "pre")) {
          preformatted = !closing;
          if (!w.line()) return false;
        } else if (same(name, n, "sup") || same(name, n, "sub")) {
          if (!w.text(closing ? ")" : same(name, n, "sup") ? "^(" : "_(")) return false;
        }
        if (list) {
          if (!w.line()) return false;
          if (closing) {
            if (level) --level;
          } else {
            if (level == 12) return false;
            lists[level++] = same(name, n, "ol") ? 1 : 0;
          }
        } else if (block) {
          if (!w.line()) return false;
          if (same(name, n, "li") && !closing) {
            char number[12];
            if (level && lists[level - 1])
              std::snprintf(number, sizeof(number), "%u. ", lists[level - 1]++);
            else
              std::snprintf(number, sizeof(number), "- ");
            if (!w.text(number)) return false;
          }
        } else if ((same(name, n, "td") || same(name, n, "th")) && !closing && w.at && out[w.at - 1] != '\n') {
          if (!w.text(" | ")) return false;
        }
        continue;
      }
    }
    if (!std::strncmp(s, "[sound:", 7)) {
      const char* end = std::strchr(s, ']');
      if (end) {
        s = end + 1;
        continue;
      }
    }
    if (!std::strncmp(s, "[latex]", 7)) {
      s += 7;
      continue;
    }
    if (!std::strncmp(s, "[/latex]", 8)) {
      s += 8;
      continue;
    }
    if (*s == '\\' && (s[1] == '(' || s[1] == '[')) {
      const char close[] = {'\\', s[1] == '(' ? ')' : ']', 0};
      const char* end = std::strstr(s + 2, close);
      if (!end || !renderMath(s + 2, end, w)) return false;
      s = end + 2;
      continue;
    }
    if (*s == '$') {
      const bool display = s[1] == '$';
      const char* begin = s + (display ? 2 : 1);
      const char* end = display ? std::strstr(begin, "$$") : std::strchr(begin, '$');
      if (end) {
        // Render only the math segment, leaving shell and currency text alone.
        if (!renderMath(begin, end, w)) return false;
        s = end + (display ? 2 : 1);
        continue;
      }
    }
    if (*s == '\\' && word(s[1])) {
      const char* end = s + 1;
      while (*end && *end != '<' && *end != '$' && *end != '&') ++end;
      if (!renderMath(s, end, w)) return false;
      s = end;
      continue;
    }
    if (*s == '&') {
      if (!entity(s, w)) return false;
      continue;
    }
    if (*s == '\r' || *s == '\n' || *s == '\t') {
      if (preformatted && *s != '\r') {
        if (!w.text(s++, 1)) return false;
        continue;
      }
      ++s;
      if (w.at && out[w.at - 1] != ' ' && out[w.at - 1] != '\n' && !w.text(" ")) return false;
      continue;
    }
    if (!preformatted && *s == ' ' && (!w.at || out[w.at - 1] == ' ' || out[w.at - 1] == '\n')) {
      ++s;
      continue;
    }
    if (!w.text(s++, 1)) return false;
  }
  while (w.at && (out[w.at - 1] == ' ' || out[w.at - 1] == '\n')) out[--w.at] = 0;
  return true;
}
bool cloze(const char* source, unsigned ordinal, bool reveal, char* out, size_t capacity) {
  size_t at = 0;
  out[0] = 0;
  for (size_t n = 0; source[n];) {
    if (!std::strncmp(source + n, "{{c", 3)) {
      char* endNumber = nullptr;
      const unsigned number = std::strtoul(source + n + 3, &endNumber, 10);
      if (endNumber && endNumber[0] == ':' && endNumber[1] == ':') {
        const char* content = endNumber + 2;
        const char* end = std::strstr(content, "}}");
        if (!end) return false;
        const char* nested = std::strstr(content, "{{");
        if (nested && nested < end) return false;
        const char* hint = std::strstr(content, "::");
        if (hint && hint > end) hint = nullptr;
        if (reveal || number != ordinal) {
          if (!append(out, capacity, at, content, (hint ? hint : end) - content)) return false;
        } else {
          if (!append(out, capacity, at, "[", 1) ||
              !append(out, capacity, at, hint ? hint + 2 : "...", hint ? end - hint - 2 : 3) ||
              !append(out, capacity, at, "]", 1))
            return false;
        }
        n = end - source + 2;
        continue;
      }
    }
    if (!append(out, capacity, at, source + n, 1)) return false;
    ++n;
  }
  return true;
}
}  // namespace flashtext
