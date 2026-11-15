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

#include "../../lib/GfxRenderer/FontFallback.h"
#include "../../src/apps_local/quest/QuestMilestones.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "../../src/apps_local/quest/QuestScreens.h"
#include "../../src/apps_local/quest/QuestWidgets.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>

#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#include "../../src/apps_local/ui/fonts/ui_prose_bold_14.h"
#pragma GCC diagnostic pop
#include "../../.cache/locale-ui/fixtures/locales.inc"
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
static EpdFont huntLabelRegular{&ui_prose_14}, huntLabelBold{&ui_prose_bold_14};
static EpdFontFamily huntLabelFamily{&huntLabelRegular, &huntLabelBold};
static EpdFont coinValue{&toybox_30};
static EpdFontFamily coinValueFamily{&coinValue};
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
struct Target final : fui::DrawTarget {
  fui::DisplayTarget base;
  std::vector<uint8_t>& pixels;
  int width, height;
  bool paint;
  unsigned huntCountLabels = 0;
  bool experienceMode = false, experienceStarted = false;
  int experienceTextBottom = 0;
  Target(std::vector<uint8_t>& p, int w, int h, bool painting)
      : base(p.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        pixels(p),
        width(w),
        height(h),
        paint(painting) {}
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
    if (text[0] == char(0xc3) && text[1] == char(0x97)) ++huntCountLabels;
    if (experienceMode && !std::strcmp(text, tr(STR_QUEST_EXPERIENCE))) experienceStarted = true;
    if (experienceStarted && std::strcmp(text, tr(STR_DONE)))
      experienceTextBottom = std::max(experienceTextBottom, int(r.y + inkBounds(st.font, text, st).bottom()));
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
  fonts.insert_or_assign(questui::HUNT_LABEL_FONT, huntLabelFamily);
  fonts.emplace(3, f8);
  fonts.emplace(4, f10);
  fonts.emplace(questui::COIN_VALUE_FONT, coinValueFamily);
  int group = lang == 37 ? 3 : lang == 38 ? 6 : 0;
  for (int n = 0; n < 3; n++) fonts.emplace(100 + n, sd[group + n].family);
  for (int id : {0, 1, 2, int(fui::HEADER_CLOCK_FONT)}) fallbacks[id] = 102;
  fallbacks[3] = 100;
  fallbacks[4] = 101;
}
static int frame(questui::Model model, int width, int height, const std::string& name, bool shot = false,
                 bool levelPopup = false) {
  currentCase = name;
  std::vector<uint8_t> pixels((width + 7) / 8 * height, 255);
  Target target(pixels, width, height, shot);
  target.experienceMode = model.view == questui::View::Experience;
  auto device = target.base.deviceContext();
  device.hasTouch = true;
  device.safeArea = width < height ? (board == 0 ? fui::Insets{10, 1, 0, 1} : fui::Insets{9, 3, 3, 3})
                                   : (board == 0 ? fui::Insets{1, 10, 1, 0} : fui::Insets{3, 9, 3, 3});
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::HeaderStatus status;
  std::snprintf(status.clock, sizeof(status.clock), "23:59");
  toybox::Frame context(target, device, input, interactions, nullptr, status);
  toybox::Screen screen(context);
  questui::Work work;
  if (levelPopup)
    questui::buildLevelUp(screen, *model.profile, false, work);
  else
    questui::build(screen, model, work);
  if (name.starts_with("collection-") && target.huntCountLabels) std::exit(11);
  if (name.starts_with("hunt-record-") && quest::requestClaimed(*model.profile, model.selectedRequest) &&
      target.huntCountLabels != 1)
    std::exit(12);
  if (model.view == questui::View::Experience) {
    bool close = false;
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& hit = interactions.data()[n];
      if (hit.action != questui::Back) std::exit(9);
      if (hit.rect.width > 200 && target.experienceTextBottom > hit.rect.y - 4) {
        std::fprintf(stderr, "Experience text overlaps Done: %s %s %dx%d bottom=%d button=%d\n",
                     localeCodes[currentLanguage], name.c_str(), width, height, target.experienceTextBottom,
                     hit.rect.y);
        std::exit(15);
      }
      close = true;
    }
    if (!close) std::exit(10);
  }
  if (name == "insufficient-coins") {
    bool back = false;
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& hit = interactions.data()[n];
      if (hit.action == questui::ConfirmPurchase) std::exit(6);
      back |= hit.action == questui::Back;
    }
    if (!back) std::exit(7);
  }
  if (model.view == questui::View::Request && quest::validRequest(model.selectedRequest) &&
      (quest::requestUnlocked(*model.profile, model.selectedRequest) ||
       quest::requestClaimed(*model.profile, model.selectedRequest))) {
    bool rewardHit = false;
    for (size_t n = 0; n < interactions.count(); ++n)
      rewardHit |= interactions.data()[n].action == questui::ShowHuntRewards;
    if (rewardHit == quest::requestClaimed(*model.profile, model.selectedRequest)) std::exit(8);
  }
  frames++;
  if (shot) {
    std::string file =
        outputDir + "/" + std::string(localeCodes[currentLanguage]) + "-" + name + "-" + std::to_string(width) + ".pbm";
    FILE* f = std::fopen(file.c_str(), "wb");
    std::fprintf(f, "P4\n%d %d\n", width, height);
    for (auto p : pixels) std::fputc(p ^ 255, f);
    std::fclose(f);
  }
  return work.pages;
}
int main(int argc, char** argv) {
  if (argc < 3) return 2;
  outputDir = argv[2];
  board = argc > 4 ? std::atoi(argv[4]) : 0;
  report = std::fopen((outputDir + "/issues.jsonl").c_str(), "w");
  for (int group = 0; group < 3; group++)
    for (int n = 0; n < 3; n++)
      sd[group * 3 + n].load(std::string(argv[1]) + "/assets_local/language-fonts/Interface" +
                             (group == 1   ? "_SC"
                              : group == 2 ? "_TC"
                                           : "") +
                             "_" +
                             std::to_string(n == 0   ? 8
                                            : n == 1 ? 10
                                                     : 12) +
                             ".cpfont");
  int only = argc > 3 ? std::atoi(argv[3]) : -1;
  for (currentLanguage = 0; currentLanguage < 39; currentLanguage++) {
    if (only >= 0 && only != currentLanguage) continue;
    auto bytes = read(std::string(argv[1]) + "/.cache/locale-ui/fixtures/" + std::to_string(currentLanguage) + ".bin");
    catalogue.assign(bytes.begin(), bytes.end());
    labels.clear();
    for (size_t i = 0; i < catalogue.size();) {
      labels.push_back(catalogue.data() + i);
      i += std::strlen(catalogue.data() + i) + 1;
    }
    I18N.setLanguage(Language(currentLanguage));
    bind(currentLanguage);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      int width = size.first, height = size.second;
      quest::Profile hero;
      std::snprintf(hero.name, sizeof(hero.name), "MAXIMILIANO");
      hero.xp = 10000000;
      hero.bonus = 123456;
      hero.campaign.initialized = true;
      hero.campaign.prologueSeen = true;
      hero.campaign.introducedBooks = 12;
      hero.campaign.narratedEncounters = 720;
      hero.campaign.encounter = 719;
      hero.campaign.phase = quest::BattlePhase::Reading;
      hero.campaign.state = quest::AdventureState::Active;
      readingstats::Summary summary;
      summary.anchorDay = 20770;
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.today = 20770;
      model.selectedEncounter = 719;
      model.selectedBook = 11;
      model.selectedRequest = 0;
      {
        auto complete = hero;
        complete.campaign.phase = quest::BattlePhase::Complete;
        for (int item = 0; item < quest::kCollectionItemCount; ++item)
          if (quest::visibleItem(item)) quest::grantItem(complete, item);
        const auto original = model;
        model.profile = &complete;
        model.view = questui::View::Shop;
        for (int category = 0; category < quest::kCategoryCount; ++category) {
          model.category = category;
          frame(model, width, height, "completed-category-" + std::to_string(category));
        }
        model = original;
      }
      if (argc > 7 && std::string(argv[5]) == "polish-preview") {
        const auto saved = read(argv[7]);
        if (saved.size() != 8192 || !quest::decode(saved.data(), hero)) return 13;
        sdart::Pack base, rewards, outfits, story, guardians;
        const std::string card = argv[6] + std::string("/apps/reading-adventure/");
        if (!base.open((card + "graphics.bin").c_str(), questassets::COUNT) ||
            !rewards.open((card + "rewards.bin").c_str(), questassets::REWARDS_COUNT) ||
            !outfits.open((card + "bestiary-outfits.bin").c_str(), questassets::BESTIARY_OUTFITS_COUNT) ||
            !story.open((card + "story.bin").c_str(), questassets::STORY_COUNT) ||
            !guardians.open((card + "guardians.bin").c_str(), questassets::GUARDIANS_COUNT))
          return 14;
        model.assets = &base;
        model.rewardAssets = &rewards;
        model.outfitAssets = &outfits;
        model.storyAssets = &story;
        model.enemyAssets = &guardians;
        model.view = questui::View::Experience;
        frame(model, width, height, "level-stats", true);
        model.failed = true;
        frame(model, width, height, "level-stats-save-notice", true);
        model.failed = false;
        for (int category = 0; category < quest::kCategoryCount; ++category) {
          model.category = category;
          model.view = questui::View::Shop;
          model.preview = -1;
          frame(model, width, height, "completed-shop-" + std::to_string(category), true);
          model.view = questui::View::Gear;
          model.preview = quest::catalogItem(quest::Slot(category), 0);
          frame(model, width, height, "gear-detail-" + std::to_string(category), true);
        }
        model.preview = -1;
        model.view = questui::View::Campaign;
        model.campaignTab = questui::CampaignTab::Saga;
        model.page = 7 / questui::sagaPageSize(width > height);
        frame(model, width, height, "saga-viii", true);
        model.view = questui::View::Book;
        model.selectedBook = 7;
        for (int page = 0; page < 2; ++page) {
          model.introductionPage = page;
          frame(model, width, height, "saga-story-" + std::to_string(page), true);
        }
        questreturn::Report report;
        report.kind = questreturn::Kind::Logged;
        report.seconds = 15 * 60;
        report.requestId = quest::kCrystalHunt;
        report.requestBefore = 0;
        report.requestAfter = 15;
        report.requestTarget = quest::requestTarget(hero, report.requestId);
        model.readingReturn = &report;
        model.view = questui::View::ReadingReturn;
        model.returnPanel = questreturn::Panel::Request;
        frame(model, width, height, "return-hunt", true);
        hero.campaign.state = quest::AdventureState::Active;
        hero.campaign.phase = quest::BattlePhase::Reading;
        hero.campaign.encounter = quest::battleAnchor(118);
        report.kind = questreturn::Kind::Combat;
        report.before = 0;
        report.after = 15 * 60;
        report.target = quest::encounterTarget(hero);
        report.xp = 45;
        report.requestId = -1;
        model.returnPanel = questreturn::Panel::Reading;
        frame(model, width, height, "return-campaign", true);
        continue;
      }
      if (argc > 7 && std::string(argv[5]) == "hero-preview") {
        const auto saved = read(argv[7]);
        if (saved.size() != 8192 || !quest::decode(saved.data(), hero)) return 11;
        sdart::Pack base, rewards, outfits;
        const std::string card = argv[6];
        if (!base.open((card + "/apps/reading-adventure/graphics.bin").c_str(), questassets::COUNT) ||
            !rewards.open((card + "/apps/reading-adventure/rewards.bin").c_str(), questassets::REWARDS_COUNT) ||
            !outfits.open((card + "/apps/reading-adventure/bestiary-outfits.bin").c_str(),
                          questassets::BESTIARY_OUTFITS_COUNT))
          return 12;
        model.assets = &base;
        model.rewardAssets = &rewards;
        model.outfitAssets = &outfits;
        model.view = questui::View::Hero;
        frame(model, width, height, "hero-preview", true);
        model.view = questui::View::Experience;
        frame(model, width, height, "experience-max", true);
        hero.xp = quest::threshold(50) + 72;
        frame(model, width, height, "experience-50", true);
        continue;
      }
      if (argc > 5 && std::string(argv[5]) == "changes") {
        for (auto v : {questui::View::Hero, questui::View::Settings, questui::View::Profile}) {
          model.view = v;
          frame(model, width, height, "changes-" + std::to_string(int(v)), true);
        }
        hero.xp = hero.bonus = hero.spent = hero.campaign.extraCoins = 0;
        model.view = questui::View::PurchaseConfirm;
        model.preview = quest::catalogItem(quest::Slot::Body, 3);
        frame(model, width, height, "insufficient-coins", true);
        continue;
      }
      for (int v = 0; v <= int(questui::View::Experience); v++) {
        model.view = questui::View(v);
        frame(model, width, height, "view-" + std::to_string(v),
              currentLanguage == 11 || currentLanguage == 1 || currentLanguage == 33 || currentLanguage == 37);
      }
      const auto savedXp = hero.xp;
      for (unsigned level : {1u, 50u, 149u, 150u}) {
        model.view = questui::View::Experience;
        hero.xp = quest::threshold(level);
        for (int notice = 0; notice < 3; ++notice) {
          model.failed = notice == 1;
          model.storeNotice = notice == 2 ? quest::StoreNotice::Recovered : quest::StoreNotice::None;
          frame(model, width, height,
                "experience-start-" + std::to_string(level) + "-notice-" + std::to_string(notice));
          if (level < quest::MAX_LEVEL) {
            hero.xp = quest::threshold(level + 1) - 1;
            frame(model, width, height,
                  "experience-end-" + std::to_string(level) + "-notice-" + std::to_string(notice));
            hero.xp = quest::threshold(level);
          }
        }
        model.failed = false;
        model.storeNotice = quest::StoreNotice::None;
      }
      hero.xp = savedXp;
      for (int t = 0; t < 3; t++)
        for (auto phase : {quest::BattlePhase::Reading, quest::BattlePhase::Victory, quest::BattlePhase::Story,
                           quest::BattlePhase::Complete}) {
          model.view = questui::View::Campaign;
          model.campaignTab = questui::CampaignTab(t);
          hero.campaign.phase = phase;
          frame(model, width, height, "campaign-" + std::to_string(t) + "-" + std::to_string(int(phase)));
        }
      hero.campaign.phase = quest::BattlePhase::Reading;
      for (int category = 0; category < quest::kCategoryCount; category++) {
        model.category = category;
        for (auto v : {questui::View::Shop, questui::View::Gear}) {
          model.view = v;
          model.preview = -1;
          frame(model, width, height, "catalog-" + std::to_string(int(v)) + "-" + std::to_string(category));
          for (int n = 0; n < quest::count(quest::Slot(category)); n++) {
            model.preview = quest::catalogItem(quest::Slot(category), n);
            frame(model, width, height, "item-" + std::to_string(model.preview) + "-" + std::to_string(int(v)));
          }
        }
      }
      model.preview = -1;
      for (int request = 0; request < quest::kRequestCount; request++) {
        model.selectedRequest = request;
        model.view = questui::View::Request;
        frame(model, width, height, "hunt-" + std::to_string(request));
      }
      for (int book = 0; book < 12; book++) {
        model.selectedBook = book;
        model.view = questui::View::Book;
        int pages = 1;
        for (int page = 0; page < pages; page++) {
          model.introductionPage = page;
          pages = frame(model, width, height, "book-" + std::to_string(book) + "-" + std::to_string(page));
          if (pages > 300) return 3;
        }
      }
      for (int chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; chapter++) {
        model.view = questui::View::Lore;
        model.selectedEncounter = quest::battleAnchor(chapter);
        int pages = 1;
        for (int page = 0; page < pages; page++) {
          model.introductionPage = page;
          pages = frame(model, width, height, "path-" + std::to_string(chapter) + "-" + std::to_string(page));
          if (pages > 100) return 3;
        }
      }
      {
        quest::Profile fresh;
        auto saved = model.profile;
        model.profile = &fresh;
        model.view = questui::View::Campaign;
        int pages = 1;
        for (int page = 0; page < pages; page++) {
          model.introductionPage = page;
          pages = frame(model, width, height, "prologue-" + std::to_string(page));
          if (pages > 100) return 3;
        }
        model.profile = saved;
      }
      model.introductionPage = 0;
      for (int hunt : {0, quest::kRequestCount - 1}) {
        hero.activeRequest = hunt;
        hero.requestTargetMinutes = 120;
        hero.requestReported = 1800;
        hero.requestElapsed = 1800;
        model.selectedRequest = (hunt + 1) % quest::kRequestCount;
        for (auto v :
             {questui::View::Requests, questui::View::RequestCancelConfirm, questui::View::RequestSwitchConfirm}) {
          model.view = v;
          frame(model, width, height, "active-" + std::to_string(hunt) + "-" + std::to_string(int(v)));
        }
      }
      hero.activeRequest = quest::kRequestCount;
      {
        const auto original = model;
        const auto originalHero = hero;
        for (unsigned rank = 1; rank <= 150; rank += 5) {
          hero.xp = quest::threshold(rank);
          model.view = questui::View::Hero;
          frame(model, width, height, "rank-" + std::to_string(rank));
        }
        for (unsigned rank : {50u, 100u, 150u}) {
          hero.xp = quest::threshold(rank);
          hero.noticedLevel = uint16_t(rank - 1);
          frame(model, width, height, "level-up-" + std::to_string(rank), false, true);
        }
        hero.xp = 10000000;
        hero.noticedLevel = 150;
        for (unsigned mask : {0u, 1u, 3u}) {
          hero.celebratedMilestones = uint8_t(mask);
          model.view = questui::View::Celebration;
          frame(model, width, height, "milestone-" + std::to_string(mask));
        }
        hero.celebratedMilestones = 0;
        for (int category = 0; category < quest::kSlotCount; ++category) {
          model.category = category;
          model.view = questui::View::PurchaseConfirm;
          for (int n = 0; n < quest::count(quest::Slot(category)); ++n) {
            model.preview = quest::catalogItem(quest::Slot(category), n);
            if (!quest::storyExclusive(model.preview))
              frame(model, width, height, "buy-" + std::to_string(model.preview));
          }
        }
        model.preview = -1;
        for (int category = 0; category < quest::kSlotCount; ++category) {
          model.category = category;
          model.view = questui::View::Shop;
          int pages = 1;
          for (int page = 0; page < pages; ++page) {
            model.page = page;
            pages = frame(model, width, height, "shop-page-" + std::to_string(category) + "-" + std::to_string(page));
          }
          for (int n = 0; n < quest::count(quest::Slot(category)); ++n)
            quest::grantItem(hero, quest::catalogItem(quest::Slot(category), n));
          model.view = questui::View::Gear;
          pages = 1;
          for (int page = 0; page < pages; ++page) {
            model.page = page;
            pages = frame(model, width, height, "gear-page-" + std::to_string(category) + "-" + std::to_string(page));
          }
        }
        model.page = 0;
        hero.activeRequest = 0;
        for (int hunt : quest::HUNT_OUTFIT_SOURCES) {
          model.selectedRequest = hunt;
          model.view = questui::View::HuntRewards;
          frame(model, width, height, "rewards-" + std::to_string(hunt));
        }
        model.view = questui::View::RequestSwitchConfirm;
        for (int hunt = 1; hunt < quest::kRequestCount; ++hunt) {
          model.selectedRequest = hunt;
          frame(model, width, height, "switch-" + std::to_string(hunt));
        }
        hero.activeRequest = quest::kRequestCount;
        for (int n = 1; n < quest::count(quest::Slot::Body); ++n) {
          const int item = quest::catalogItem(quest::Slot::Body, n);
          quest::grantItem(hero, item);
          hero.outfitOffers[0] = uint16_t(item);
          model.view = questui::View::TryOut;
          for (int gender = 0; gender < 2; ++gender) {
            hero.gender = quest::Gender(gender);
            frame(model, width, height, "try-" + std::to_string(item) + "-" + std::to_string(gender));
          }
        }
        hero.gender = quest::Gender::Masculine;
        model.preview = -1;
        for (auto view : {questui::View::Name, questui::View::Profile, questui::View::Hero}) {
          model.nameDraft = "éééééééééééé";
          model.view = view;
          frame(model, width, height, "name-accent-" + std::to_string(int(view)));
          model.nameDraft = "冒険冒険冒険冒険冒険冒険";
          std::snprintf(hero.name, sizeof(hero.name), "%s", model.nameDraft);
          frame(model, width, height, "name-cjk-" + std::to_string(int(view)));
        }
        std::snprintf(hero.name, sizeof(hero.name), "MAXIMILIANO");
        questreturn::Report won;
        won.kind = questreturn::Kind::Combat;
        won.victory = true;
        won.seconds = 3600;
        won.xp = 9000;
        model.readingReturn = &won;
        for (int book = 0; book < 12; ++book) {
          hero.campaign.encounter = uint16_t(book * 60 + 59);
          hero.campaign.phase = quest::BattlePhase::Story;
          model.view = questui::View::ReadingReturn;
          model.returnPanel = questreturn::Panel::Victory;
          frame(model, width, height, "saga-victory-" + std::to_string(book));
        }
        hero.campaign.encounter = 719;
        hero.campaign.phase = quest::BattlePhase::Complete;
        for (auto& win : hero.campaignVictories) win = {20770, 108000, 9000, 900, 3};
        for (auto& win : hero.victories) win = {20770, 108000, 9000, 900, 3};
        for (auto& count : hero.huntWins) count = 3;
        for (int n = 0; n < quest::kHuntOutfitCount; ++n) {
          const int id = quest::huntOutfitSource(quest::huntOutfitAt(n));
          if (quest::validRequest(id)) hero.requestClaims[id / 8] |= uint8_t(1u << (id % 8));
        }
        model.view = questui::View::Requests;
        model.requestFilter = questui::RequestFilter::Collection;
        int pages = 1;
        for (int page = 0; page < pages; ++page) {
          model.page = page;
          pages = frame(model, width, height, "collection-" + std::to_string(page));
        }
        model.page = 0;
        for (int book = 0; book < 12; ++book) {
          model.view = questui::View::AtlasRegion;
          model.selectedBook = book;
          int pages = 1;
          for (int page = 0; page < pages; ++page) {
            model.page = page;
            pages = frame(model, width, height, "atlas-grid-" + std::to_string(book) + "-" + std::to_string(page));
          }
        }
        model.page = 0;
        for (int chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; ++chapter) {
          model.view = questui::View::AtlasPlace;
          model.selectedEncounter = quest::battleAnchor(chapter);
          frame(model, width, height, "atlas-record-" + std::to_string(chapter));
        }
        for (int hunt = 0; hunt < quest::kRequestCount; ++hunt) {
          model.view = questui::View::Request;
          model.selectedRequest = hunt;
          frame(model, width, height, "hunt-record-" + std::to_string(hunt));
        }
        model.preview = quest::catalogItem(quest::Slot::Body, 3);
        model.failed = true;
        for (auto view : {questui::View::Missions, questui::View::Shop, questui::View::PurchaseConfirm,
                          questui::View::Requests, questui::View::RequestSwitchConfirm}) {
          model.view = view;
          frame(model, width, height, "save-error-" + std::to_string(int(view)));
        }
        hero = originalHero;
        model = original;
      }
      questreturn::Report progress;
      progress.kind = questreturn::Kind::Combat;
      progress.seconds = 1200;
      progress.before = 0;
      progress.after = 1200;
      progress.target = 1800;
      model.readingReturn = &progress;
      model.view = questui::View::ReadingReturn;
      frame(model, width, height, "partial");
      progress.victory = true;
      progress.after = 1800;
      progress.xp = 900;
      frame(model, width, height, "victory");
    }
    std::printf("%s: %d frames, %d text issues\n", localeCodes[currentLanguage], frames, issues);
    std::fflush(stdout);
  }
  std::fclose(report);
  std::printf("TOTAL %d frames, %d text issues\n", frames, issues);
  return 0;
}
