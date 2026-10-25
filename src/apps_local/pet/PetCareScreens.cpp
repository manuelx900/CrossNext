#include "../ui/PageIndicator.h"
#include "PetAlbumWidgets.h"
#include "PetHeader.h"
#include "PetOverview.h"
#include "PetPolishIcons.h"
#include "PetReferenceIcons.h"

namespace petui {
using namespace widget;
namespace {
void healthHelp(toybox::Screen& s, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  text(s, rect(r.x + 6, r.y, r.width - 12, 42), tr(STR_PET_HEALTH_HELP_TITLE), 1);
  const auto cards = rect(r.x, r.y + 52, r.width, r.height - 114);
  static constexpr StrId titles[] = {StrId::STR_PET_CLEAN, StrId::STR_PET_CURE, StrId::STR_PET_REST};
  static constexpr StrId copy[] = {StrId::STR_PET_HELP_CLEAN, StrId::STR_PET_HELP_CURE, StrId::STR_PET_HELP_REST};
  static constexpr const freeink::Icon* marks[] = {&SOAP_ICON, &MEDICINE_ICON, &extra_moon_64};
  for (unsigned i = 0; i < 3; ++i) {
    const int height = (cards.height - GAP * 2) / 3;
    const auto box =
        wide ? cell(cards, int(i), 3) : rect(cards.x, cards.y + int(i) * (height + GAP), cards.width, height);
    frame(s, box);
    icon(s, rect(box.x + 12, box.y + 12, 32, 32), *marks[i]);
    text(s, rect(box.x + 54, box.y + 12, box.width - 66, 28), I18N.get(titles[i]), 0);
    prose(s, rect(box.x + 12, box.y + 56, box.width - 24, box.height - 68), I18N.get(copy[i]), 6);
  }
  button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_PET_BACK_HEALTH), Back, 0, true);
}
void healthCondition(toybox::Screen& s, const Model& m, fui::Rect r) {
  const auto& p = *m.profile;
  frame(s, r);
  const int imageWidth = r.width * 41 / 100;
  const auto picture = rect(r.right() - imageWidth + 8, r.y + 8, imageWidth - 16, r.height - 16);
  const auto copy = rect(r.x + 12, r.y + 12, r.width - imageWidth - 20, r.height - 24);
  static constexpr StrId titles[] = {StrId::STR_PET_HEALTHY, StrId::STR_PET_COLD, StrId::STR_PET_MALAISE};
  creature(s, m, picture);
  text(s, rect(copy.x, copy.y, copy.width, 32), I18N.get(titles[unsigned(p.illness)]), r.height < 150 ? 0 : 1);
  const int gap = r.height < 150 ? 34 : 40;
  prose(s, rect(copy.x, copy.y + gap, copy.width, copy.height - gap), healthAdvice(m), 5);
}
void healthAction(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, bool healing, bool wide) {
  const auto& p = *m.profile;
  const bool enabled = pet::canCare(p) && m.writable &&
                       (healing ? p.illness != pet::Illness::Healthy && pet::medicine(p) : p.hygiene < 100);
  const bool inverse = button(s, w, box, "", healing ? Cure : Clean, 0, false, nullptr, enabled);
  const auto header = box.inset({4, 4, 0, 4});
  const int headerH = wide ? 34 : 36;
  s.target().fill(rect(header.x, header.y, header.width, headerH), ink);
  icon(s, rect(header.x + 8, header.y + (headerH - 24) / 2, 24, 24), healing ? MEDICINE_ICON : SOAP_ICON, true);
  text(s, rect(header.x + 40, header.y, header.width - 46, headerH), healing ? tr(STR_PET_CURE) : tr(STR_PET_CLEAN), 0,
       true);
  const bool complete = healing ? p.illness == pet::Illness::Healthy : p.hygiene == 100;
  const int side = std::min(wide ? 44 : 64, int(box.width) / 3);
  const int top = header.y + headerH + 8;
  const auto mark = rect(box.x + 10, top + 8, side, std::min(side * 5 / 4, box.bottom() - top - 22));
  icon(s, mark, healing ? (complete ? referenceicons::REMEDY_DONE : referenceicons::REMEDY) : referenceicons::SOAP,
       inverse);
  const int x = mark.right() + 10, width = box.right() - x - (enabled ? 20 : 10);
  if (enabled) {
    prose(s, rect(x, top, width, wide ? 23 : 28), healing ? tr(STR_PET_CURE_ONE) : tr(STR_PET_CLEAN_FREE), 1,
          fui::TextAlign::Left, inverse);
    prose(s, rect(x, top + (wide ? 24 : 30), width, box.bottom() - top - (wide ? 32 : 58)),
          healing ? tr(STR_PET_CURE_BENEFIT) : tr(STR_PET_CLEAN_BENEFIT), 3, fui::TextAlign::Left, inverse);
    if (!wide) text(s, rect(box.x + 10, box.bottom() - 23, box.width - 20, 20), tr(STR_PET_ACTION_READY), 3, inverse);
    chevron(s, rect(box.right() - 15, top + 31, 7, 16), inverse);
  } else {
    prose(s, rect(x, top + 4, width, box.bottom() - top - 14), healthActionHint(m, healing), 4);
  }
}
void healthHelpButton(toybox::Screen& s, Work& w, fui::Rect box) {
  const bool inverse = button(s, w, box, "", OpenHealthHelp);
  icon(s, rect(box.x + 12, box.y + (box.height - 32) / 2, 32, 32), READING_INFO_ICON, inverse);
  s.target().line({int16_t(box.x + 58), int16_t(box.y + 8)}, {int16_t(box.x + 58), int16_t(box.bottom() - 8)}, 1,
                  inverse ? paper : ink);
  text(s, rect(box.x + 72, box.y + 5, box.width - 104, box.height - 10), tr(STR_PET_HEALTH_HELP_TITLE), 0, inverse);
  chevron(s, rect(box.right() - 19, box.y + (box.height - 16) / 2, 7, 16), inverse);
}
}  // namespace
void careDetails(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  if (m.view == pet::View::HealthHelp) {
    healthHelp(s, w, r);
    return;
  }
  if (m.view == pet::View::Health) {
    text(s, rect(r.x + 6, r.y, r.width - 12, 43), tr(STR_PET_HEALTH_HYGIENE), 2);
    if (wide) {
      const auto body = rect(r.x, r.y + 51, r.width, r.height - 113);
      const int leftW = body.width * 44 / 100;
      const auto status = rect(body.x, body.y, leftW, 76);
      metric(s, w, cell(status, 0, 2), tr(STR_PET_HYGIENE), p.hygiene, SOAP_ICON);
      const auto stock = cell(status, 1, 2);
      frame(s, stock);
      icon(s, rect(stock.x + 8, stock.y + 22, 28, 34), MEDICINE_ICON);
      text(s, rect(stock.x + 44, stock.y + 5, stock.width - 50, 24), tr(STR_PET_MEDICINE), 3);
      std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(pet::medicine(p)));
      text(s, rect(stock.x + 44, stock.y + 34, stock.width - 50, 30), w.label, 1);
      healthCondition(s, m, rect(body.x, status.bottom() + GAP, leftW, body.bottom() - status.bottom() - GAP));
      const auto actions = rect(body.x + leftW + GAP, body.y, body.width - leftW - GAP, body.height);
      const int height = (actions.height - GAP) / 2;
      healthAction(s, m, w, rect(actions.x, actions.y, actions.width, height), false, true);
      healthAction(s, m, w, rect(actions.x, actions.y + height + GAP, actions.width, height), true, true);
      const auto footer = rect(r.x, r.bottom() - 54, r.width, 54);
      healthHelpButton(s, w, cell(footer, 0, 2));
      button(s, w, cell(footer, 1, 2), tr(STR_PET_BACK_CARE), Back, 0, true);
      return;
    }
    const auto status = rect(r.x, r.y + 51, r.width, 84);
    metric(s, w, cell(status, 0, 2), tr(STR_PET_HYGIENE), p.hygiene, SOAP_ICON);
    auto stock = cell(status, 1, 2);
    frame(s, stock);
    icon(s, rect(stock.x + 12, stock.y + 20, 40, 40), MEDICINE_ICON);
    text(s, rect(stock.x + 64, stock.y + 6, stock.width - 72, 25), tr(STR_PET_MEDICINE), 0);
    std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(pet::medicine(p)));
    text(s, rect(stock.x + 64, stock.y + 36, stock.width - 72, 36), w.label, 1);
    const int top = status.bottom() + GAP;
    const int actionH = 164, helpH = 48;
    const int conditionH = wide ? r.bottom() - top - 62 : r.bottom() - top - actionH - helpH - 54 - GAP * 3;
    const auto condition = rect(r.x, top, wide ? r.width * 55 / 100 : r.width, conditionH);
    healthCondition(s, m, condition);
    const auto actions = wide
                             ? rect(condition.right() + GAP, top, r.right() - condition.right() - GAP, condition.height)
                             : rect(r.x, condition.bottom() + GAP, r.width, actionH);
    healthAction(s, m, w, cell(actions, 0, 2), false, wide);
    healthAction(s, m, w, cell(actions, 1, 2), true, wide);
    const auto footer = rect(r.x, r.bottom() - 54, r.width, 54);
    if (wide) {
      healthHelpButton(s, w, cell(footer, 0, 2));
      button(s, w, cell(footer, 1, 2), tr(STR_PET_BACK_CARE), Back, 0, true);
    } else {
      healthHelpButton(s, w, rect(r.x, actions.bottom() + GAP, r.width, helpH));
      button(s, w, footer, tr(STR_PET_BACK_CARE), Back, 0, true);
    }
    return;
  }
}
}  // namespace petui
