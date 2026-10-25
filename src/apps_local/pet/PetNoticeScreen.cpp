#include "PetCelebration.h"
#include "PetPolishIcons.h"
#include "PetWidgets.h"

namespace petui {
using namespace widget;
void noticeWindow(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const auto box = rect(r.x + 4, r.y + 12, r.width - 8, r.height - 24);
  s.target().fill(rect(box.x + 4, box.y + 4, box.width, box.height), ink);
  s.target().fill(box, paper);
  frame(s, box);
  const bool game = m.noticeKind == NoticeKind::Game;
  const bool fed = m.noticeKind == NoticeKind::Fed;
  const bool success = fed || m.noticeKind == NoticeKind::Cleaned || m.noticeKind == NoticeKind::Cured;
  const auto& resultIcon = fed                                   ? extra_dashboard_apple_64
                           : game                                ? extra_cards_64
                           : m.noticeKind == NoticeKind::Cleaned ? SOAP_ICON
                           : m.noticeKind == NoticeKind::Cured   ? MEDICINE_ICON
                                                                 : extra_information_32;
  const char* title = tr(STR_PET_NOTICE_TITLE);
  switch (m.noticeKind) {
    case NoticeKind::Information:
      break;
    case NoticeKind::Cleaned:
      title = tr(STR_PET_NOTICE_CLEAN);
      break;
    case NoticeKind::Cured:
      title = tr(STR_PET_NOTICE_CURED);
      break;
    case NoticeKind::Game:
      title = tr(STR_PET_GAME_RESULT_TITLE);
      break;
    case NoticeKind::Fed:
      title = tr(STR_PET_NOTICE_FED);
      break;
  }
  const auto heading = rect(box.x + 12, box.y + 12, box.width - 24, 58);
  celebrationBanner(s, heading, title, resultIcon, m.noticeKind == NoticeKind::Cured,
                    success || (game && m.gameResult.outcome != GameOutcome::CareNeeded));
  const auto footer = rect(box.x + 12, box.bottom() - 66, box.width - 24, 54);
  const bool receipt = game || (fed && m.feedResult.visible) || (success && m.careResult.visible);
  const auto result = wide ? rect(box.x + box.width / 2, footer.y - 84, box.width / 2 - 14, 74)
                           : rect(box.x + 12, footer.y - 84, box.width - 24, 74);
  const auto copy = wide ? rect(result.x, heading.bottom() + 10, result.width, result.y - heading.bottom() - 18)
                         : rect(box.x + 18, heading.bottom() + 14, box.width - 36, 100);
  const auto picture =
      wide ? rect(box.x + 14, heading.bottom() + 10, box.width / 2 - 28, footer.y - heading.bottom() - 18)
           : rect(box.x + 18, copy.bottom() + 4, box.width - 36,
                  (receipt ? result.y - 10 : footer.y - 10) - copy.bottom() - 4);
  const bool separateSoap =
      m.noticeKind == NoticeKind::Cleaned &&
      !(m.profile->species == pet::Species::Ember && pet::stage(m.profile->readingSeconds) == pet::Stage::Adult);
  const auto companion = separateSoap ? rect(picture.x + 36, picture.y, picture.width - 36, picture.height) : picture;
  prose(s, copy, m.notice ? m.notice : tr(STR_PET_SAVE_FAILED), 5, fui::TextAlign::Center);
  if (m.profile->lifeStatus == pet::LifeStatus::Lost)
    farewellPicture(s, m, picture);
  else
    celebrationPicture(s, m, companion, success || (game && m.gameResult.joyAfter > m.gameResult.joyBefore),
                       m.noticeKind == NoticeKind::Cleaned ? Celebration::Clean
                       : fed                               ? Celebration::Fed
                                                           : Celebration::Joy);
  if (separateSoap) icon(s, rect(picture.x + 4, picture.y + 6, 64, 80), referenceicons::SOAP);
  if (receipt) {
    frame(s, result);
    icon(s, rect(result.x + 10, result.y + 15, 40, 44), resultIcon);
    const int x = result.x + 61, width = result.width - 73;
    if (game) {
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GAME_JOY_RESULT), unsigned(m.gameResult.joyBefore),
                    unsigned(m.gameResult.joyAfter));
      text(s, rect(x, result.y + 5, width, 25), w.label, 0);
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GAME_ENERGY_RESULT), unsigned(m.gameResult.energyBefore),
                    unsigned(m.gameResult.energyAfter));
      text(s, rect(x, result.y + 33, width, 25), w.label, 3);
    } else if (fed && m.feedResult.visible) {
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_FEED_CHANGE), unsigned(m.feedResult.before),
                    unsigned(m.feedResult.after));
      text(s, rect(x, result.y + 5, width, 25), w.label, 0);
      text(s, rect(x, result.y + 33, width, 25), tr(STR_PET_FOOD_COST), 3);
    } else if (success && m.careResult.visible) {
      const auto& care = m.careResult;
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_HYGIENE_RESULT), unsigned(care.hygieneBefore),
                    unsigned(care.hygieneAfter));
      text(s, rect(x, result.y + 5, width, 25), w.label, 0);
      if (m.noticeKind == NoticeKind::Cleaned)
        std::snprintf(w.label, sizeof(w.label), tr(STR_PET_CLEAN_RESULT), unsigned(care.joyBefore),
                      unsigned(care.joyAfter));
      else
        std::snprintf(w.label, sizeof(w.label), tr(STR_PET_CURE_COST), unsigned(care.medicineSpent));
      text(s, rect(x, result.y + 33, width, 25), w.label, 3);
    }
  }
  button(s, w, footer, tr(STR_PET_CONTINUE), Back, 0, true);
}
}  // namespace petui
