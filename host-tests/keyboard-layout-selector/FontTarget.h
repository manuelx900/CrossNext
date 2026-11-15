#pragma once
// Host-only font decoder, raster target and glyph observer for this suite.
#include <BidiUtils.h>
#include <EpdFontFamily.h>
#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>
#include <Utf8.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "lib/GfxRenderer/FontFallback.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>
#pragma GCC diagnostic pop
#include "locales.inc"
namespace fui = freeink::ui;
static std::vector<char> catalogue;
static std::vector<const char*> labels;
I18n& I18n::getInstance() {
  static I18n instance;
  return instance;
}
void I18n::setLanguage(Language l) { _language = l; }
const char* I18n::get(StrId id) const { return labels.at(size_t(id)); }
static std::vector<uint8_t> read(const std::string& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "Cannot open %s\n", file.c_str());
    std::exit(2);
  }
  return {std::istreambuf_iterator<char>(in), {}};
}
static uint32_t le32(const uint8_t* p) {
  return p[0] | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
static uint16_t le16(const uint8_t* p) { return p[0] | uint16_t(p[1]) << 8; }
struct SdFont {
  std::vector<uint8_t> file;
  std::vector<EpdUnicodeInterval> intervals[2];
  std::vector<EpdGlyph> glyphs[2];
  std::vector<EpdLigaturePair> ligatures[2];
  EpdFontData data[2]{};
  EpdFont regular{&data[0]}, bold{&data[1]};
  EpdFontFamily family{&regular, &bold};
  void load(const std::string& name) {
    file = read(name);
    for (int n = 0; n < 2; n++) {
      const auto* h = file.data() + 32 + 32 * n;
      auto& d = data[n];
      size_t off = le32(h + 24);
      d.intervalCount = le32(h + 4);
      d.advanceY = h[12];
      d.ascender = int16_t(le16(h + 13));
      d.descender = int16_t(le16(h + 15));
      d.is2Bit = (le16(file.data() + 10) & 1) != 0;
      intervals[n].resize(d.intervalCount);
      std::memcpy(intervals[n].data(), file.data() + off, 12 * d.intervalCount);
      d.intervals = intervals[n].data();
      off += 12 * d.intervalCount;
      glyphs[n].resize(le32(h + 8));
      std::memcpy(glyphs[n].data(), file.data() + off, 16 * glyphs[n].size());
      d.glyph = glyphs[n].data();
      off += 16 * le32(h + 8);
      d.kernLeftEntryCount = le16(h + 17);
      d.kernRightEntryCount = le16(h + 19);
      // UI fallback skips kerning; Thai cluster substitutions are resident.
      const size_t left = d.kernLeftEntryCount, right = d.kernRightEntryCount;
      off += 3 * (left + right) + size_t(h[21]) * h[22];
      d.ligaturePairCount = le16(h + 28);
      ligatures[n].resize(d.ligaturePairCount);
      std::memcpy(ligatures[n].data(), file.data() + off, 8 * d.ligaturePairCount);
      d.ligaturePairs = ligatures[n].data();
      off += 8 * d.ligaturePairCount;
      d.bitmap = file.data() + off;
    }
  }
};
static EpdFont small{&notosans_8_regular}, normal10{&ubuntu_10_regular}, bold10{&ubuntu_10_bold},
    normal12{&ubuntu_12_regular}, bold12{&ubuntu_12_bold};
static EpdFontFamily f8{&small}, f10{&normal10, &bold10}, f12{&normal12, &bold12};
static std::array<SdFont, 9> sd;
static std::map<int, EpdFontFamily> fonts;
static std::map<int, int> fallbacks;
static std::string currentCase, outputDir;
static int currentLanguage, frames = 0, issues = 0;
static FILE* report;
static int board = 0;
static std::string jsonQuote(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += char(c);
    } else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else if (c < 32) {
      char b[8];
      std::snprintf(b, sizeof(b), "\\u%04x", c);
      out += b;
    } else
      out += char(c);
  }
  return out + '"';
}
static EpdFontFamily::Style weight(fui::TextStyle s) { return s.bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR; }
static const EpdFontFamily& resolve(int id, const char* text, fui::TextStyle s) {
  return fonts.at(fontfallback::resolve(fonts, fallbacks, id, text, weight(s)));
}
static std::string visual(const char* text) {
  std::string out;
  if (BidiUtils::applyBidiVisual(text, out)) return out;
  return text;
}
struct FontTarget final : fui::DrawTarget {
  fui::DisplayTarget base;
  std::vector<uint8_t>& pixels;
  int width, height;
  bool paint;
  FontTarget(std::vector<uint8_t>& p, int w, int h, bool painting)
      : base(p.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        pixels(p),
        width(w),
        height(h),
        paint(painting) {}
  void setFont(unsigned slot, int) {
    fonts.insert_or_assign(slot, f10);
    fallbacks[slot] = 101;
  }
  fui::Rect clipRect() const override { return base.clipRect(); }
  bool setClipRect(fui::Rect r) override { return base.setClipRect(r); }
  int16_t lineHeight(fui::FontId f) const override { return fonts.at(f).getData()->advanceY; }
  fui::Size measureText(fui::FontId f, const char* s, fui::TextStyle st) const override {
    const auto& font = resolve(f, s, st);
    int w, h;
    font.getTextDimensions(visual(s).c_str(), &w, &h, weight(st));
    return {int16_t(w), lineHeight(f)};
  }
  fui::Rect inkBounds(fui::FontId id, const char* text, fui::TextStyle st) const override {
    const auto& f = resolve(id, text, st);
    auto v = visual(text);
    int minX, minY, maxX, maxY;
    f.getTextBounds(v.c_str(), &minX, &minY, &maxX, &maxY, weight(st));
    const int baseline = f.getData()->ascender + (lineHeight(id) - f.getData()->advanceY) / 2;
    return fui::makeRect(minX, baseline - maxY, maxX - minX, maxY - minY);
  }

  void fill(fui::Rect r, fui::Paint p, uint8_t radius, uint8_t corners) override {
    if (paint) base.fill(r, p, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t w, uint8_t radius, uint8_t corners) override {
    if (paint) base.stroke(r, p, w, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override {
    if (paint) base.line(a, b, w, p);
  }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override {
    if (paint) base.triangle(a, b, c, p);
  }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode m, fui::Paint p, fui::Rotation rot) override {
    if (paint) base.bitmap(r, b, m, p, rot);
  }
  void note(const char* type, fui::Rect r, const char* text, fui::TextStyle st, unsigned lines, fui::Rect glyph = {}) {
    issues++;
    std::fprintf(report,
                 "{\"language\":%s,\"case\":%s,\"screen\":[%d,%d],\"type\":%s,\"rect\":[%d,%d,%d,%d],\"font\":%d,"
                 "\"lines\":%u,\"maxLines\":%d,\"text\":%s,\"clip\":[%d,%d,%d,%d],\"glyph\":[%d,%d,%d,%d]}\n",
                 jsonQuote(localeCodes[currentLanguage]).c_str(), jsonQuote(currentCase).c_str(), width, height,
                 jsonQuote(type).c_str(), r.x, r.y, r.width, r.height, st.font, lines, st.maxLines,
                 jsonQuote(text).c_str(), clipRect().x, clipRect().y, clipRect().width, clipRect().height, glyph.x,
                 glyph.y, glyph.width, glyph.height);
  }
  void glyphLine(fui::Rect r, const char* text, fui::TextStyle st) {
    auto v = visual(text);
    const auto& f = resolve(st.font, text, st);
    auto type = weight(st);
    const auto* d = f.getData(type);
    int measured = measureText(st.font, text, st).width;
    int x = r.x + (st.align == fui::TextAlign::Center  ? (r.width - measured) / 2
                   : st.align == fui::TextAlign::Right ? r.width - measured
                                                       : 0);
    int y = r.y + f.getData()->ascender + (lineHeight(st.font) - f.getData()->advanceY) / 2;
    bool reportedClip = false;
    int bx = x, bleft = 0, bwidth = 0, btop = 0;
    int32_t advance = 0;
    uint32_t prev = 0, cp;
    const char* at = v.c_str();
    while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&at)))) {
      const bool mark = utf8IsCombiningMark(cp);
      if (!mark) cp = f.applyLigatures(cp, at, type);
      const auto* g = f.getGlyph(cp, type);
      if (!g) continue;
      if (!mark && prev) bx += fp4::toPixel(advance + f.getKerning(prev, cp, type));
      const auto anchor = combiningMark::anchorFor(cp);
      int gx = mark ? combiningMark::anchorOver(anchor, bx, bleft, bwidth, g->left, g->width) : bx;
      int gy = y - (mark ? combiningMark::raiseAboveBase(anchor, g->top, g->height, btop) : 0);
      auto clip = clipRect();
      if (!reportedClip && g->width && g->height &&
          (gx + g->left < clip.x || gy - g->top < clip.y || gx + g->left + g->width > clip.right() ||
           gy - g->top + g->height > clip.bottom())) {
        note("glyph-clipped", r, text, st, 1, fui::makeRect(gx + g->left, gy - g->top, g->width, g->height));
        reportedClip = true;
      }
      if (!paint) {
        if (!mark) {
          bleft = g->left;
          bwidth = g->width;
          btop = g->top;
          advance = g->advanceX;
          prev = cp;
        }
        continue;
      }
      for (int row = 0; row < g->height; row++)
        for (int col = 0; col < g->width; col++) {
          int bit = row * g->width + col;
          int coverage = d->is2Bit ? ((d->bitmap[g->dataOffset + bit / 4] >> (6 - (bit % 4) * 2)) & 3)
                                   : ((d->bitmap[g->dataOffset + bit / 8] >> (7 - bit % 8)) & 1) * 3;
          if (!coverage) continue;
          int px = gx + g->left + col, py = gy - g->top + row;
          if (px < 0 || py < 0 || px >= width || py >= height || px < clip.x || px >= clip.right() || py < clip.y ||
              py >= clip.bottom())
            continue;
          auto& b = pixels[py * ((width + 7) / 8) + px / 8];
          if (st.color == fui::Color::White)
            b |= 0x80 >> (px % 8);
          else
            b &= ~(0x80 >> (px % 8));
        }
      if (!mark) {
        bleft = g->left;
        bwidth = g->width;
        btop = g->top;
        advance = g->advanceX;
        prev = cp;
      }
    }
  }
  void text(fui::Rect r, const char* text, fui::TextStyle st) override {
    if (!text || !*text) return;
    const auto shaped = visual(text);
    const char* scan = shaped.c_str();
    const auto& face = resolve(st.font, text, st);
    uint32_t codepoint;
    while ((codepoint = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&scan)))) {
      if (codepoint <= 0x20 || (codepoint >= 0x200b && codepoint <= 0x200f) ||
          (codepoint >= 0x202a && codepoint <= 0x202e) || (codepoint >= 0x2066 && codepoint <= 0x2069))
        continue;
      if (!face.hasCodepoint(codepoint, weight(st))) {
        note("missing-glyph", r, text, st, 1);
        break;
      }
    }
    unsigned lines = measureText(st.font, text, st).width <= r.width && !std::strchr(text, '\n')
                         ? 1
                         : fui::textAreaWalk(*this, r.width, text, st, [](uint32_t, const fui::TextAreaLine&) {});
    const int lh = lineHeight(st.font);
    if (lines > unsigned(std::max(1, int(st.maxLines))) || (lines > 1 && int(lines) * lh > r.height))
      note("overflow", r, text, st, lines);
    if (r.x < 0 || r.y < 0 || r.right() > width || r.y + std::min(lh, int(r.height)) > height)
      note("outside-screen", r, text, st, lines);
    int shown = std::min<unsigned>(lines, std::max(1, int(st.maxLines)));
    int start = r.y + std::max(0, (r.height - shown * lh) / 2);
    fui::textAreaWalk(*this, r.width, text, st, [&](uint32_t n, const fui::TextAreaLine& line) {
      if (n >= unsigned(shown)) return;
      std::string s(text + line.start, line.len);
      glyphLine(fui::makeRect(r.x, start + n * lh, r.width, lh), s.c_str(), st);
    });
  }
};
static void bind(int lang) {
  fonts.clear();
  fallbacks.clear();
  for (int id : {0, 1, 2, int(fui::HEADER_CLOCK_FONT)}) fonts.emplace(id, f12);
  fonts.emplace(3, f8);
  fonts.emplace(4, f10);
  int group = lang == 37 ? 3 : lang == 38 ? 6 : 0;
  for (int n = 0; n < 3; n++) fonts.emplace(100 + n, sd[group + n].family);
  for (int id : {0, 1, 2, int(fui::HEADER_CLOCK_FONT)}) fallbacks[id] = 102;
  fallbacks[3] = 100;
  fallbacks[4] = 101;
}