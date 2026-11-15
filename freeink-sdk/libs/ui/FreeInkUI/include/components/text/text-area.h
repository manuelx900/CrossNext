#pragma once

#include "../../FreeInkUICore.h"

namespace freeink {
namespace ui {

// --- TextArea: multi-line scrollable writing canvas --------------------------
//
// A pure-render multi-line text surface (the editor body). The app owns the text
// buffer and caret offset and feeds it keystrokes; textArea word-wraps `text`
// into the rect, draws the window of visual lines starting at `topLine`, and (if
// showCaret) a thin caret at `cursor`. Unlike layoutText() it is top-aligned and
// has no line cap, so it scrolls arbitrarily long documents. Each source byte
// belongs to exactly one visual line (no spaces dropped) so a caret offset maps
// 1:1 to a (line, column). Keep the caret on screen with textAreaMeasure() +
// textAreaTopLineFor(), mirroring listTopIndexFor() for lists.
struct TextAreaProps {
  const char* text = nullptr;  // NUL-terminated, contiguous
  uint32_t cursor = 0;         // caret byte offset into text
  uint32_t topLine = 0;        // first visual line drawn at the top of the rect
  TextStyle style{};
  bool showCaret = true;
  // Selection highlight: byte range [selStart, selEnd). Empty (selStart>=selEnd)
  // means no selection. Drawn as a dithered band behind the text so it reads on
  // a 1-bit panel without inverting glyphs.
  uint32_t selStart = 0;
  uint32_t selEnd = 0;
};

// One wrapped visual line: byte range [start, start+len) of the source text plus
// whether it was terminated by an explicit '\n' (consumed, not part of len).
struct TextAreaLine {
  uint32_t start;
  uint16_t len;
  bool hardBreak;
};

namespace detail {
inline uint32_t textAreaScalarEnd(const char* text, uint32_t at, uint32_t& cp) {
  const auto first = static_cast<uint8_t>(text[at]);
  const unsigned length = first < 0x80                     ? 1
                          : first >= 0xc2 && first < 0xe0  ? 2
                          : first >= 0xe0 && first < 0xf0  ? 3
                          : first >= 0xf0 && first <= 0xf4 ? 4
                                                           : 1;
  cp = first & (length == 1 ? 0xff : (1u << (7 - length)) - 1);
  for (unsigned n = 1; n < length; ++n) {
    const auto byte = static_cast<uint8_t>(text[at + n]);
    if ((byte & 0xc0) != 0x80) {
      cp = first;
      return at + 1;
    }
    cp = (cp << 6) | (byte & 0x3f);
  }
  return at + length;
}
// Keep common combining marks and Thai vowel clusters on the same bounded line.
inline bool textAreaAttached(uint32_t left, uint32_t right) {
  return (right >= 0x0300 && right <= 0x036f) || (right >= 0x0591 && right <= 0x05bd) || right == 0x05bf ||
         (right >= 0x05c1 && right <= 0x05c2) || (right >= 0x05c4 && right <= 0x05c5) ||
         (right >= 0x0610 && right <= 0x061a) || (right >= 0x064b && right <= 0x065f) || right == 0x0670 ||
         (right >= 0x06d6 && right <= 0x06ed) || (right >= 0x1dc0 && right <= 0x1dff) ||
         (right >= 0x20d0 && right <= 0x20ff) || (right >= 0xfe20 && right <= 0xfe2f) ||
         (right >= 0xfe00 && right <= 0xfe0f) || right == 0x3099 || right == 0x309a ||
         (right >= 0x0e30 && right <= 0x0e3a) || right == 0x0e45 || (right >= 0x0e47 && right <= 0x0e4e) ||
         (left >= 0x0e40 && left <= 0x0e44 && right >= 0x0e01 && right <= 0x0e5b);
}
inline uint32_t textAreaClusterEnd(const char* text, uint32_t at) {
  uint32_t left;
  uint32_t end = textAreaScalarEnd(text, at, left);
  while (text[end] && text[end] != '\n') {
    uint32_t right;
    const uint32_t next = textAreaScalarEnd(text, end, right);
    if (next - at > 220 || !textAreaAttached(left, right)) break;
    left = right;
    end = next;
  }
  return end;
}
}  // namespace detail

// Walks `text` wrapped to `width`, calling emit(index, TextAreaLine) for each
// visual line in order; returns the total visual-line count. Empty text yields
// one empty line; a trailing '\n' yields a final empty line (so the caret has a
// line to sit on). Greedy word wrap, breaking after the last space that fits, or
// mid-word (keeping supported combining and Thai clusters together) for words wider than the rect.
template <typename Emit>
inline uint32_t textAreaWalk(const DrawTarget& target, int16_t width, const char* text, const TextStyle& style,
                             Emit&& emit) {
  if (!text) text = "";
  if (width < 1) width = 1;
  char buf[224];
  const auto fits = [&](uint32_t start, uint32_t len) -> bool {
    if (len > 220) return false;
    const uint32_t n = len;
    memcpy(buf, text + start, n);
    buf[n] = '\0';
    return target.measureText(style.font, buf, style).width <= width;
  };
  uint32_t total = 0;
  uint32_t lineStart = 0;
  for (;;) {
    uint32_t i = lineStart;
    uint32_t lastBreak = lineStart;  // byte after the last fitting space
    bool haveBreak = false;
    while (text[i] != '\0' && text[i] != '\n') {
      const uint32_t next = detail::textAreaClusterEnd(text, i);
      if (i > lineStart && !fits(lineStart, next - lineStart)) break;
      if (text[i] == ' ') {
        lastBreak = next;
        haveBreak = true;
      }
      i = next;
    }
    uint32_t lineEnd;
    uint32_t nextStart;
    bool hard = false;
    if (text[i] == '\n') {
      lineEnd = i;
      nextStart = i + 1;
      hard = true;
    } else if (text[i] == '\0') {
      lineEnd = i;
      nextStart = i;
    } else if (haveBreak && lastBreak > lineStart) {
      lineEnd = lastBreak;  // soft wrap after a space
      nextStart = lastBreak;
    } else {
      lineEnd = i;
      nextStart = i;
    }
    emit(total, TextAreaLine{lineStart, static_cast<uint16_t>(lineEnd - lineStart), hard});
    ++total;
    if (!hard && text[lineEnd] == '\0') break;  // end of buffer, no trailing newline
    if (hard && text[nextStart] == '\0') {
      emit(total, TextAreaLine{nextStart, 0, false});  // trailing newline -> empty final line
      ++total;
      break;
    }
    lineStart = nextStart;
  }
  return total;
}

struct TextAreaMetrics {
  uint32_t lineCount = 0;  // total visual lines
  uint32_t caretLine = 0;  // visual line containing `cursor`
};

// Total visual lines and the line the caret sits on, for scroll math.
inline TextAreaMetrics textAreaMeasure(const DrawTarget& target, int16_t width, const char* text,
                                       const TextStyle& style, uint32_t cursor) {
  TextAreaMetrics m;
  bool found = false;
  m.lineCount = textAreaWalk(target, width, text, style, [&](uint32_t idx, const TextAreaLine& ln) {
    if (!found && cursor >= ln.start && cursor <= static_cast<uint32_t>(ln.start + ln.len)) {
      m.caretLine = idx;
      found = true;
    }
  });
  if (!found) m.caretLine = m.lineCount > 0 ? m.lineCount - 1 : 0;
  return m;
}

// Visual lines that fully fit in the rect at the given line height.
inline uint16_t textAreaVisibleLines(Rect rect, int16_t lineHeight) {
  if (rect.height <= 0 || lineHeight <= 0) return 0;
  return static_cast<uint16_t>(rect.height / lineHeight);
}

// Adjusts topLine the minimal amount so caretLine is visible, then clamps to the
// valid scroll range — the textArea analogue of listTopIndexFor().
inline uint32_t textAreaTopLineFor(uint32_t caretLine, uint32_t topLine, uint16_t visibleLines, uint32_t lineCount) {
  if (visibleLines == 0) return 0;
  const uint32_t maxTop = lineCount > visibleLines ? lineCount - visibleLines : 0;
  uint32_t top = topLine > maxTop ? maxTop : topLine;
  if (caretLine < top) {
    top = caretLine;
  } else if (caretLine >= top + visibleLines) {
    top = caretLine - visibleLines + 1;
  }
  return top > maxTop ? maxTop : top;
}

template <size_t MaxInteractions>
void textArea(Frame<MaxInteractions>& frame, Rect rect, const TextAreaProps& props) {
  DrawTarget& t = frame.target();
  if (rect.empty()) return;
  const int16_t lh = t.lineHeight(props.style.font);
  if (lh <= 0) return;
  const uint16_t visible = static_cast<uint16_t>(rect.height / lh);
  if (visible == 0) return;
  const char* text = props.text ? props.text : "";
  const uint32_t top = props.topLine;
  TextStyle lineStyle = props.style;
  lineStyle.maxLines = 1;
  lineStyle.align = TextAlign::Left;
  char buf[224];
  bool caretDrawn = false;
  const bool hasSel = props.selEnd > props.selStart;
  textAreaWalk(t, rect.width, text, props.style, [&](uint32_t idx, const TextAreaLine& ln) {
    if (idx < top || idx >= top + visible) return;
    const int16_t y = static_cast<int16_t>(rect.y + (idx - top) * lh);
    uint16_t n = ln.len < 220 ? ln.len : 220;
    const uint32_t lineEnd = ln.start + ln.len;

    // Selection band behind the text. Dithered so 1-bit glyphs stay readable.
    if (hasSel && props.selStart <= lineEnd && props.selEnd > ln.start) {
      uint32_t ovS = props.selStart > ln.start ? props.selStart : ln.start;
      uint16_t ps = static_cast<uint16_t>(ovS - ln.start);
      if (ps > n) ps = n;
      memcpy(buf, text + ln.start, ps);
      buf[ps] = '\0';
      const int16_t xs = static_cast<int16_t>(rect.x + t.measureText(props.style.font, buf, props.style).width);
      int16_t xe;
      if (props.selEnd > lineEnd) {
        xe = rect.right();  // selection continues onto the next line: show the line break selected
      } else {
        uint16_t pe = static_cast<uint16_t>(props.selEnd - ln.start);
        if (pe > n) pe = n;
        memcpy(buf, text + ln.start, pe);
        buf[pe] = '\0';
        xe = static_cast<int16_t>(rect.x + t.measureText(props.style.font, buf, props.style).width);
      }
      if (xe > xs) t.fill(Rect{xs, y, static_cast<int16_t>(xe - xs), lh}, Paint::dither(Color::LightGray));
    }

    memcpy(buf, text + ln.start, n);
    buf[n] = '\0';
    t.text(Rect{rect.x, y, rect.width, lh}, buf, lineStyle);
    if (props.showCaret && !caretDrawn && props.cursor >= ln.start &&
        props.cursor <= static_cast<uint32_t>(ln.start + ln.len)) {
      caretDrawn = true;
      uint16_t pre = static_cast<uint16_t>(props.cursor - ln.start);
      if (pre > n) pre = n;
      memcpy(buf, text + ln.start, pre);
      buf[pre] = '\0';
      const int16_t cx = static_cast<int16_t>(rect.x + t.measureText(props.style.font, buf, props.style).width);
      t.fill(Rect{cx, y, 2, lh}, Paint::solid(Color::Black));
    }
  });
}

}  // namespace ui
}  // namespace freeink
