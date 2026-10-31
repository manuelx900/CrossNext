#include "../ui/PageIndicator.h"
#include "PetWidgets.h"

namespace petui {
using namespace widget;
namespace {
const char* description(pet::Species value) {
  static constexpr StrId labels[] = {StrId::STR_PET_SILVA_DESC, StrId::STR_PET_NOX_DESC, StrId::STR_PET_EMBER_DESC};
  return I18N.get(labels[unsigned(value)]);
}
void ruleCard(toybox::Screen& s, fui::Rect r, const char* title, const char* body, const freeink::Icon& mark,
              bool inlineCopy = false) {
  frame(s, r);
  if (inlineCopy) {
    icon(s, rect(r.x + 12, r.y + (r.height - 28) / 2, 28, 28), mark);
    text(s, rect(r.x + 52, r.y + (r.height - 30) / 2, 142, 30), title, 0);
    fui::TextStyle copy;
    copy.font = 4;
    copy.maxLines = 2;
    s.target().text(rect(r.x + 206, r.y + 4, r.width - 218, r.height - 8), body, copy);
    return;
  }
  const bool compact = r.width > r.height * 2;
  icon(s, rect(r.x + 12, r.y + 10, compact ? 30 : 42, compact ? 30 : 42), mark);
  text(s, rect(r.x + (compact ? 56 : 68), r.y + 8, r.width - (compact ? 68 : 80), 30), title, 1);
  const int top = compact ? 48 : 65;
  fui::TextStyle copy;
  copy.font = 4;
  copy.maxLines = 8;
  s.target().text(rect(r.x + 12, r.y + top, r.width - 24, r.height - top - 8), body, copy);
}
}  // namespace
void setup(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  if (m.view == pet::View::Info || m.view == pet::View::Rules) {
    const bool rules = m.view == pet::View::Rules;
    const bool inlineCopy = wide && !rules;
    if (!inlineCopy)
      text(s, rect(r.x + 6, r.y, r.width - 12, 42), rules ? tr(STR_PET_HOW_TO) : tr(STR_PET_READ_TO_GROW), 2);
    const int perPage = rules ? (wide ? 1 : 2) : 3;
    w.pages = 6 / perPage;
    const int page = std::clamp(m.page, 0, w.pages - 1);
    const int top = r.y + (inlineCopy ? 0 : 56);
    const int footer = inlineCopy ? 108 : 112;
    const int height = (r.bottom() - top - footer - GAP * (perPage - 1)) / perPage;
    static constexpr StrId titles[] = {StrId::STR_PET_APP,    StrId::STR_PET_SACIETY, StrId::STR_PET_JOY,
                                       StrId::STR_PET_ENERGY, StrId::STR_PET_HYGIENE, StrId::STR_PET_HOME};
    static constexpr StrId rulesTitles[] = {StrId::STR_PET_FOODS,       StrId::STR_PET_MEDICINE,
                                            StrId::STR_PET_GAMES_SHORT, StrId::STR_PET_HYGIENE,
                                            StrId::STR_PET_DIFFICULTY,  StrId::STR_PET_HOME};
    static constexpr StrId info[] = {StrId::STR_PET_GUIDE_READING, StrId::STR_PET_GUIDE_SATIETY,
                                     StrId::STR_PET_GUIDE_MOOD,    StrId::STR_PET_GUIDE_ENERGY,
                                     StrId::STR_PET_GUIDE_HYGIENE, StrId::STR_PET_GUIDE_HOME};
    static constexpr StrId rulesText[] = {StrId::STR_PET_GUIDE_SUPPLIES,   StrId::STR_PET_GUIDE_FEED,
                                          StrId::STR_PET_GUIDE_GAMES,      StrId::STR_PET_GUIDE_HEALTH,
                                          StrId::STR_PET_GUIDE_DIFFICULTY, StrId::STR_PET_GUIDE_PROJECT};
    static constexpr const freeink::Icon* infoMarks[] = {&extra_reading_book_64, &extra_food_64,
                                                         &icon_hearts_32,        &extra_moon_64,
                                                         &referenceicons::SOAP,  &icon_cat_house_32};
    static constexpr const freeink::Icon* ruleMarks[] = {&extra_food_64,  &referenceicons::REMEDY,
                                                         &icon_games_32,  &referenceicons::SOAP,
                                                         &extra_clock_64, &icon_cat_house_32};
    for (int n = 0; n < perPage; ++n) {
      const int i = page * perPage + n;
      const auto box = rect(r.x, top + n * (height + GAP), r.width, height);
      ruleCard(s, box, I18N.get(rules ? rulesTitles[i] : titles[i]), I18N.get(rules ? rulesText[i] : info[i]),
               *(rules ? ruleMarks[i] : infoMarks[i]), inlineCopy);
    }
    pageui::build(s, rect(r.x, r.bottom() - footer + 6, r.width, inlineCopy ? 36 : 42), page + 1, w.pages, Page, -1,
                  Page, 1);
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54),
           m.profile->created ? tr(STR_PET_CONTINUE) : tr(STR_PET_CHOOSE_EGG), m.profile->created ? Back : SetupTab, 0,
           true);
    return;
  }
  const int footerH = 54;
  text(s, rect(r.x + 7, r.y, r.width - 14, 48), tr(STR_PET_CHOOSE_EGG), 2);
  prose(s, rect(r.x + 8, r.y + 49, r.width - 16, 32), tr(STR_PET_EGG_INTRO), 1);
  const int cardsH = wide ? r.height - 95 - footerH - GAP : std::max(176, int(r.height) * 40 / 100);
  const int cardsWidth = wide ? r.width * 59 / 100 : r.width;
  const auto cards = rect(r.x, r.y + 89, cardsWidth, cardsH);
  for (int i = 0; i < 3; ++i) {
    const auto box = cell(cards, i, 3, 7);
    const bool selected = int(m.choice) == i;
    const bool inverse = button(s, w, box, "", SelectSpecies, i);
    if (selected) s.target().stroke(box, ink, chrome::lightHeader ? 3 : 8, lightui::radius());
    const auto image = rect(box.x + 10, box.y + 12, box.width - 20, box.height - 86);
    art(s, m, image, unsigned(i), speciesIcon(pet::Species(i)), false, inverse);
    pixelui::copyCaps(w.label, sizeof(w.label), eggName(pet::Species(i)));
    fui::TextStyle caption;
    caption.font = 3;
    caption.maxLines = 2;
    caption.align = fui::TextAlign::Center;
    caption.color = inverse ? fui::Color::White : fui::Color::Black;
    s.target().text(rect(box.x + 12, box.bottom() - 61, box.width - 24, 48), w.label, caption);
  }
  const auto about = wide ? rect(cards.right() + GAP, cards.y, r.right() - cards.right() - GAP, cards.height)
                          : rect(r.x, cards.bottom() + GAP, r.width, r.bottom() - footerH - GAP * 2 - cards.bottom());
  const int configH = wide ? 54 : SETUP_CONFIG_H;
  const auto features = rect(about.x, about.y, about.width, about.height - configH - UI_GAP);
  frame(s, features);
  const int infoW = wide ? features.width : features.width * 59 / 100;
  text(s, rect(features.x + UI_INSET, features.y + 8, features.width - UI_INSET * 2, 24), tr(STR_PET_TRAITS),
       wide ? 3 : 0);
  prose(s, rect(features.x + UI_INSET, features.y + 36, infoW - UI_INSET - 6, features.height - 44),
        description(m.choice), wide ? 3 : 4);
  if (!wide)
    art(s, m, rect(features.x + infoW, features.y + 12, features.width - infoW - UI_INSET, features.height - 24),
        3 + unsigned(m.choice) * 16, icon_cat_animals_32, false);
  const auto configuration = rect(about.x, features.bottom() + UI_GAP, about.width, configH);
  const bool inverse = button(s, w, configuration, "", OpenSettings);
  const int mark = wide ? 28 : 34;
  icon(s, rect(configuration.x + UI_INSET, configuration.y + (configH - mark) / 2, mark, mark), icon_go_settings_32,
       inverse);
  icon(s, rect(configuration.right() - UI_INSET - 32, configuration.y + (configH - 36) / 2, 32, 36), icon_opens_32,
       inverse);
  const int copyX = configuration.x + UI_INSET + mark + 12, copyW = configuration.right() - UI_INSET - 44 - copyX;
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_DIFFICULTY_VALUE), modeName(m.selectedMode));
  text(s, rect(copyX, configuration.y + (wide ? 4 : 9), copyW, 24), w.label, wide ? 3 : 0, inverse);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GOAL_VALUE), unsigned(pet::GOALS[m.selectedGoal]));
  text(s, rect(copyX, configuration.y + (wide ? 29 : 39), copyW, 21), w.label, 3, inverse);
  button(s, w, rect(r.x, r.bottom() - footerH, r.width, footerH), tr(STR_PET_BEGIN), Start, 0, true,
         &extra_dashboard_play_64, m.writable && m.clockKnown && !m.historyChanged);
}
void settings(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  if (m.view == pet::View::SettingsConfirm) {
    const int width = std::min(int(r.width), wide ? 680 : 440);
    const int height = std::min(int(r.height), wide ? 320 : 490);
    const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
    frame(s, box);
    icon(s, rect(box.x + 16, box.y + 18, 36, 36), extra_information_32);
    text(s, rect(box.x + 64, box.y + 16, box.width - 80, 40), tr(STR_PET_SETTINGS_EXPIRED_TITLE), 1);
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_SETTINGS_EXPIRED_HELP), modeName(m.selectedMode));
    prose(s, rect(box.x + 18, box.y + 76, box.width - 36, box.height - 218), w.label, 7);
    button(s, w, rect(box.x + 14, box.bottom() - 66, box.width - 28, 52), tr(STR_PET_CANCEL), Back);
    button(s, w, rect(box.x + 14, box.bottom() - 128, box.width - 28, 52), tr(STR_PET_APPLY_AND_END), ConfirmSettings,
           0, true, nullptr, m.writable && m.historyWritable);
    return;
  }
  const int footerH = 52;
  const auto footer = rect(r.x, r.bottom() - footerH, r.width, footerH);
  const int groupGap = UI_GAP, inset = UI_INSET;
  const int modeW = wide ? r.width * 57 / 100 : r.width;
  const int modeH = wide ? footer.y - r.y - groupGap : footer.y - r.y - 112 - 2 * groupGap;
  const auto modes = rect(r.x, r.y, modeW, modeH);
  frame(s, modes);
  text(s, rect(modes.x + inset, modes.y + 8, modes.width - inset * 2, 36), tr(STR_PET_DIFFICULTY), wide ? 1 : 2);
  const int rowGap = wide ? 4 : 10, titleH = 50;
  const int rowH = (modes.height - titleH - inset - 3 * rowGap) / 4;
  static constexpr StrId descriptions[] = {StrId::STR_PET_RELAXED_DESC, StrId::STR_PET_NORMAL_DESC,
                                           StrId::STR_PET_HARD_DESC, StrId::STR_PET_EXTREME_DESC};
  for (int i = 0; i < 4; ++i) {
    const auto box = rect(modes.x + inset, modes.y + titleH + i * (rowH + rowGap), modes.width - inset * 2, rowH);
    const bool inverse = button(s, w, box, "", SelectMode, i, i == int(m.selectedMode));
    const int side = wide ? 28 : 32;
    icon(s, rect(box.x + 12, box.y + (box.height - side) / 2, side, side),
         i == 0   ? extra_moon_64
         : i == 1 ? icon_hearts_32
                  : extra_clock_64,
         inverse);
    const int nameFont = wide ? 3 : 1;
    const int nameH = s.target().lineHeight(nameFont), detailH = s.target().lineHeight(4);
    const int copyY = box.y + (box.height - nameH - detailH - 4) / 2;
    const auto copy = rect(box.x + side + 24, copyY, box.width - side - 72, nameH + detailH + 4);
    text(s, rect(copy.x, copy.y, copy.width, nameH), modeName(pet::Mode(i)), nameFont, inverse);
    fui::TextStyle detail;
    detail.font = 4;
    detail.color = inverse ? fui::Color::White : fui::Color::Black;
    s.target().text(rect(copy.x, copy.y + nameH + 4, copy.width, detailH), I18N.get(descriptions[i]), detail);
    const auto radio = rect(box.right() - 32, box.y + (box.height - 23) / 2, 23, 23);
    s.target().stroke(radio, inverse ? paper : ink, 2, 12);
    if (i == int(m.selectedMode)) s.target().fill(radio.inset({6, 6, 6, 6}), inverse ? paper : ink, 6);
  }
  const auto goals =
      wide ? rect(modes.right() + groupGap, r.y + (modes.height - 152) / 2, r.right() - modes.right() - groupGap, 152)
           : rect(r.x, modes.bottom() + groupGap, r.width, 112);
  frame(s, goals);
  text(s, rect(goals.x + inset, goals.y + 10, goals.width - inset * 2, 32), tr(STR_PET_DAILY_GOAL), 1);
  const auto choices = rect(goals.x + inset, goals.y + 52, goals.width - inset * 2, wide ? 86 : 44);
  for (int i = 0; i < 4; ++i) {
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_MINUTES), static_cast<unsigned long long>(pet::GOALS[i]));
    const auto box = wide ? rect(choices.x + (i % 2) * (choices.width + 8) / 2, choices.y + (i / 2) * 47,
                                 (choices.width - 8) / 2, 39)
                          : cell(choices, i, 4, 8);
    button(s, w, box, w.label, SelectGoal, i, i == m.selectedGoal);
  }
  button(s, w, footer, tr(STR_PET_SAVE), SaveSettings, 0, true, &extra_check_32, m.writable);
}
}  // namespace petui
