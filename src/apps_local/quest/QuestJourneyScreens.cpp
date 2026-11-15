#include "QuestJourneyScreens.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/PageIndicator.h"
#include "QuestActionIcons.h"
#include "QuestAvatar.h"
#include "QuestCampaignStory.h"
#include "QuestCatalog.h"
#include "QuestEnemyArt.h"
#include "QuestExclusiveHuntBounds.h"
#include "QuestExpansionText.h"
#include "QuestJourneyData.h"
#include "QuestKeepsakeArt.h"
#include "QuestMemoryArt.h"
#include "QuestPortraits.h"
#include "QuestReadingReturnArt.h"
#include "QuestRequestData.h"
#include "QuestRequestUi.h"
#include "QuestRequests.h"
#include "QuestSagaCovers.h"
#include "QuestSceneData.h"
#include "QuestWidgets.h"

namespace questui {
namespace fui = freeink::ui;
namespace {
using namespace widget;
constexpr const char* ROMANS[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII"};

void heading(toybox::Screen& s, fui::Rect& r, const char* title) {
  ornamentTitle(s, rect(r.x, r.y, r.width, 38), title);
  r.y += 48;
  r.height -= 48;
}

void victoryDate(Work& w, const quest::HuntVictory& win) {
  if (!win.day) {
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_COLLECTION_UNKNOWN_DATE));
    return;
  }
  const auto date = readingstats::dateFromDay(int(win.day));
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_DATE), unsigned(date.day), unsigned(date.month),
                unsigned(date.year));
}
void victoryReading(Work& w, const quest::HuntVictory& win) {
  if (!win.seconds)
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_COLLECTION_UNKNOWN_READING));
  else if (win.seconds >= 3600)
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_HOURS), win.seconds / 3600,
                  win.seconds % 3600 / 60);
  else
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_MINUTES),
                  unsigned((uint64_t(win.seconds) + 59) / 60));
}
void victoryCard(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, int id, const quest::HuntVictory& win,
                 int action, int value);
void rewardLink(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, int id, bool enabled = true) {
  const bool obtained = quest::requestClaimed(*m.profile, id);
  if (obtained)
    panel(s, box);
  else
    button(s, w, box, "", ShowHuntRewards, 0, false, enabled);
  icon(s, rect(box.x + 12, box.y + (box.height - 30) / 2, 30, 30), extra_trophy_32);
  text(s, rect(box.x + 56, box.y + 4, box.width - 92, box.height - 8),
       obtained ? tr(STR_QUEST_HUNT_REWARDS_OBTAINED) : tr(STR_QUEST_HUNT_REWARDS), HUNT_LABEL_FONT, true);
  if (obtained)
    icon(s, rect(box.right() - 34, box.y + (box.height - 24) / 2, 24, 24), extra_checked_32);
  else
    questicons::chevron(s.target(), rect(box.right() - 25, box.y + (box.height - 18) / 2, 14, 18));
}

void atlas(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  heading(s, r, tr(STR_QUEST_CAM_ATLAS_TITLE));
  text(s, rect(r.x + 4, r.y, r.width - 8, 42), tr(STR_QUEST_COLLECTION_ATLAS_HINT), 3, false, fui::TextAlign::Left,
       false, 2);
  r.y += 50;
  r.height -= 50;
  const bool wide = s.device().width > s.device().height;
  const int capacity = atlasPageSize(wide), pages = 12 / capacity, page = std::clamp(m.page, 0, pages - 1);
  const auto area = rect(r.x, r.y, r.width, r.height - 84);
  const int cols = wide ? 4 : 2;
  const int rows = (capacity + cols - 1) / cols;
  const int width = (area.width - (cols - 1) * GAP) / cols, height = (area.height - (rows - 1) * GAP) / rows;
  const auto completed = questjourney::rememberedChapters(p);
  for (int n = 0; n < capacity; ++n) {
    const unsigned book = page * capacity + n;
    const bool open = p.campaign.prologueSeen && book < p.campaign.introducedBooks;
    const unsigned recovered = std::min(10u, completed > book * 10 ? completed - book * 10 : 0u);
    const auto box = rect(area.x + n % cols * (width + GAP), area.y + n / cols * (height + GAP), width, height);
    const bool inverse = button(s, w, box, "", AtlasBook, int(book), false, open);
    const auto picture = rect(box.x + 8, box.y + 8, box.width - 16, box.height - 92);
    s.target().fill(picture, paper, 4);
    if (open)
      questcover::draw(s.target(), picture, book);
    else
      icon(s, rect(picture.x + (picture.width - 48) / 2, picture.y + (picture.height - 48) / 2, 48, 48), extra_lock_32);
    statusPill(s, rect(box.x + 10, box.y + 10, 54, 26), ROMANS[book], true, nullptr, 3);
    text(s, rect(box.x + 10, box.bottom() - 80, box.width - 20, 48),
         open ? I18N.get(queststory::BOOKS[book]) : tr(STR_QUEST_CAM_ATLAS_HIDDEN), 3, false, fui::TextAlign::Center,
         inverse, 2);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_ATLAS_REGION), recovered);
    text(s, rect(box.x + 10, box.bottom() - 32, box.width - 20, 24),
         open ? w.label : tr(STR_QUEST_CAM_ATLAS_LOCKED_NOTE), 3, false, fui::TextAlign::Center, inverse);
  }
  w.page = page;
  w.pages = pages;
  pagination(s, w, rect(r.x, r.bottom() - 78, r.width, 44), page + 1, pages);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_ATLAS_TOTAL), completed);
  text(s, rect(r.x, r.bottom() - 26, r.width, 24), w.label, 3, false, fui::TextAlign::Center);
}
void region(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const unsigned book = std::clamp(m.selectedBook, 0, int(quest::book(p)));
  heading(s, r, I18N.get(queststory::BOOKS[book]));
  const bool wide = s.device().width > s.device().height;
  const int recovered = std::clamp(int(questjourney::rememberedChapters(p)) - int(book) * 10, 0, 10);
  const int capacity = atlasRegionPageSize(wide), pages = std::max(1, (recovered + capacity - 1) / capacity),
            page = std::clamp(m.page, 0, pages - 1);
  const auto area = rect(r.x, r.y, r.width, r.height - 58);
  const int columns = wide ? 4 : 2, rows = wide ? 1 : 2;
  const int width = (area.width - (columns - 1) * GAP) / columns, height = (area.height - (rows - 1) * GAP) / rows;
  if (!recovered) {
    questcover::draw(s.target(), rect(area.x, area.y, area.width, area.height / 2), book);
    text(s, rect(area.x + 12, area.y + area.height / 2 + 12, area.width - 24, area.height / 2 - 12),
         tr(STR_QUEST_COLLECTION_ATLAS_EMPTY), 0, false, fui::TextAlign::Center, false, 3);
  }
  for (int n = 0; n < capacity && page * capacity + n < recovered; ++n) {
    const unsigned chapter = book * 10 + page * capacity + n;
    const auto box = rect(area.x + n % columns * (width + GAP), area.y + n / columns * (height + GAP), width, height);
    victoryCard(s, m, w, box, quest::huntForPath(chapter), p.campaignVictories[chapter], AtlasChapter, int(chapter));
  }
  w.page = page;
  w.pages = pages;
  pagination(s, w, rect(r.x, r.bottom() - 48, r.width, 44), page + 1, pages);
}

void prosePage(toybox::Screen& s, Work& w, fui::Rect area, const char* value, int page, const char* second = nullptr,
               const char* third = nullptr, int font = 4) {
  fui::TextStyle style;
  style.font = font;
  const int lineHeight = std::max(1, int(s.target().lineHeight(style.font))),
            capacity = std::max(1, int(area.height) / lineHeight);
  const char* paragraphs[] = {value, second, third};
  int lines = 0;
  for (const auto paragraph : paragraphs)
    if (paragraph) {
      if (lines) ++lines;
      lines += fui::textAreaWalk(s.target(), area.width, paragraph, style, [](uint32_t, const fui::TextAreaLine&) {});
    }
  w.pages = std::max(1, (lines + capacity - 1) / capacity);
  w.page = std::clamp(page, 0, w.pages - 1);
  int line = 0;
  for (const auto paragraph : paragraphs)
    if (paragraph) {
      if (line) ++line;
      fui::textAreaWalk(s.target(), area.width, paragraph, style, [&](uint32_t, const fui::TextAreaLine& run) {
        const int y = line++ - w.page * capacity;
        if (y < 0 || y >= capacity) return;
        const auto bytes = std::min<size_t>(run.len, sizeof(w.storyLine) - 1);
        std::memcpy(w.storyLine, paragraph + run.start, bytes);
        w.storyLine[bytes] = 0;
        s.target().text(rect(area.x, area.y + y * lineHeight, area.width, lineHeight), w.storyLine, style);
      });
    }
}

void recap(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const auto context = questjourney::context(p);
  heading(s, r, tr(STR_QUEST_CAM_RECAP));
  const bool wide = s.device().width > s.device().height;
  const auto action = rect(r.x, r.bottom() - 54, r.width, 54);
  const auto picture = wide ? rect(r.x, r.y, r.width * 45 / 100, action.y - r.y - 12)
                            : rect(r.x, r.y, r.width, std::min(265, int(r.height) * 42 / 100));
  panel(s, picture);
  const unsigned chapter = context.prologue ? 0 : context.chapter;
  questmemory::draw(s.target(), picture.inset({8, 8, 8, 8}), p, m.assets, chapter,
                    context.beat == 3 ? questenemy::Pose::Defeated : questenemy::Pose::Ready, m.enemyAssets,
                    m.rewardAssets, m.outfitAssets);
  const auto copy = wide ? rect(picture.right() + GAP, r.y, r.right() - picture.right() - GAP, action.y - r.y - 12)
                         : rect(r.x + 6, picture.bottom() + 14, r.width - 12, action.y - picture.bottom() - 26);
  const char* words = context.prologue ? tr(STR_QUEST_CAM_JOURNEY2_RECAP_PROLOGUE)
                      : context.beat   ? tr(STR_QUEST_CAM_ORDER_CLEAR_PATH)
                                       : I18N.get(questscene::APPROACH[context.chapter]);
  prosePage(s, w, rect(copy.x, copy.y, copy.width, copy.height - 116), words, m.introductionPage);
  const auto next = rect(copy.x, copy.bottom() - 108, copy.width, 62);
  text(s, rect(next.x, next.y, next.width, 22), tr(STR_QUEST_CAM_RECAP_NEXT), 3, true);
  text(s, rect(next.x, next.y + 25, next.width, 36),
       quest::needsIntroduction(p) ? tr(STR_QUEST_CAM_CHOOSE_SAGA) : journeyTitle(p.campaign.encounter), 3, false,
       fui::TextAlign::Left, false, 2);
  button(s, w, rect(copy.x, copy.bottom() - 42, copy.width, 40), tr(STR_QUEST_CAM_RECAP_READ), RecallStory);
  button(s, w, action, w.page + 1 < w.pages ? tr(STR_QUEST_CAM_NEXT) : tr(STR_QUEST_CAM_RECAP_RESUME),
         w.page + 1 < w.pages ? IntroductionPage : RecapContinue, 1, true);
}
enum class HuntScene : uint8_t { Creature, Preview, Encounter };
void huntPicture(toybox::Screen& s, const Model& m, fui::Rect area, int id, questenemy::Pose pose, HuntScene scene) {
  if (!m.assets) return;
  const int encounter = int(quest::huntEncounter(id));
  if (scene == HuntScene::Preview) {
    const auto backdrop = questavatar::storyBackground(s.target(), area, *m.assets,
                                                       questassets::SAGA_SCENES + unsigned(encounter) / 60 * 2,
                                                       questavatar::Framing::Cover);
    area = rect(backdrop.x + backdrop.width * 19 / 100, backdrop.y + backdrop.height / 5, backdrop.width * 62 / 100,
                backdrop.height * 76 / 100);
  }
  const bool battle = scene == HuntScene::Encounter;
  if (quest::specialHunt(id)) {
    const unsigned index = unsigned(id - quest::kCampaignHuntCount) * 3 + unsigned(pose);
    const bool expanded = quest::bestiaryHunt(id);
    const unsigned frame = expanded ? unsigned(id - quest::kBestiaryHuntStart) * 3 + unsigned(pose) : index;
    const questextra::Sprite art{m.rewardAssets,
                                 (expanded ? questassets::BESTIARY_BEASTS : questassets::HUNT_BEASTS) + frame,
                                 (expanded ? questassets::BESTIARY_MASKS : questassets::HUNT_BEAST_MASKS) + frame};
    if (battle)
      questenemy::battle(s.target(), area, *m.assets, *m.profile, pose, questavatar::Framing::Cover, m.enemyAssets,
                         m.rewardAssets, encounter, &art, m.outfitAssets);
    else {
      const auto& bounds = questhuntart::INK[index];
      const auto prior = s.target().clipRect();
      const int scale = std::min(area.width * 1024 / bounds.width, area.height * 1024 / bounds.height);
      const int width = 248 * scale / 1024, height = 280 * scale / 1024;
      const int left = area.x + (area.width - bounds.width * scale / 1024) / 2 - bounds.x * scale / 1024;
      const int top = area.y + (area.height - bounds.height * scale / 1024) / 2 - bounds.y * scale / 1024;
      const auto clipped = rect(std::max(area.x, prior.x), std::max(area.y, prior.y),
                                std::max(0, std::min(area.right(), prior.right()) - std::max(area.x, prior.x)),
                                std::max(0, std::min(area.bottom(), prior.bottom()) - std::max(area.y, prior.y)));
      if (!clipped.empty() && s.target().setClipRect(clipped)) {
        questextra::paint(s.target(), rect(left, top, width, height), art);
        s.target().setClipRect(prior);
      }
    }
    return;
  }
  if (battle)
    questenemy::battle(s.target(), area, *m.assets, *m.profile, pose, questavatar::Framing::Cover, m.enemyAssets,
                       m.rewardAssets, encounter, nullptr, m.outfitAssets);
  else
    questenemy::thumbnail(s.target(), area, *m.assets, m.enemyAssets, unsigned(encounter), pose);
}
void requestBenefits(toybox::Screen&, const Model&, Work&, fui::Rect, int, const quest::HuntVictory* earned = nullptr);
void rankBadge(toybox::Screen& s, fui::Rect card, int id) {
  const auto rank = quest::huntRank(id);
  const char* label = requestui::rank(id);
  fui::TextStyle style;
  style.font = 3;
  style.bold = true;
  const int width = s.target().measureText(3, label, style).width + 18;
  statusPill(s, rect(card.x + 10, card.y + 9, width, 25), label,
             rank == quest::HuntRank::Legendary || rank == quest::HuntRank::Secret, nullptr, 3);
  const int stars = int(rank) + 1;
  const int available = card.width - width - 31;
  const int side = std::clamp(available / stars - 2, 12, 24);
  for (int n = 0; n < stars; ++n)
    questicons::star(s.target(), rect(card.x + width + 16 + n * (side + 2), card.y + 10, side, 24), true, false, true);
}

void victoryCard(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, int id, const quest::HuntVictory& win,
                 int action, int value) {
  const bool focused = button(s, w, box, "", action, value);
  if (focused) s.target().fill(box.inset({2, 2, 2, 2}), paper, RADIUS - 2);
  const int nameHeight = 2 * s.target().lineHeight(0), footer = 66;
  const int nameY = box.bottom() - footer - nameHeight - 9;
  const auto art = rect(box.x + 12, box.y + 35, box.width - 24, std::max(0, nameY - box.y - 41));
  huntPicture(s, m, art, id, questenemy::Pose::Ready, HuntScene::Creature);
  if (action == AtlasChapter) {
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_PATH), unsigned(value % 10 + 1));
    text(s, rect(box.x + 12, box.y + 6, box.width - 48, 25), w.label, 3, true);
  } else {
    rankBadge(s, box, id);
  }
  questicons::chevron(s.target(), rect(box.right() - 19, box.y + 12, 9, 14));
  text(s, rect(box.x + 9, nameY, box.width - 18, nameHeight), huntTitle(id), 0, true, fui::TextAlign::Center, false, 2);
  const int y = box.bottom() - footer + 2;
  victoryDate(w, win);
  icon(s, rect(box.x + 12, y, 22, 22), extra_calendar_32);
  text(s, rect(box.x + 42, y, box.width - 52, 25), w.label, 3);
  victoryReading(w, win);
  icon(s, rect(box.x + 12, y + 28, 22, 22), extra_clock_32);
  text(s, rect(box.x + 42, y + 28, box.width - 52, 26), w.label, 0, true);

  if (focused) s.target().stroke(box.inset({4, 4, 4, 4}), ink, 2, RADIUS - 4);
}
void victoryStats(toybox::Screen& s, Work& w, fui::Rect area, const quest::HuntVictory& win) {
  const int count = win.books != UINT32_MAX && win.books ? 3 : 2;
  const int width = (area.width - (count - 1) * GAP) / count;
  for (int n = 0; n < count; ++n) {
    const auto card = rect(area.x + n * (width + GAP), area.y,
                           n + 1 == count ? area.right() - area.x - n * (width + GAP) : width, 72);
    panel(s, card);
    icon(s, rect(card.x + 8, card.y + 8, 23, 23),
         n == 0   ? extra_calendar_32
         : n == 1 ? extra_clock_32
                  : extra_reading_book_32);
    text(s, rect(card.x + 38, card.y + 7, card.width - 44, 24),
         n == 0   ? tr(STR_QUEST_COLLECTION_DATE_LABEL)
         : n == 1 ? tr(STR_QUEST_COLLECTION_READING_LABEL)
                  : tr(STR_QUEST_COLLECTION_BOOKS_LABEL),
         3);
    if (n == 0)
      victoryDate(w, win);
    else if (n == 1)
      victoryReading(w, win);
    else
      std::snprintf(w.label, sizeof(w.label), "%u", win.books);
    text(s, rect(card.x + 6, card.y + 35, card.width - 12, 30), w.label, count == 3 && n < 2 ? 3 : 0, true,
         fui::TextAlign::Center);
  }
}

const char* campaignGiftName(int item) {
  if (item >= quest::kExpandedItemStart && item <= quest::kKingDecoration)
    return I18N.get(questexp::DECOR_NAMES[item - quest::kExpandedItemStart]);
  if (item == quest::kKingBody) return tr(STR_QUEST_CAM_EXP_CROWN_BODY);
  if (item == quest::kKingBackground) return tr(STR_QUEST_CAM_EXP_CROWN_BACKGROUND);
  if (item == quest::kKingPet) return tr(STR_QUEST_CAM_EXP_CROWN_PET);
  const int index = item - quest::first(quest::slot(item));
  switch (quest::slot(item)) {
    case quest::Slot::Body:
      return I18N.get(questcatalog::body[index]);
    case quest::Slot::Background:
      return I18N.get(questcatalog::background[index]);
    case quest::Slot::Pet:
      return I18N.get(questcatalog::pet[index]);
    case quest::Slot::Decoration:
      return I18N.get(queststory::DECOR_NAMES[index]);
  }
  return "";
}
void campaignGifts(toybox::Screen& s, const Model& m, fui::Rect box, const quest::CampaignRewards& gifts) {
  panel(s, box);
  const int columns = gifts.count > 2 ? 2 : 1, rowHeight = 80, width = box.width / columns;
  for (unsigned n = 0; n < gifts.count; ++n) {
    const int item = gifts.items[n], column = n % columns, row = n / columns;
    const auto cell = rect(box.x + column * width, box.y + row * rowHeight,
                           column + 1 == columns ? box.width - column * width : width, rowHeight);
    const auto art = rect(cell.x + 10, cell.y + 6, 48, 52);
    if (m.assets) {
      const auto slot = quest::slot(item);
      if (slot == quest::Slot::Body)
        questextra::paint(s.target(), art,
                          questextra::figure(*m.assets, m.rewardAssets, item, m.profile->gender, m.outfitAssets));
      else if (slot == quest::Slot::Pet)
        questextra::paint(s.target(), art, questextra::pet(*m.assets, m.rewardAssets, item));
      else if (slot == quest::Slot::Decoration)
        questextra::paint(s.target(), art, questextra::decoration(*m.assets, m.rewardAssets, item));
      else if (item == quest::kKingBackground) {
        if (m.rewardAssets)
          m.rewardAssets->draw(s.target(), art, questassets::KING_BACKGROUND_ICON, fui::BitmapMode::Contain);
      } else
        m.assets->draw(s.target(), art, questassets::BACKGROUND_ICONS + item - quest::first(slot),
                       fui::BitmapMode::Contain);
    }
    text(s, rect(cell.x + 66, cell.y + 5, cell.width - 76, 72), campaignGiftName(item), columns == 2 ? 4 : 0, true,
         fui::TextAlign::Left, false, columns == 2 ? 3 : 2);
    if (row)
      s.target().line({int16_t(cell.x + 10), int16_t(cell.y)}, {int16_t(cell.right() - 10), int16_t(cell.y)}, 1, ink);
    if (column)
      s.target().line({int16_t(cell.x), int16_t(cell.y + 8)}, {int16_t(cell.x), int16_t(cell.bottom() - 8)}, 1, ink);
  }
}

void victoryHistory(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, int id, const quest::HuntVictory& win,
                    int chapter = -1) {
  heading(s, r, huntTitle(id));
  const bool wide = s.device().width > s.device().height;
  const bool numbers = win.xp || win.coins;
  const int artWidth = wide ? r.width * 48 / 100 : r.width;
  const auto side = wide ? rect(r.x + artWidth + GAP, r.y, r.width - artWidth - GAP, r.height) : r;
  const auto action = rect(side.x, side.bottom() - 48, side.width, 48);
  const auto gifts = chapter < 0                                ? quest::CampaignRewards{}
                     : quest::lastPathInSaga(unsigned(chapter)) ? quest::campaignRewards(unsigned(chapter))
                                                                : quest::campaignUnlocks(unsigned(chapter));
  const int columns = gifts.count > 2 ? 2 : 1;
  const int rewardHeight = chapter < 0 ? 52 : (gifts.count + columns - 1) / columns * 80;
  const int statsHeight = 72;
  const auto gains = rect(side.x, action.y - 44, side.width, 32);
  const int rewardsBottom = (numbers ? gains.y : action.y) - GAP;
  const auto rewards = rect(side.x, rewardsBottom - rewardHeight, side.width, rewardHeight);
  const int statsY = rewards.y - GAP - statsHeight - (chapter >= 0 && rewardHeight ? 30 : 0);
  const auto picture = wide ? rect(r.x, r.y, artWidth, r.height - statsHeight - GAP)
                            : rect(r.x, r.y, r.width, std::max(0, statsY - r.y - GAP));
  panel(s, picture);
  huntPicture(s, m, picture.inset({6, 6, 6, 6}), id, questenemy::Pose::Ready, HuntScene::Preview);
  statusPill(s, rect(picture.x + 10, picture.y + 10, 124, 25), tr(STR_QUEST_COLLECTION_VICTORY), true, &extra_check_32,
             3);
  if (chapter < 0) {
    std::snprintf(w.label, sizeof(w.label), "×%u", quest::huntWinCount(*m.profile, id));
    statusPill(s, rect(picture.right() - 96, picture.y + 10, 84, 25), w.label, false, nullptr, 3);
  }
  const int statsTop = wide ? picture.bottom() + GAP : statsY;
  victoryStats(s, w, rect(wide ? r.x : side.x, statsTop, wide ? artWidth : side.width, statsHeight), win);
  if (rewardHeight) {
    if (chapter >= 0)
      text(s, rect(side.x + 4, rewards.y - 29, side.width - 8, 24),
           chapter >= 0 && !quest::lastPathInSaga(unsigned(chapter)) ? tr(STR_QUEST_VICTORY_SHOP_UNLOCK)
                                                                     : tr(STR_QUEST_COLLECTION_REWARDS),
           3, true);
    if (chapter < 0)
      rewardLink(s, m, w, rewards, id);
    else
      campaignGifts(s, m, rewards, gifts);
  }
  if (numbers) requestBenefits(s, m, w, gains, id, &win);
  if (chapter < 0) {
    const int backWidth = (action.width - GAP) / 3;
    button(s, w, rect(action.x, action.y, backWidth, action.height), tr(STR_BACK), BackToRequests);
    button(s, w, rect(action.x + backWidth + GAP, action.y, action.width - backWidth - GAP, action.height),
           id == quest::kInkHunt && m.profile->pausedInkSeconds ? tr(STR_QUEST_HUNTS_RESUME) : tr(STR_QUEST_HUNT_AGAIN),
           AcceptRequest, id, true, m.writable);
  } else
    button(s, w, action, tr(STR_QUEST_COLLECTION_ATLAS_BACK), Back, 0, true);
}
void campaignHistory(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  w.page = 0;
  w.pages = 1;
  const unsigned chapter = std::clamp(m.selectedEncounter / 6, 0, 119);
  victoryHistory(s, m, w, r, quest::huntForPath(chapter), m.profile->campaignVictories[chapter], int(chapter));
}

void huntHistory(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, int id) {
  victoryHistory(s, m, w, r, id, quest::huntVictory(*m.profile, id));
}

void request(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  w.page = 0;
  w.pages = 1;
  const auto& p = *m.profile;
  const int id = m.selectedRequest;
  if (!quest::validRequest(id) || (!quest::requestUnlocked(p, id) && !quest::requestClaimed(p, id))) return;
  const bool wide = s.device().width > s.device().height, active = p.activeRequest == id;
  const bool claimed = quest::requestClaimed(p, id) && !active;
  if (claimed) {
    huntHistory(s, m, w, r, id);
    return;
  }
  const unsigned current = quest::requestValue(p, id, m.summary), target = quest::requestTarget(p, id);
  const bool ready = active && current >= target;
  const bool legacy = active && p.requestGoal != quest::RequestGoal::Minutes;
  heading(s, r, huntTitle(id));
  const int artWidth = wide ? r.width * 48 / 100 : r.width;
  const auto side = wide ? rect(r.x + artWidth + GAP, r.y, r.width - artWidth - GAP, r.height) : r;
  const auto action = rect(side.x, side.bottom() - 52, side.width, 52);
  const auto gains = wide ? rect(r.x, action.y, artWidth, action.height) : rect(side.x, action.y - 50, side.width, 40);
  const int taskHeight = 48;
  const int rewardHeight = 52;
  const auto reward =
      rect(side.x, wide ? side.y + taskHeight + GAP : gains.y - rewardHeight - GAP, side.width, rewardHeight);
  const auto task = rect(side.x, wide ? side.y : reward.y - taskHeight - GAP, side.width, taskHeight);
  const auto picture = rect(r.x, r.y, artWidth, wide ? action.y - r.y - GAP : task.y - r.y - GAP);
  panel(s, picture);
  huntPicture(s, m, picture.inset({6, 6, 6, 6}), id, questenemy::progressPose(current, target),
              active ? HuntScene::Encounter : HuntScene::Preview);
  rankBadge(s, picture, id);
  panel(s, task);
  icon(s, rect(task.x + 12, task.y + (task.height - 30) / 2, 30, 30),
       ready || claimed ? extra_checked_32 : requestui::goalIcon(quest::requestGoal(p, id)));
  const char* status = ready || claimed                                         ? tr(STR_QUEST_CAM_HUNT_DEFEATED)
                       : !active && id == quest::kInkHunt && p.pausedInkSeconds ? tr(STR_QUEST_HUNTS_PAUSED)
                       : legacy                                                 ? tr(STR_QUEST_CAM_HUNT_LEGACY)
                       : active                                                 ? tr(STR_QUEST_CAM_HUNT_READ)
                                                                                : tr(STR_QUEST_CAM_HUNT_TARGET);
  const auto* compactStatus = ready ? tr(STR_QUEST_CAM_HUNT_DEFEATED) : legacy ? status : tr(STR_SETTINGS_READING);
  if (!claimed) {
    requestui::goalText(w, m, id, active || (id == quest::kInkHunt && p.pausedInkSeconds), true);
    fui::TextStyle valueStyle;
    valueStyle.bold = true;
    const int valueWidth = std::min(int(task.width) / 2, int(s.target().measureText(0, w.label, valueStyle).width) + 8);
    text(s, rect(task.x + 56, task.y + 4, task.width - valueWidth - 72, task.height - 8), compactStatus,
         HUNT_LABEL_FONT, true);
    text(s, rect(task.right() - valueWidth - 12, task.y + 4, valueWidth, task.height - 8), w.label, 0, true,
         fui::TextAlign::Right);
  }
  rewardLink(s, m, w, reward, id);
  requestBenefits(s, m, w, gains, id);
  if (ready) {
    button(s, w, action, tr(STR_QUEST_CAM_RECEIVE), CollectRequest, id, true, m.writable);
  } else if (active) {
    const int left = (action.width - GAP) * 43 / 100;
    button(s, w, rect(action.x, action.y, left, action.height),
           id == quest::kInkHunt ? tr(STR_QUEST_HUNTS_PAUSE) : tr(STR_QUEST_CAM_HUNT_CANCEL), CancelRequest, id, false,
           m.writable);
    button(s, w, rect(action.x + left + GAP, action.y, action.width - left - GAP, action.height),
           tr(STR_QUEST_COLLECTION_MENU), OpenView, int(View::Hero), true);
  } else if (claimed)
    button(s, w, action, tr(STR_QUEST_CAM_HUNT_BACK), BackToRequests, 0, true);
  else
    button(s, w, action,
           id == quest::kInkHunt && p.pausedInkSeconds ? tr(STR_QUEST_HUNTS_RESUME) : tr(STR_QUEST_CAM_HUNT_ACCEPT),
           AcceptRequest, id, true, m.writable);
}
void requestBenefits(toybox::Screen& s, const Model& m, Work& w, fui::Rect row, int id,
                     const quest::HuntVictory* earned) {
  const int gap = 8, first = row.width * 55 / 100;
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_RETURN_XP),
                static_cast<unsigned long long>(earned ? earned->xp : quest::requestXp(*m.profile, id)));
  statusPill(s, rect(row.x, row.y, first - gap, row.height), w.label, false, &extra_trophy_32, 0);
  const auto coins = rect(row.x + first, row.y, row.width - first, row.height);
  s.target().fill(coins, paper, coins.height / 2);
  s.target().stroke(coins, ink, 2, coins.height / 2);
  questicons::coin(s.target(), rect(coins.x + 8, coins.y + (coins.height - 20) / 2, 20, 20));
  std::snprintf(w.label, sizeof(w.label), "%u", earned ? earned->coins : quest::requestCoins(*m.profile, id));
  text(s, rect(coins.x + 32, coins.y, coins.width - 38, coins.height), w.label, 0, false, fui::TextAlign::Center);
}
void requests(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  if (quest::hasWon(p) && p.endingSeen) {
    ornamentTitle(s, rect(r.x, r.y, r.width - 128, 38), tr(STR_QUEST_CAM_REQUESTS));
    button(s, w, rect(r.right() - 116, r.y, 116, 38), tr(STR_QUEST_CAM_TITLE), OpenView, int(View::Campaign));
    r.y += 48;
    r.height -= 48;
  } else {
    heading(s, r, tr(STR_QUEST_CAM_REQUESTS));
  }
  const bool collection = m.requestFilter == RequestFilter::Collection, wide = s.device().width > s.device().height;
  for (int n = 0; n < 2; ++n)
    button(s, w, rect(r.x + n * (r.width + GAP) / 2, r.y, (r.width - GAP) / 2, 40),
           n ? tr(STR_QUEST_CAM_REQUEST_COLLECTION) : tr(STR_QUEST_CAM_REQUEST_AVAILABLE), FilterRequests, n,
           n == int(m.requestFilter));
  r.y += 52;
  r.height -= 52;
  const int columns = wide && !collection ? 4 : 2;
  const int rows = wide ? 1 : 2;
  const int capacity = columns * rows;
  int available[quest::MAX_AVAILABLE_HUNTS];
  const int count = collection ? quest::requestCount(p, true) : quest::availableHunts(p, available);
  const int pages = std::max(1, (count + capacity - 1) / capacity), page = std::clamp(m.page, 0, pages - 1);
  w.page = page;
  w.pages = pages;
  const auto area = rect(r.x, r.y, r.width, r.height - (collection ? 58 : 0));
  const int width = (area.width - (columns - 1) * GAP) / columns;
  const int height = (area.height - (rows - 1) * GAP) / rows;
  if (!count)
    text(s, area.inset({8, 8, 8, 8}),
         collection         ? tr(STR_QUEST_CAM_REQUEST_COLLECTION_EMPTY)
         : quest::hasWon(p) ? tr(STR_QUEST_CAM_REQUEST_COMPLETE_COLLECTION)
                            : tr(STR_QUEST_CAM_REQUEST_EMPTY),
         0, false, fui::TextAlign::Left, false, 4);
  for (int n = 0; n < capacity && page * capacity + n < count; ++n) {
    const int id = collection ? quest::requestAt(p, true, page * capacity + n) : available[n];
    const auto box = rect(area.x + n % columns * (width + GAP), area.y + n / columns * (height + GAP), width, height);
    if (collection) {
      victoryCard(s, m, w, box, id, quest::huntVictory(p, id), RequestInfo, id);
      continue;
    }
    const bool active = !collection && p.activeRequest == id;
    const unsigned current = quest::requestValue(p, id, m.summary), target = quest::requestTarget(p, id);
    const bool ready = active && current >= target;
    const bool paused = !active && !collection && id == quest::kInkHunt && p.pausedInkSeconds;
    const bool focused = button(s, w, box, "", RequestInfo, id);
    if (focused) s.target().fill(box.inset({2, 2, 2, 2}), paper, RADIUS - 2);
    const auto rewards = rect(box.x + 9, box.bottom() - 37, box.width - 18, 27);
    fui::TextStyle nameStyle;
    nameStyle.font = 0;
    nameStyle.bold = true;
    const int nameLines = std::clamp(int(fui::textAreaWalk(s.target(), box.width - 18, huntTitle(id), nameStyle,
                                                           [](uint32_t, const fui::TextAreaLine&) {})),
                                     1, 3);
    const int nameHeight = collection ? 2 * s.target().lineHeight(3) : nameLines * s.target().lineHeight(0);
    const int statusY = rewards.y - 36, nameY = statusY - nameHeight - 6;
    const int artTop = box.y + 37;
    const auto art = rect(box.x + 10, artTop, box.width - 20, nameY - artTop - 2);
    huntPicture(s, m, art, id, questenemy::progressPose(current, target), HuntScene::Creature);
    rankBadge(s, box, id);
    questicons::chevron(s.target(), rect(box.right() - 19, box.y + 12, 9, 14));
    text(s, rect(box.x + 9, nameY, box.width - 18, nameHeight), huntTitle(id), collection ? 3 : 0, true,
         fui::TextAlign::Center, false, collection ? 2 : 3);
    if (collection || ready)
      statusPill(s, rect(box.x + 9, statusY, box.width - 18, 25), tr(STR_QUEST_CAM_HUNT_DEFEATED), false,
                 &extra_check_32, 3);
    else {
      requestui::goalText(w, m, id, active || paused, true);
      if (active || paused)
        statusPill(s, rect(box.x + 9, statusY, box.width - 18, 26),
                   active ? tr(STR_QUEST_CAM_REQUEST_ACTIVE) : tr(STR_QUEST_HUNTS_PAUSED), active, nullptr, 0);
      else
        text(s, rect(box.x + 9, statusY, box.width - 18, 30), w.label, collection ? 3 : 0, true,
             fui::TextAlign::Center);
    }
    const int half = (rewards.width - 6) / 2;
    if (wide && !collection) {
      for (int kind = 0; kind < 2; ++kind) {
        const auto pill =
            rect(rewards.x + kind * (half + 6), rewards.y, kind ? rewards.width - half - 6 : half, rewards.height);
        s.target().stroke(pill, ink, 2, pill.height / 2);
        const auto mark = rect(pill.x + 4, pill.y + 5, 16, 16);
        if (kind)
          questicons::coin(s.target(), mark);
        else
          icon(s, mark, extra_trophy_32);
        std::snprintf(w.label, sizeof(w.label), kind ? "%u" : "+%u",
                      kind ? quest::requestCoins(*m.profile, id) : quest::requestXp(*m.profile, id));
        text(s, rect(pill.x + 23, pill.y, pill.width - 26, pill.height), w.label, 3, true, fui::TextAlign::Center);
      }
    } else {
      const int half = (rewards.width - 6) / 2;
      std::snprintf(w.label, sizeof(w.label), "+%u", quest::requestXp(*m.profile, id));
      statusPill(s, rect(rewards.x, rewards.y, half, rewards.height), w.label, false, &extra_trophy_32, 3);
      const auto coins = rect(rewards.x + half + 6, rewards.y, rewards.width - half - 6, rewards.height);
      s.target().stroke(coins, ink, 2, coins.height / 2);
      questicons::coin(s.target(), rect(coins.x + 7, coins.y + 4, 19, 19));
      std::snprintf(w.label, sizeof(w.label), "%u", quest::requestCoins(*m.profile, id));
      text(s, rect(coins.x + 29, coins.y, coins.width - 34, coins.height), w.label, 3, false, fui::TextAlign::Center);
    }
    if (focused) s.target().stroke(box.inset({4, 4, 4, 4}), ink, 2, RADIUS - 4);
  }
  if (collection) pagination(s, w, rect(r.x, r.bottom() - 48, r.width, 44), page + 1, pages);
}

void requestChange(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  if (!quest::validRequest(p.activeRequest) || !quest::validRequest(m.selectedRequest)) return;
  const bool switching = m.view == View::RequestSwitchConfirm;
  const bool pausing = p.activeRequest == quest::kInkHunt;
  const bool resuming = switching && m.selectedRequest == quest::kInkHunt && p.pausedInkSeconds;
  const bool wide = s.device().width > s.device().height;
  const int width = std::min(wide ? 720 : 440, int(r.width) - 8);
  const int height = std::min(wide ? 376 : switching ? 630 : 448, int(r.height) - 8);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  panel(s, box);
  const auto inside = box.inset({14, 16, 14, 16});
  text(s, rect(inside.x, inside.y, inside.width, 38),
       switching ? tr(STR_QUEST_CAM_REQUEST_SWITCH_TITLE)
       : pausing ? tr(STR_QUEST_HUNTS_PAUSE_TITLE)
                 : tr(STR_QUEST_CAM_REQUEST_CANCEL_TITLE),
       0, true);
  const auto controls = rect(inside.x, inside.bottom() - (wide ? 48 : 108), inside.width, wide ? 48 : 108);
  const auto warning = rect(inside.x, controls.y - GAP - (wide ? 58 : 96), inside.width, wide ? 58 : 96);
  const auto cards = rect(inside.x, inside.y + (wide ? 34 : 50), inside.width, warning.y - inside.y - (wide ? 42 : 64));
  const bool stackedArt = switching && !wide;
  const int cardWidth = switching ? (cards.width - GAP) / 2 : cards.width;
  const auto current = rect(cards.x, cards.y, cardWidth, cards.height);
  const auto next = rect(current.right() + GAP, cards.y, cardWidth, cards.height);
  const auto card = [&](fui::Rect area, int id, bool active) {
    panel(s, area);
    const auto copy = stackedArt ? rect(area.x + 10, area.y + 10, area.width - 20, area.height - 20)
                                 : rect(area.x + 102, area.y + (wide ? 6 : 9), area.width - 114, area.height - 18);
    const char* label = active ? tr(STR_QUEST_CAM_REQUEST_ACTIVE) : tr(STR_QUEST_CAM_REQUEST_NEW_HUNT);
    fui::TextStyle tagStyle;
    tagStyle.bold = true;
    const int tagWidth = std::min(int(copy.width), s.target().measureText(3, label, tagStyle).width + 20);
    statusPill(s, rect(copy.x, copy.y, tagWidth, 23), label, active, nullptr, 3);
    const auto art = stackedArt ? rect(copy.x, copy.y + 31, copy.width, area.height - 178)
                                : rect(area.x + 12, area.y + 29, 76, area.height - 42);
    if (m.assets)
      huntPicture(s, m, art, id,
                  questenemy::progressPose(quest::requestValue(p, id, m.summary), quest::requestTarget(p, id)),
                  HuntScene::Creature);
    else
      questicons::monsterHead(s.target(), art, true);
    const int nameY = stackedArt ? art.bottom() + 8 : copy.y + (wide ? 26 : 29);
    const int metricY = area.bottom() - (wide ? 27 : 35);
    text(s, rect(copy.x, nameY, copy.width, metricY - nameY - 4), huntTitle(id), toybox::kSmallFont, true,
         fui::TextAlign::Left, false, stackedArt ? 3 : 2);
    requestui::goalText(w, m, id, active);
    icon(s, rect(copy.x, metricY + 2, 22, 22), requestui::goalIcon(quest::requestGoal(p, id)));
    text(s, rect(copy.x + 30, metricY, copy.width - 30, wide ? 24 : 28), w.label, (stackedArt || wide) ? 3 : 0);
  };
  card(current, p.activeRequest, true);
  if (switching) card(next, m.selectedRequest, false);
  const auto mark = rect(warning.x + 2, warning.y + 3, 24, 24);
  if (pausing)
    icon(s, mark, extra_check_32);
  else {
    s.target().stroke(mark, ink, 2, 12);
    s.target().fill(rect(mark.x + 11, mark.y + 5, 2, 8), ink);
    s.target().fill(rect(mark.x + 11, mark.y + 16, 2, 2), ink);
  }
  text(s, rect(warning.x + 38, warning.y, warning.width - 38, warning.height),
       pausing     ? tr(STR_QUEST_HUNTS_PAUSE_WARNING)
       : resuming  ? tr(STR_QUEST_HUNTS_RESUME_WARNING)
       : switching ? tr(STR_QUEST_CAM_REQUEST_SWITCH_WARNING)
                   : tr(STR_QUEST_CAM_REQUEST_LOSE_PROGRESS),
       toybox::kSmallFont, false, fui::TextAlign::Left, false, 3);
  const int half = (controls.width - GAP) / 2;
  button(s, w, rect(controls.x, controls.y, wide ? half : controls.width, 48), tr(STR_QUEST_CAM_REQUEST_KEEP_CURRENT),
         KeepRequest, 0, true);
  button(s, w,
         rect(wide ? controls.x + half + GAP : controls.x, wide ? controls.y : controls.bottom() - 48,
              wide ? controls.width - half - GAP : controls.width, 48),
         switching ? tr(STR_QUEST_CAM_REQUEST_CONFIRM_SWITCH)
         : pausing ? tr(STR_QUEST_HUNTS_PAUSE)
                   : tr(STR_QUEST_CAM_REQUEST_CANCEL),
         ConfirmRequestChange, m.selectedRequest, false, m.writable);
  s.frame().limitButtonActions({KeepRequest, ConfirmRequestChange});
}

}  // namespace

const char* journeyTitle(unsigned encounter) {
  const unsigned family = questenemy::family(encounter);
  if (family == 72)
    return encounter / quest::ENCOUNTERS_PER_CHAPTER == quest::CAMPAIGN_CHAPTERS - 1 ? tr(STR_QUEST_CAM_EXP_FINAL_KING)
                                                                                     : tr(STR_QUEST_CAM_KING_SHADOW);
  return I18N.get(queststory::ENEMIES[family / 6][family % 6]);
}
const char* journeyResult(unsigned encounter) {
  switch (questjourney::kind(encounter)) {
    case questjourney::Kind::Combat:
      return tr(STR_QUEST_CAM_JOURNEY_DEFEATED);
    case questjourney::Kind::Explore:
      return tr(STR_QUEST_CAM_JOURNEY_FOUND);
    case questjourney::Kind::Rescue:
      return tr(STR_QUEST_CAM_JOURNEY_FREED);
    case questjourney::Kind::Restore:
      return tr(STR_QUEST_CAM_JOURNEY_REPAIRED);
  }
  return tr(STR_QUEST_CAM_JOURNEY_COMPLETE);
}
const char* huntTitle(int id) {
  if (quest::bestiaryHunt(id)) return I18N.get(questbestiary::NAME[id - quest::kBestiaryHuntStart]);
  switch (id) {
    case quest::kStormHunt:
      return tr(STR_QUEST_HUNTS_STORM_NAME);
    case quest::kCrystalHunt:
      return tr(STR_QUEST_HUNTS_CRYSTAL_NAME);
    case quest::kPhoenixHunt:
      return tr(STR_QUEST_HUNTS_PHOENIX_NAME);
    case quest::kInkHunt:
      return tr(STR_QUEST_HUNTS_INK_NAME);
    default:
      return journeyTitle(quest::huntEncounter(id));
  }
}
const freeink::Icon& journeyIcon(questjourney::Kind kind) {
  switch (kind) {
    case questjourney::Kind::Combat:
      return questaction::campaign;
    case questjourney::Kind::Explore:
      return questaction::pathTab;
    case questjourney::Kind::Rescue:
      return icon_cat_people_32;
    case questjourney::Kind::Restore:
      return questaction::phase1;
  }
  return questaction::sagaTab;
}
void journeyScene(toybox::Screen& s, const Model& m, fui::Rect r, unsigned encounter, bool restored) {
  if (!m.assets) return;
  if (restored) {
    questmemory::draw(s.target(), r, *m.profile, m.assets, encounter / quest::ENCOUNTERS_PER_CHAPTER,
                      questenemy::Pose::Defeated, m.enemyAssets, m.rewardAssets, m.outfitAssets);
    return;
  }
  questavatar::draw(s.target(), r, *m.profile, *m.assets, -1, questassets::SAGA_SCENES + encounter / 60 * 2,
                    questavatar::Framing::Contain, m.rewardAssets, questavatar::Cast::HeroOnly, m.outfitAssets);
  framedIcon(s, rect(r.right() - 48, r.y + 6, 42, 42), journeyIcon(questjourney::kind(encounter)));
}
void requestReturn(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  if (!m.readingReturn || !quest::validRequest(m.readingReturn->requestId)) return;
  const auto& report = *m.readingReturn;
  const int id = report.requestId;
  const bool playing = questreturn::isPlaying(m.attackPhase);
  const unsigned shown =
      !playing || m.attackPhase == questreturn::Phase::Impact ? report.requestAfter : report.requestBefore;
  const bool complete = shown >= report.requestTarget;
  const bool only = questreturn::huntOnly(report);
  heading(s, r, only ? tr(STR_QUEST_RETURN_HUNT_ONLY) : tr(STR_QUEST_RETURN_HUNT));
  if (only && report.seconds) {
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_RETURN_TIME),
                  static_cast<unsigned long long>(report.seconds / 60));
    text(s, rect(r.x, r.y, r.width, 32), w.label, 0, true, fui::TextAlign::Center);
    r.y += 40;
    r.height -= 40;
  }
  const bool wide = s.device().width > s.device().height;
  const int taskHeight = wide ? 64 : 88;
  const int gap = wide ? 8 : GAP, actionHeight = wide ? 44 : 52, gainsHeight = complete ? (wide ? 28 : 32) : 0;
  const int rewardHeight = 52;
  const auto picture =
      wide ? rect(r.x, r.y, r.width * 42 / 100, r.height)
           : rect(r.x, r.y, r.width,
                  int(r.height) - taskHeight - rewardHeight - actionHeight - gainsHeight - (complete ? 4 : 3) * gap);
  panel(s, picture);
  huntPicture(s, m, picture.inset({8, 8, 8, 8}), id,
              playing ? questreturn::monsterPose(m.attackPhase) : questenemy::progressPose(shown, report.requestTarget),
              HuntScene::Encounter);
  if (m.assets)
    questreturn::letters(s.target(), picture.inset({8, 8, 8, 8}), *m.assets, *m.profile, m.attackPhase,
                         uint32_t(report.seconds) ^ uint32_t(id),
                         wide ? questavatar::Framing::Contain : questavatar::Framing::Tall, m.rewardAssets,
                         m.outfitAssets);
  const auto content = wide ? rect(picture.right() + GAP, r.y, r.right() - picture.right() - GAP, r.height)
                            : rect(r.x, picture.bottom() + GAP, r.width, r.bottom() - picture.bottom() - GAP);
  const auto task = rect(content.x, content.y, content.width, taskHeight);
  panel(s, task);
  framedIcon(s, rect(task.x + 10, task.y + 16, 44, 44), requestui::goalIcon(quest::requestGoal(*m.profile, id)));
  text(s, rect(task.x + 66, task.y + 6, task.width - 78, wide ? 26 : 40), huntTitle(id), 0, true, fui::TextAlign::Left,
       false, wide ? 1 : 2);
  requestui::goalProgressText(w, quest::requestGoal(*m.profile, id), shown, report.requestTarget);
  text(s, rect(task.x + 66, task.y + (wide ? 30 : 48), task.width - 78, 30), w.label, wide ? 0 : 1, true);
  rewardLink(s, m, w, rect(content.x, task.bottom() + gap, content.width, rewardHeight), id, !playing);
  if (complete)
    requestBenefits(
        s, m, w, rect(content.x, content.bottom() - actionHeight - gap - gainsHeight, content.width, gainsHeight), id);
  button(s, w, rect(content.x, content.bottom() - actionHeight, content.width, actionHeight),
         complete ? tr(STR_QUEST_RETURN_CLAIM_MENU) : tr(STR_QUEST_RETURN_MENU), DismissReadingReturn, 0, true,
         !playing);
}
void huntRewards(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const int id = m.selectedRequest;
  if (!quest::validRequest(id)) return;
  w.page = 0;
  w.pages = 1;
  heading(s, r, tr(STR_QUEST_HUNT_REWARDS));
  const auto action = rect(r.x, r.bottom() - 48, r.width, 48);
  text(s, rect(r.x + 4, r.y, r.width - 8, 40), huntTitle(id), 0, true, fui::TextAlign::Center, false, 2);
  const bool earned = m.profile->activeRequest != id && quest::requestClaimed(*m.profile, id);
  requestBenefits(s, m, w, rect(r.x, r.y + 48, r.width, 40), id,
                  earned ? &quest::huntVictory(*m.profile, id) : nullptr);
  r.y += 100;
  r.height = action.y - GAP - r.y;
  int items[3] = {quest::huntOutfitItem(id), 0, 0};
  int count = 1;
  const int firstExtra = quest::huntExtraItem(id, 0);
  for (int item :
       {quest::visibleItem(firstExtra) ? firstExtra : quest::requestDecoration(id), quest::huntExtraItem(id, 1)})
    if (quest::visibleItem(item)) items[count++] = item;
  const bool wide = s.device().width > s.device().height;
  const bool stacked = wide;
  const int columns = wide ? count : 1;
  const int rows = (count + columns - 1) / columns;
  const int width = (r.width - (columns - 1) * GAP) / columns;
  const int height = (r.height - (rows - 1) * GAP) / rows;
  for (int n = 0; n < count; ++n) {
    const int item = items[n];
    const auto box = rect(r.x + n % columns * (width + GAP), r.y + n / columns * (height + GAP), width, height);
    panel(s, box);
    const int side = stacked ? std::min(100, int(box.height) - 76) : std::min(100, int(box.width) / 3);
    const auto art = stacked ? rect(box.x + (box.width - side) / 2, box.y + 8, side, side)
                             : rect(box.x + 12, box.y + 8, side, box.height - 16);
    if (m.assets) {
      switch (quest::slot(item)) {
        case quest::Slot::Body:
          questextra::paint(s.target(), art,
                            questextra::figure(*m.assets, m.rewardAssets, item, m.profile->gender, m.outfitAssets));
          break;
        case quest::Slot::Pet:
          questextra::paint(s.target(), art, questextra::pet(*m.assets, m.rewardAssets, item));
          break;
        case quest::Slot::Background:
          if (item == quest::kKingBackground) {
            if (m.rewardAssets)
              m.rewardAssets->draw(s.target(), art, questassets::KING_BACKGROUND_ICON, fui::BitmapMode::Contain);
          } else
            m.assets->draw(s.target(), art,
                           questassets::BACKGROUND_ICONS + item - quest::first(quest::Slot::Background),
                           fui::BitmapMode::Contain);
          break;
        case quest::Slot::Decoration:
          questextra::paint(s.target(), art, questextra::decoration(*m.assets, m.rewardAssets, item));
          break;
      }
    }
    const auto label = stacked ? rect(box.x + 8, art.bottom() + 6, box.width - 16, box.bottom() - art.bottom() - 14)
                               : rect(art.right() + 12, box.y + 10, box.right() - art.right() - 24, box.height - 20);
    text(s, label,
         n == 0                                 ? requestui::outfitName(id)
         : item == quest::requestDecoration(id) ? requestui::rewardName(id)
                                                : requestui::extraName(item),
         0, n == 0, stacked ? fui::TextAlign::Center : fui::TextAlign::Left, false, 3);
  }
  button(s, w, action, tr(STR_BACK), Back, 0, true);
}
void buildJourney(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  switch (m.view) {
    case View::Atlas:
      atlas(s, m, w, r);
      break;
    case View::AtlasPlace:
      campaignHistory(s, m, w, r);
      break;
    case View::AtlasRegion:
      region(s, m, w, r);
      break;
    case View::Requests:
      requests(s, m, w, r);
      break;
    case View::RequestCancelConfirm:
    case View::RequestSwitchConfirm:
      requestChange(s, m, w, r);
      break;
    case View::ReadingReturn:
      requestReturn(s, m, w, r);
      break;
    case View::Recap:
      recap(s, m, w, r);
      break;
    case View::HuntRewards:
      huntRewards(s, m, w, r);
      break;
    case View::Request:
    case View::Thanks:
      request(s, m, w, r);
      break;
    default:
      break;
  }
}
}  // namespace questui
