#include "QuestScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/PageIndicator.h"
#include "../ui/PixelMenu.h"
#include "../ui/RequiredResources.h"
#include "QuestAvatar.h"
#include "QuestCatalog.h"
#include "QuestIcons.h"
namespace questui {
namespace fui = freeink::ui;
namespace {
constexpr auto ink = fui::Paint::solid(fui::Color::Black);
void text(toybox::Screen& s, fui::Rect r, const char* label, int font = 0, bool left = false) {
  char caps[160];
  pixelui::copyCaps(caps, sizeof(caps), label);
  fui::TextStyle style;
  style.font = font;
  style.align = left ? fui::TextAlign::Left : fui::TextAlign::Center;
  style.maxLines = std::max(1, int(r.height) / std::max(1, int(s.target().lineHeight(font))));
  s.target().text(r, caps, style);
}
void button(toybox::Screen& s, Work& w, fui::Rect r, const char* label, fui::ActionId action, int value = 0,
            bool selected = false) {
  pixelui::copyCaps(w.label, sizeof(w.label), label);
  w.button = {};
  w.button.label = w.label;
  w.button.text.font = action == LevelContinue ? 1 : action == Claim || action == Milestone ? 3 : pixelui::kSmall;
  w.button.text.align = fui::TextAlign::Center;
  w.button.styles = pixelui::rowStyles();
  w.button.minTouchSize = 0;
  w.button.action = action;
  w.button.value = value;
  w.button.state = selected ? fui::StateSelected : fui::StateNormal;
  s.button(w.button, r);
}
void bar(toybox::Screen& s, fui::Rect r, uint64_t v, uint64_t total) {
  s.target().stroke(r, ink, 2);
  const int inset = r.height >= 10 ? 3 : 1;
  if (total)
    s.target().fill(fui::makeRect(r.x + inset, r.y + inset,
                                  (r.width - 2 * inset) * std::min<double>(1, double(v) / total), r.height - 2 * inset),
                    ink);
}
constexpr StrId categories[] = {StrId::STR_QUEST_CLOTHING, StrId::STR_QUEST_AURA_SLOT, StrId::STR_QUEST_PET_SLOT};
void itemName(Work& w, int item) {
  if (!quest::visibleItem(item)) {
    w.name[0] = 0;
    return;
  }
  const auto category = quest::slot(item);
  const int index = item - quest::first(category);
  StrId key = StrId::STR_QUEST_CATALOG_BODY_0;
  switch (category) {
    case quest::Slot::Body:
      key = questcatalog::body[index];
      break;
    case quest::Slot::Aura:
      key = questcatalog::aura[index];
      break;
    case quest::Slot::Pet:
      key = questcatalog::pet[index];
      break;
  }
  std::snprintf(w.name, sizeof(w.name), "%s", I18N.get(key));
}
void sprite(toybox::Screen& s, sdart::Pack* assets, int item, fui::Rect box, bool = false) {
  if (!assets || !quest::visibleItem(item)) return;
  const int index = item - quest::first(quest::slot(item));
  switch (quest::slot(item)) {
    case quest::Slot::Body:
      assets->draw(s.target(), box, questassets::OUTFIT_MASKS + index, fui::BitmapMode::Contain, fui::Color::White);
      assets->draw(s.target(), box, questassets::OUTFITS + index);
      break;
    case quest::Slot::Aura:
      assets->draw(s.target(), box, questassets::AURA_MASKS + index, fui::BitmapMode::Contain, fui::Color::White);
      assets->draw(s.target(), box, questassets::AURAS + index);
      break;
    case quest::Slot::Pet:
      assets->draw(s.target(), box, questassets::PET_ICON_MASKS + index, fui::BitmapMode::Contain, fui::Color::White);
      assets->draw(s.target(), box, questassets::PET_ICONS + index);
      break;
  }
}
void coin(toybox::Screen& s, fui::Rect r) {
  s.target().stroke(r, ink, 2, r.width / 2);
  const auto inner = fui::makeRect(r.x + 4, r.y + 3, r.width - 8, r.height - 6);
  s.target().stroke(inner, ink, 1, inner.width / 2);
}
void lockMark(toybox::Screen& s, fui::Rect r) {
  s.target().stroke(fui::makeRect(r.x + 4, r.y, r.width - 8, r.height * 2 / 3), ink, 2, 4);
  s.target().fill(fui::makeRect(r.x, r.y + r.height / 3, r.width, r.height * 2 / 3), ink);
  s.target().fill(fui::makeRect(r.x + r.width / 2 - 1, r.y + r.height / 2, 3, 5), fui::Paint::solid(fui::Color::White));
}
void shopStats(toybox::Screen& s, Work& w, const quest::Profile& p) {
  const auto r = s.takeTop(42, 8);
  s.target().stroke(r, ink, 2);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL), int(quest::level(p.xp)));
  text(s, fui::makeRect(r.x + 8, r.y + 4, r.width / 2 - 16, r.height - 8), w.label, 0);
  coin(s, fui::makeRect(r.x + r.width / 2 + 8, r.y + 10, 18, 24));
  std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(quest::coins(p)));
  pixelui::text(s.target(), fui::makeRect(r.x + r.width / 2 + 34, r.y + 4, r.width / 2 - 42, r.height - 8), w.label, 0);
}
void hero(toybox::Screen& s, sdart::Pack* assets, fui::Rect area, const quest::Profile& p, int preview = -1) {
  if (assets) questavatar::draw(s.target(), area, p, *assets, preview);
}
void dailyCard(toybox::Screen& s, Work& w, fui::Rect r, const Model& m) {
  const auto& p = *m.profile;
  const unsigned minutes = std::min<uint32_t>(quest::DAILY_MINUTES, m.summary->onDay(m.today) / 60);
  s.target().stroke(r, ink, 3);
  s.target().fill(fui::makeRect(r.x + 2, r.y + 2, r.width - 4, 24), ink);
  pixelui::text(s.target(), fui::makeRect(r.x + 10, r.y + 2, r.width - 20, 24), tr(STR_QUEST_DAILY), 0,
                fui::Color::White);
  const int contentX = r.x + 80, contentWidth = r.width - 210;
  questicons::book(s.target(), fui::makeRect(r.x + 12, r.y + 38, 54, 54));
  pixelui::copyCaps(w.name, sizeof(w.name), tr(STR_QUEST_DAILY_TITLE));
  const int titleFont = s.target().measureText(0, w.name, {}).width <= contentWidth ? 0 : 3;
  text(s, fui::makeRect(contentX, r.y + 30, contentWidth, 26), w.name, titleFont, true);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_PROGRESS_MINUTES), static_cast<unsigned long long>(minutes),
                quest::DAILY_MINUTES);
  text(s, fui::makeRect(contentX, r.y + 56, contentWidth, 20), w.label, 3, true);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CLAIM_REWARD), 60u);
  const int xpWidth = s.target().measureText(3, w.label, {}).width;
  text(s, fui::makeRect(contentX, r.y + 78, xpWidth + 2, 20), w.label, 3, true);
  coin(s, fui::makeRect(contentX + xpWidth + 10, r.y + 80, 12, 17));
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COIN_REWARD), 6u);
  text(s, fui::makeRect(contentX + xpWidth + 28, r.y + 78, contentWidth - xpWidth - 28, 20), w.label, 3, true);
  const bool claimed = m.today <= p.claimedDay;
  const auto claimBox = fui::makeRect(r.right() - 124, r.y + 42, 112, 48);
  if (claimed) {
    s.target().stroke(claimBox, ink, 2);
    s.target().bitmap(fui::makeRect(claimBox.x + claimBox.width / 2 - 8, claimBox.y + 4, 16, 16),
                      fui::bitmapFromIcon(extra_check_32), fui::BitmapMode::Contain);
    text(s, fui::makeRect(claimBox.x + 4, claimBox.y + 24, claimBox.width - 8, 20), tr(STR_QUEST_CLAIMED), 3);
  } else
    button(s, w, claimBox, tr(STR_QUEST_COLLECT), Claim, 0, minutes == quest::DAILY_MINUTES);
  bar(s, fui::makeRect(r.x + 10, r.bottom() - 12, r.width - 20, 7), minutes, quest::DAILY_MINUTES);
}
void genderButton(toybox::Screen& s, fui::Rect r, const quest::Profile& p) {
  const int cx = r.x + r.width / 2, cy = r.y + r.height / 2 - 3;
  s.target().stroke(fui::makeRect(cx - 8, cy - 8, 16, 16), ink, 2, 8);
  if (p.gender == quest::Gender::Feminine) {
    s.target().line({int16_t(cx), int16_t(cy + 7)}, {int16_t(cx), int16_t(cy + 17)}, 2, ink);
    s.target().line({int16_t(cx - 5), int16_t(cy + 13)}, {int16_t(cx + 5), int16_t(cy + 13)}, 2, ink);
  } else {
    s.target().line({int16_t(cx + 6), int16_t(cy - 6)}, {int16_t(cx + 15), int16_t(cy - 15)}, 2, ink);
    s.target().line({int16_t(cx + 7), int16_t(cy - 15)}, {int16_t(cx + 15), int16_t(cy - 15)}, 2, ink);
    s.target().line({int16_t(cx + 15), int16_t(cy - 15)}, {int16_t(cx + 15), int16_t(cy - 7)}, 2, ink);
  }
  s.frame().hit(r, Gender, p.gender == quest::Gender::Masculine ? 1 : 0, fui::InputTouch);
}
void heroPanel(toybox::Screen& s, Work& w, fui::Rect r, const quest::Profile& p, sdart::Pack* assets) {
  const int level = quest::level(p.xp);
  s.target().stroke(r, ink, 3);
  genderButton(s, fui::makeRect(r.right() - 50, r.y + 5, 44, 44), p);
  const auto title = rankTitle(level);
  pixelui::copyCaps(w.label, sizeof(w.label), title);
  const int titleFont = s.target().measureText(1, w.label, {}).width <= r.width - 62 ? 1 : 0;
  const int titleWidth = s.target().measureText(titleFont, w.label, {}).width;
  const int titleRight = std::min(r.x + (r.width + titleWidth) / 2, r.right() - 60);
  text(s, fui::makeRect(titleRight - titleWidth, r.y + 7, titleWidth, 34), title, titleFont);
  const auto values = fui::makeRect(r.x + 20, r.y + 43, r.width - 40, 30);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL), level);
  text(s, fui::makeRect(values.x, values.y, values.width / 2 - 10, values.height), w.label, 0);
  s.target().fill(fui::makeRect(values.x + values.width / 2 - 1, values.y + 5, 2, 20), ink);
  std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(quest::coins(p)));
  const int numberWidth = s.target().measureText(0, w.label, {}).width;
  const int moneyX = values.x + values.width * 3 / 4 - (numberWidth + 28) / 2;
  coin(s, fui::makeRect(moneyX, values.y + 3, 18, 24));
  text(s, fui::makeRect(moneyX + 26, values.y, values.right() - moneyX - 26, values.height), w.label, 0, true);
  const auto picture = fui::makeRect(r.x + 12, r.y + 80, r.width - 24, std::max(1, int(r.height) - 136));
  hero(s, assets, picture, p);
  const bool maximum = level == quest::MAX_LEVEL;
  const auto span =
      maximum ? quest::threshold(quest::MAX_LEVEL) : quest::threshold(level + 1) - quest::threshold(level);
  const auto progress = maximum ? span : p.xp - quest::threshold(level);
  const int expY = r.bottom() - 50;
  text(s, fui::makeRect(r.x + 12, expY, r.width / 2 - 18, 24), tr(STR_QUEST_EXPERIENCE), 3, true);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_XP), static_cast<unsigned long long>(progress),
                static_cast<unsigned long long>(span));
  fui::TextStyle style;
  style.font = 3;
  style.align = fui::TextAlign::Right;
  s.target().text(fui::makeRect(r.x + r.width / 2, expY, r.width / 2 - 12, 24), w.label, style);
  bar(s, fui::makeRect(r.x + 12, r.bottom() - 22, r.width - 24, 13), progress, span);
}
void statsCard(toybox::Screen& s, Work& w, fui::Rect r, unsigned value, const char* label, bool streak) {
  s.target().stroke(r, ink, 3);
  const bool wide = s.device().width > s.device().height;
  const auto icon = fui::makeRect(r.x + 8, r.y + 10, 40, wide ? 36 : r.height - 20);
  if (streak)
    questicons::flame(s.target(), icon);
  else
    questicons::book(s.target(), icon);
  if (streak)
    std::snprintf(w.label, sizeof(w.label), value == 1 ? tr(STR_QUEST_ONE_DAY_VALUE) : tr(STR_QUEST_DAYS_VALUE), value);
  else
    std::snprintf(w.label, sizeof(w.label), "%u", value);
  text(s, fui::makeRect(r.x + 52, r.y + 3, r.width - 58, 31), w.label, 1);
  text(s,
       wide ? fui::makeRect(r.x + 8, r.y + 48, r.width - 16, r.height - 52)
            : fui::makeRect(r.x + 52, r.y + 34, r.width - 58, r.height - 37),
       label, 3);
}
void pager(toybox::Screen& s, Work& w, fui::Rect r, int page, int pages) {
  w.page = page;
  w.pages = pages;
  pageui::build(s, r, page + 1, pages, Page, -1, Page, 1);
}
}  // namespace
const char* rankTitle(uint32_t level) {
  static constexpr StrId titles[] = {
      StrId::STR_QUEST_APPRENTICE,      StrId::STR_QUEST_RANK_CURIOUS,   StrId::STR_QUEST_RANK_EXPLORER,
      StrId::STR_QUEST_RANK_TRAVELER,   StrId::STR_QUEST_BOOKKEEPER,     StrId::STR_QUEST_RANK_SEEKER,
      StrId::STR_QUEST_RANK_CHRONICLER, StrId::STR_QUEST_RANK_SAGE,      StrId::STR_QUEST_RANK_WEAVER,
      StrId::STR_QUEST_RANK_KEEPER,     StrId::STR_QUEST_MASTER,         StrId::STR_QUEST_RANK_SAGA,
      StrId::STR_QUEST_RANK_WORLDS,     StrId::STR_QUEST_RANK_ARCHITECT, StrId::STR_QUEST_RANK_FORGER,
      StrId::STR_QUEST_RANK_SOVEREIGN,  StrId::STR_QUEST_RANK_ORACLE,    StrId::STR_QUEST_RANK_CELESTIAL,
      StrId::STR_QUEST_RANK_LEGEND,     StrId::STR_QUEST_RANK_DEITY,
  };
  static_assert(sizeof(titles) / sizeof(titles[0]) == quest::TITLE_COUNT);
  return I18N.get(titles[(std::clamp<uint32_t>(level, 1, quest::MAX_LEVEL) - 1) / quest::LEVELS_PER_TITLE]);
}
void buildLevelUp(toybox::Screen& s, const quest::Profile& p, bool saveFailed, Work& w) {
  const auto area = s.body();
  const bool wide = area.width > area.height;
  const int width = std::min(int(area.width) - 24, wide ? 520 : 440);
  const int height = std::min(int(area.height) - 6, 310);
  const auto box = fui::makeRect(area.x + (area.width - width) / 2,
                                 wide ? area.y + (area.height - height) / 2 : area.y + 70, width, height);
  const auto paper = fui::Paint::solid(fui::Color::White);
  s.target().fill(fui::makeRect(box.x + 6, box.y + 6, box.width, box.height), ink);
  s.target().fill(box, paper);
  s.target().stroke(box, ink, 3);
  const auto banner = fui::makeRect(box.x + 8, box.y + 8, box.width - 16, 44);
  s.target().fill(banner, ink);
  pixelui::copyCaps(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL_UP));
  const int headerFont = s.target().measureText(1, w.label, {}).width <= banner.width - 12 ? 1 : 0;
  pixelui::text(s.target(), banner.inset(fui::Insets{0, 6, 0, 6}), w.label, headerFont, fui::Color::White,
                fui::TextAlign::Center);
  const int center = box.x + box.width / 2;
  const int emblemY = box.y + 62;
  const int emblemX = center;
  // Pixel stairs keep the arrow and stars on the same grid as the character art.
  for (int row = 0; row < 8; ++row) {
    const int half = (row + 1) * 2;
    s.target().fill(fui::makeRect(emblemX - half, emblemY + row * 2, half * 2, 2), ink);
  }
  s.target().fill(fui::makeRect(emblemX - 8, emblemY + 16, 16, 16), ink);
  for (int row = 2; row < 7; ++row) {
    const int half = (row - 1) * 2;
    s.target().fill(fui::makeRect(emblemX - half, emblemY + row * 2, half * 2, 2), paper);
  }
  s.target().fill(fui::makeRect(emblemX - 4, emblemY + 14, 8, 14), paper);
  for (int direction : {-1, 1}) {
    const int starX = emblemX + direction * 42;
    for (int row = -4; row <= 4; ++row) {
      const int half = 1 + (4 - std::abs(row)) * 2;
      s.target().fill(fui::makeRect(starX - half, emblemY + 16 + row * 2, half * 2, 2), ink);
    }
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL), int(quest::level(p.xp)));
  const auto levelBox = fui::makeRect(box.x + 12, box.y + 98, box.width - 24, 44);
  pixelui::text(s.target(), levelBox, w.label, 2, fui::Color::Black, fui::TextAlign::Center);
  const int titleY = box.y + 146;
  pixelui::copyCaps(w.name, sizeof(w.name), rankTitle(quest::level(p.xp)));
  const int titleFont = s.target().measureText(1, w.name, {}).width <= box.width - 32 ? 1 : 0;
  pixelui::text(s.target(), fui::makeRect(box.x + 16, titleY, box.width - 32, 26), w.name, titleFont, fui::Color::Black,
                fui::TextAlign::Center);
  const int dividerY = box.y + 179;
  s.target().fill(fui::makeRect(box.x + 18, dividerY, box.width - 36, 2), ink);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL_GIFT), quest::pendingLevelCoins(p));
  pixelui::copyCaps(w.label, sizeof(w.label), w.label);
  const int rewardFont = s.target().measureText(1, w.label, {}).width <= box.width - 80 ? 1 : 0;
  const int rewardWidth = s.target().measureText(rewardFont, w.label, {}).width;
  coin(s, fui::makeRect(center - (rewardWidth + 36) / 2, dividerY + 10, 20, 28));
  pixelui::text(s.target(), fui::makeRect(center - rewardWidth / 2 + 18, dividerY + 10, rewardWidth + 4, 28), w.label,
                rewardFont, fui::Color::Black, fui::TextAlign::Center);
  const int messageY = dividerY + 43;
  questicons::book(s.target(), fui::makeRect(box.x + 22, messageY + 2, 34, 34));
  pixelui::text(s.target(), fui::makeRect(box.x + 70, messageY, box.width - 92, 30),
                saveFailed ? tr(STR_TOOLS_SAVE_FAILED) : tr(STR_QUEST_KEEP_READING), 3, fui::Color::Black,
                fui::TextAlign::Center);
  button(s, w, fui::makeRect(box.x + 12, box.bottom() - 54, box.width - 24, 42), tr(STR_QUEST_CONTINUE), LevelContinue,
         0, true);
  const int arrowX = box.right() - 40, arrowY = box.bottom() - 34;
  s.target().fill(fui::makeRect(arrowX - 10, arrowY - 2, 20, 4), paper);
  for (int row = -5; row <= 5; ++row)
    s.target().fill(fui::makeRect(arrowX + 8 - std::abs(row) * 2, arrowY + row * 2, 4, 2), paper);
}
void build(toybox::Screen& s, const Model& m, Work& w) {
  pixelui::begin(s);
  pixelui::Header h;
  h.title = tr(STR_READING_QUEST);
  h.back = true;
  pixelui::header(s.target(), s.device(), h);
  backui::publish(true, pixelui::geometry(s.device()).back);
  if (m.assets && !m.assets->ready()) {
    requiredresources::build(s, "/apps/reading-adventure/", Back, w.button);
    return;
  }
  static constexpr StrId names[] = {StrId::STR_QUEST_HERO, StrId::STR_QUEST_QUESTS, StrId::STR_QUEST_GEAR};
  for (int n = 0; n < 3; ++n) {
    pixelui::copyCaps(w.tabLabels[n], sizeof(w.tabLabels[n]), I18N.get(names[n]));
    w.tabs[n] = fui::tabItem(n, m.tab == n, true, w.tabLabels[n]);
  }
  const bool landscape = s.device().width > s.device().height;
  pixelui::tabs(s, w.tabs, 3, Tab, 0, landscape ? 44 : 52, landscape ? 6 : 10);
  auto body = s.body();
  if (m.loading) {
    text(s, body, tr(STR_READING_LOADING));
    return;
  }
  if (!m.profile || !m.summary) return;
  const auto& p = *m.profile;
  const auto& stats = *m.summary;
  if ((m.failed || m.denied) && m.tab != 1) {
    text(s, s.takeTop(34, 6),
         m.failed     ? tr(STR_TOOLS_SAVE_FAILED)
         : m.tab == 1 ? tr(STR_QUEST_TARGET_REQUIRED)
                      : tr(STR_QUEST_LOCKED),
         3);
    body = s.body();
  }
  const int level = quest::level(p.xp);
  if (m.tab == 0) {
    const bool wide = body.width > body.height;
    const int gap = 8;
    const auto heroBox = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 4, body.height)
                              : fui::makeRect(body.x, body.y, body.width, body.height - 202);
    heroPanel(s, w, heroBox, p, m.assets);
    const auto info =
        wide ? fui::makeRect(heroBox.right() + gap, body.y, body.right() - heroBox.right() - gap, body.height)
             : fui::makeRect(body.x, heroBox.bottom() + gap, body.width, 194);
    const int half = (info.width - gap) / 2;
    const int statsHeight = wide ? 88 : 64;
    const int infoTop = wide ? info.y + std::max(0, (int(info.height) - statsHeight - 130) / 2) : info.y;
    statsCard(s, w, fui::makeRect(info.x, infoTop, half, statsHeight), stats.currentStreak(m.today),
              tr(STR_QUEST_CURRENT_STREAK), true);
    statsCard(s, w, fui::makeRect(info.x + half + gap, infoTop, info.width - half - gap, statsHeight), stats.completed,
              tr(STR_QUEST_BOOKS_LABEL), false);
    dailyCard(s, w, fui::makeRect(info.x, infoTop + statsHeight + gap, info.width, 122), m);
    return;
  }
  const auto pages = s.takeBottom(m.tab == 1 ? 66 : 44, 8);
  if (m.tab == 1) {
    dailyCard(s, w, s.takeTop(118, 10), m);
    const auto heading = s.takeTop(30, 6);
    s.target().fill(heading, ink);
    const auto headingLabel = m.failed   ? tr(STR_TOOLS_SAVE_FAILED)
                              : m.denied ? tr(STR_QUEST_TARGET_REQUIRED)
                                         : tr(STR_QUEST_READING_MISSIONS);
    pixelui::text(s.target(), heading.inset(fui::Insets{0, 12, 0, 12}), headingLabel, m.failed || m.denied ? 3 : 0,
                  fui::Color::White);
    body = s.body();
    constexpr int wanted = 4;
    const int capacity = std::max(1, std::min(wanted, (int(body.height) + 8) / 82));
    const int pageCount = (quest::kQuestCount + capacity - 1) / capacity,
              page = std::clamp(m.questPage, 0, pageCount - 1);
    pager(s, w, pages, page, pageCount);
    static constexpr StrId metric[] = {StrId::STR_QUEST_MINUTES_TARGET,  StrId::STR_QUEST_PAGES_TARGET,
                                       StrId::STR_QUEST_SESSIONS_TARGET, StrId::STR_QUEST_DAYS_TARGET,
                                       StrId::STR_QUEST_BOOKS_TARGET,    StrId::STR_QUEST_STREAK_TARGET};
    static constexpr StrId units[] = {StrId::STR_QUEST_PROGRESS_MINUTES,  StrId::STR_QUEST_PROGRESS_PAGES,
                                      StrId::STR_QUEST_PROGRESS_SESSIONS, StrId::STR_QUEST_PROGRESS_DAYS,
                                      StrId::STR_QUEST_PROGRESS_BOOKS,    StrId::STR_QUEST_PROGRESS_DAYS};
    static constexpr const freeink::Icon* icons[] = {&extra_clock_64,        &extra_reading_book_64,
                                                     &extra_calendar_64,     &extra_calendar_64,
                                                     &extra_reading_book_64, &extra_reading_flame_64};
    const int rowHeight = (body.height - (capacity - 1) * 8) / capacity;
    for (int n = 0; n < capacity && page * capacity + n < quest::kQuestCount; ++n) {
      const int q = quest::orderedQuest(p, stats, page * capacity + n);
      const auto row = fui::makeRect(body.x, body.y + n * (rowHeight + 8), body.width, rowHeight);
      const int contentY = row.y + std::max(2, (row.height - 74) / 2);
      const uint32_t target = quest::questTarget(q), reward = quest::questReward(q);
      const uint64_t value = std::min<uint64_t>(target, quest::questValue(q, stats));
      const bool done = value >= target, claimed = quest::isClaimed(p, q);
      s.target().stroke(row, ink, 2);
      const int iconSize = std::min(48, int(row.height) - 20), contentX = row.x + 66, contentWidth = row.width - 186;
      s.target().bitmap(fui::makeRect(row.x + 8, row.y + (row.height - iconSize) / 2, iconSize, iconSize),
                        fui::bitmapFromIcon(*icons[q % 6]), fui::BitmapMode::Contain);
      const StrId titleKey = target == 1 && q % 6 == 4 ? StrId::STR_QUEST_ONE_BOOK_TARGET : metric[q % 6];
      std::snprintf(w.label, sizeof(w.label), I18N.get(titleKey), target);
      pixelui::copyCaps(w.name, sizeof(w.name), w.label);
      const int font = s.target().measureText(0, w.name, {}).width <= contentWidth ? 0 : 3;
      text(s, fui::makeRect(contentX, contentY, contentWidth, 25), w.label, font, true);
      std::snprintf(w.label, sizeof(w.label),
                    I18N.get(target == 1 && q % 6 == 4 ? StrId::STR_QUEST_ONE_BOOK_PROGRESS : units[q % 6]),
                    static_cast<unsigned long long>(value), target);
      text(s, fui::makeRect(contentX, contentY + 25, contentWidth, 18), w.label, 3, true);
      bar(s, fui::makeRect(contentX, contentY + 45, contentWidth, 6), value, target);
      text(s, fui::makeRect(contentX + (done ? 20 : 0), contentY + 52, contentWidth - (done ? 20 : 0), 18),
           claimed ? tr(STR_QUEST_CLAIMED)
           : done  ? tr(STR_QUEST_COMPLETED)
                   : tr(STR_QUEST_IN_PROGRESS),
           3, true);
      if (done)
        s.target().bitmap(fui::makeRect(contentX, contentY + 53, 16, 16), fui::bitmapFromIcon(extra_check_32),
                          fui::BitmapMode::Contain);
      const auto rewards = fui::makeRect(row.right() - 110, row.y + (row.height - 54) / 2, 102, 54);
      s.target().stroke(rewards, ink, done && !claimed ? 3 : 2);
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CLAIM_REWARD), reward);
      text(s, fui::makeRect(rewards.x + 4, rewards.y + 3, rewards.width - 8, 23), w.label, 3);
      coin(s, fui::makeRect(rewards.x + 26, rewards.y + 30, 12, 17));
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COIN_REWARD), quest::questCoinReward(q));
      text(s, fui::makeRect(rewards.x + 44, rewards.y + 27, rewards.width - 48, 23), w.label, 3, true);
      if (!claimed) s.frame().hit(rewards, Milestone, q, fui::InputTouch);
    }
    return;
  }
  shopStats(s, w, p);
  if (m.preview >= 0 && m.preview < quest::kGearCount) {
    button(s, w, pages, tr(STR_QUEST_BACK), Back);
    itemName(w, m.preview);
    text(s, s.takeTop(36, 4), w.name);
    const auto controls = s.takeBottom(92, 8);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_PRICE), quest::price(m.preview),
                  quest::requiredLevel(m.preview));
    text(s, fui::makeRect(controls.x, controls.y, controls.width, 32), w.label, 3);
    const bool equipped = p.equipped[int(quest::slot(m.preview))] == m.preview;
    button(s, w, fui::makeRect(controls.x, controls.y + 40, controls.width, 48),
           equipped && (quest::slot(m.preview) == quest::Slot::Aura || quest::slot(m.preview) == quest::Slot::Pet)
               ? tr(STR_QUEST_UNEQUIP)
           : equipped                  ? tr(STR_QUEST_EQUIPPED)
           : quest::owns(p, m.preview) ? tr(STR_QUEST_EQUIP)
                                       : tr(STR_QUEST_BUY),
           Outfit, m.preview, equipped);
    sprite(s, m.assets, m.preview, s.body());
    return;
  }
  for (int n = 0; n < quest::kSlotCount; ++n) {
    pixelui::copyCaps(w.tabLabels[n], sizeof(w.tabLabels[n]), I18N.get(categories[n]));
    w.tabs[n] = fui::tabItem(n, m.category == n, true, w.tabLabels[n]);
  }
  pixelui::tabs(s, w.tabs, quest::kSlotCount, Category, 0, landscape ? 44 : 52, landscape ? 6 : 10);
  {
    const auto band = s.takeTop(44, 8);
    const int half = (band.width - 8) / 2;
    button(s, w, fui::makeRect(band.x, band.y, half, 44), tr(STR_QUEST_GEAR_SHOP), InventoryView, 0, !m.ownedGear);
    button(s, w, fui::makeRect(band.x + half + 8, band.y, half, 44), tr(STR_QUEST_MY_GEAR), InventoryView, 1,
           m.ownedGear);
  }
  body = s.body();
  const bool wide = s.device().width > s.device().height;
  const int columns = wide ? 3 : 2;
  const int rows = wide ? 1 : 3, capacity = rows * columns;
  const auto category = quest::Slot(std::clamp(m.category, 0, quest::kSlotCount - 1));
  const int count = m.ownedGear ? quest::ownedCount(p, category) : quest::shopCount(p, category);
  const int pageCount = std::max(1, (count + capacity - 1) / capacity), page = std::clamp(m.page, 0, pageCount - 1);
  pager(s, w, pages, page, pageCount);
  if (!count) {
    text(s, body, m.ownedGear ? tr(STR_QUEST_NO_GEAR) : tr(STR_QUEST_SHOP_EMPTY), 3);
    return;
  }
  const int width = (body.width - (columns - 1) * 10) / columns, height = (body.height - (rows - 1) * 8) / rows;
  for (int n = 0; n < capacity && page * capacity + n < count; ++n) {
    const int item = m.ownedGear ? quest::ownedItem(p, category, page * capacity + n)
                                 : quest::shopItem(p, category, page * capacity + n);
    const auto box =
        fui::makeRect(body.x + (n % columns) * (width + 10), body.y + (n / columns) * (height + 8), width, height);
    const bool equipped = p.equipped[int(category)] == item,
               locked = !quest::owns(p, item) && level < int(quest::requiredLevel(item));
    s.target().stroke(box, ink, equipped ? 4 : 2);
    const int artWidth = std::min(86, box.width * 40 / 100);
    sprite(s, m.assets, item, fui::makeRect(box.x + 6, box.y + 8, artWidth - 8, box.height - 16));
    const auto info = fui::makeRect(box.x + artWidth + 8, box.y + 6, box.width - artWidth - 16, box.height - 12);
    itemName(w, item);
    const bool compact = info.height < 116;
    const int titleHeight = 2 * s.target().lineHeight(3);
    text(s, fui::makeRect(info.x, info.y, info.width, titleHeight), w.name, 3, true);
    const int priceY = info.y + titleHeight + 2;
    coin(s, fui::makeRect(info.x, priceY + 3, 16, 20));
    std::snprintf(w.label, sizeof(w.label), "%u", quest::price(item));
    text(s, fui::makeRect(info.x + 23, priceY, (compact ? info.width / 2 : info.width) - 23, 25), w.label, 3, true);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_ITEM_LEVEL), quest::requiredLevel(item));
    const int levelX = compact ? info.x + info.width / 2 : info.x, levelY = compact ? priceY : priceY + 26;
    text(s, fui::makeRect(levelX, levelY, (compact ? info.width - info.width / 2 : info.width) - (locked ? 22 : 0), 22),
         w.label, 3, true);
    if (locked) lockMark(s, fui::makeRect(info.right() - 17, levelY + 2, 15, 18));
    const auto status = fui::makeRect(info.x, info.bottom() - 21, info.width, 21);
    if (equipped) {
      s.target().fill(status, ink);
      pixelui::text(s.target(), status, tr(STR_QUEST_EQUIPPED), 3, fui::Color::White, fui::TextAlign::Center);
    } else if (!locked)
      text(s, status, quest::owns(p, item) ? tr(STR_QUEST_EQUIP) : tr(STR_QUEST_AVAILABLE), 3);
    s.frame().hit(box, Preview, item, fui::InputTouch);
  }
}
}  // namespace questui
