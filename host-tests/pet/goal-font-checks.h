#pragma once
#include "../../lib/EpdFont/builtinFonts/notosans_8_regular.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_10_bold.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_12_bold.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_12_regular.h"
#include "../../src/apps_local/ui/fonts/book_bold_10.h"
#include "../../src/apps_local/ui/fonts/reading_serif_11.h"
#include "../../src/apps_local/ui/fonts/reading_serif_14.h"
#include "../../src/apps_local/ui/fonts/reading_serif_bold_11.h"
#include "../../src/apps_local/ui/fonts/reading_serif_bold_14.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#include "../../src/apps_local/ui/fonts/ui_prose_bold_14.h"

struct GoalTarget : Target {
  bool alternative = false, watching = false, elided = false;
  fui::Rect nameBox{};
  std::string name;
  GoalTarget(uint8_t* data, int w, int h) : Target(data, w, h) {}
  fui::TextStyle resolve(fui::TextStyle style) const {
    if (alternative && style.bold) style.font = style.font == 1 || style.font == 2 ? 6 : 7;
    return style;
  }
  fui::Size measureText(fui::FontId id, const char* value, fui::TextStyle style) const override {
    style.font = id;
    style = resolve(style);
    return backend.measureText(style.font, value, style);
  }
  fui::Rect inkBounds(fui::FontId id, const char* value, fui::TextStyle style) const override {
    style.font = id;
    style = resolve(style);
    return backend.inkBounds(style.font, value, style);
  }
  void text(fui::Rect r, const char* value, fui::TextStyle style) override {
    if (watching && clipRect().x >= nameBox.x && clipRect().right() <= nameBox.right() &&
        clipRect().y >= nameBox.y && clipRect().bottom() <= nameBox.bottom()) {
      CHECK(style.font == 0 || style.font == 1);
      CHECK(clipRect().x >= nameBox.x && clipRect().y >= nameBox.y);
      CHECK(clipRect().right() <= nameBox.right() && clipRect().bottom() <= nameBox.bottom());
      const auto ink = inkBounds(style.font, value, style);
      CHECK(r.y + ink.y >= nameBox.y && r.y + ink.bottom() <= nameBox.bottom());
      elided |= std::strstr(value, "…") != nullptr;
      name += value;
    }
    style = resolve(style);
    backend.text(r, value, style);
  }
};
struct GoalCanvas {
  std::vector<uint8_t> pixels;
  FontBridge pixelSmall{toybox_14}, pixelBody{toybox_20}, caption{notosans_8_regular}, regular{ui_prose_10};
  FontBridge ubuntuSmall{ubuntu_10_regular}, ubuntuSmallBold{ubuntu_10_bold}, ubuntuBody{ubuntu_12_regular},
      ubuntuBold{ubuntu_12_bold};
  FontBridge sansBody{ui_prose_14}, sansBold{ui_prose_bold_14}, sansSmallBold{book_bold_10};
  FontBridge serifSmall{reading_serif_11}, serifSmallBold{reading_serif_bold_11}, serifBody{reading_serif_14},
      serifBold{reading_serif_bold_14};
  GoalTarget target;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;
  GoalCanvas(int style, int width, int height)
      : pixels((width + 7) / 8 * height, 255),
        target(pixels.data(), width, height),
        frame(target, Canvas::context(target), input, interactions),
        screen(frame) {
    const fui::BitmapFont *small = &pixelSmall.font, *body = &pixelBody.font, *smallBold = small, *bodyBold = body;
    switch (style) {
      case 1:
        small = &ubuntuSmall.font;
        body = &ubuntuBody.font;
        smallBold = &ubuntuSmallBold.font;
        bodyBold = &ubuntuBold.font;
        break;
      case 2:
        small = &regular.font;
        body = &sansBody.font;
        smallBold = &sansSmallBold.font;
        bodyBold = &sansBold.font;
        break;
      case 3:
        small = &serifSmall.font;
        body = &serifBody.font;
        smallBold = &serifSmallBold.font;
        bodyBold = &serifBold.font;
        break;
    }
    for (int id : {0, 4}) target.backend.setFont(id, *small);
    for (int id : {1, 2}) target.backend.setFont(id, *body);
    target.backend.setFont(3, caption.font);
    target.backend.setFont(6, *bodyBold);
    target.backend.setFont(7, *smallBold);
    target.alternative = style != 0;
  }
};
static std::string withoutSpaces(const char* text) {
  std::string result;
  while (*text) {
    if (*text != ' ' && *text != '\n') result += *text;
    ++text;
  }
  return result;
}
static void goalFontChecks(const char* output) {
  for (int style = 0; style < 4; ++style)
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (bool wide : {false, true}) {
        auto c = std::make_unique<GoalCanvas>(style, wide ? 800 : 480, wide ? 480 : 800);
        pet::Profile p;
        p.created = true;
        p.readingSeconds = 1200;
        petui::Model m;
        m.profile = &p;
        petui::Work work;
        const auto body = pixelui::geometry(c->screen.device()).body;
        const int columnWidth = wide ? body.width - body.width * 44 / 100 - petui::widget::GAP : body.width;
        const auto card = fui::makeRect(12, 150, (columnWidth - petui::widget::GAP) * 60 / 100, 140);
        const auto originalClip = c->target.clipRect();
        for (int item = -1; item < int(pet::OBJECT_COUNT); ++item) {
          c->interactions.clear();
          p.readingSeconds = item < 0 ? 0 : 1200;
          p.project = std::max(0, item);
          const auto* title = item < 0 ? tr(STR_PET_HATCH_GOAL) : petui::objectName(item);
          const int side = std::min({80, int(card.width) * 40 / 100, int(card.height) - 40});
          c->target.nameBox = fui::makeRect(card.x + side + 24, card.y + 9, card.width - side - 36,
                                           card.height - 64);
          c->target.name.clear();
          c->target.elided = false;
          c->target.watching = true;
          petui::widget::objective(c->screen, m, work, card);
          c->target.watching = false;
          if (c->target.elided) {
            const auto prefix = withoutSpaces(c->target.name.substr(0, c->target.name.find("…")).c_str());
            CHECK(prefix.size() >= 12 && withoutSpaces(title).find(prefix) == 0);
            std::printf("Overview title uses catalogue fallback: style%d %s item%d %s\n", style,
                        language == Language::ES ? "ES" : "EN", item, title);
          } else
            CHECK(withoutSpaces(c->target.name.c_str()) == withoutSpaces(title));
          CHECK(c->target.clipRect().x == originalClip.x && c->target.clipRect().width == originalClip.width);
          if (item >= 0) {
            c->interactions.clear();
            c->target.nameBox = fui::makeRect(153, 199, columnWidth - 155, 66);
            c->target.name.clear();
            c->target.elided = false;
            c->target.watching = true;
            petui::widget::project(c->screen, m, work, fui::makeRect(12, 150, columnWidth, 180));
            c->target.watching = false;
            if (c->target.elided) {
              const auto prefix = withoutSpaces(c->target.name.substr(0, c->target.name.find("…")).c_str());
              CHECK(prefix.size() >= 12 && withoutSpaces(title).find(prefix) == 0);
              std::printf("Home title uses catalogue fallback: style%d %s item%d %s\n", style,
                          language == Language::ES ? "ES" : "EN", item, title);
            } else
              CHECK(withoutSpaces(c->target.name.c_str()) == withoutSpaces(title));
            CHECK(c->target.clipRect().x == originalClip.x && c->target.clipRect().width == originalClip.width);
          }
          if (output && item == 174) {
            FILE* file = std::fopen((std::string(output) + "/goal-font-" + std::to_string(style) + "-" +
                                     (language == Language::ES ? "es-" : "en-") + (wide ? "wide.pbm" : "portrait.pbm"))
                                        .c_str(),
                                    "wb");
            CHECK(file != nullptr);
            if (file) {
              std::fprintf(file, "P4\n%d %d\n", wide ? 800 : 480, wide ? 480 : 800);
              for (auto byte : c->pixels) std::fputc(byte ^ 255, file);
              std::fclose(file);
            }
          }
        }
      }
    }
}
