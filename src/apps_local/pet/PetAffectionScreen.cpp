#include "PetCelebration.h"

namespace petui {
using namespace widget;
namespace {
const char* reactionText(const pet::Affection& result) {
  switch (result.reaction) {
    case pet::Need::Sick:
      return tr(STR_PET_AFFECTION_SICK);
    case pet::Need::Sleeping:
      return tr(STR_PET_AFFECTION_SLEEP);
    case pet::Need::Hungry:
      return tr(STR_PET_AFFECTION_HUNGRY);
    case pet::Need::Tired:
      return tr(STR_PET_AFFECTION_TIRED);
    case pet::Need::Dirty:
      return tr(STR_PET_AFFECTION_DIRTY);
    case pet::Need::Company:
      return tr(STR_PET_AFFECTION_COMPANY);
    case pet::Need::None:
      return !result.first          ? tr(STR_PET_AFFECTION_AGAIN)
             : result.before == 100 ? tr(STR_PET_AFFECTION_FULL)
                                    : tr(STR_PET_AFFECTION_HAPPY);
  }
  return "";
}
void doubleFrame(toybox::Screen& s, fui::Rect r) {
  s.target().fill(rect(r.x + 4, r.y + 4, r.width, r.height), ink);
  s.target().fill(r, paper);
  frame(s, r);
}
void pettingHand(toybox::Screen& s, fui::Rect r) {
  static constexpr fui::Point outline[] = {
      {73, 19}, {62, 19}, {52, 22}, {43, 26}, {32, 32}, {21, 39}, {9, 48},  {9, 51},  {12, 52},
      {16, 52}, {14, 56}, {14, 59}, {17, 60}, {22, 60}, {22, 64}, {25, 66}, {29, 66}, {35, 61},
      {35, 67}, {37, 70}, {41, 70}, {45, 67}, {50, 60}, {56, 54}, {63, 48}, {66, 46}, {66, 49},
      {63, 57}, {63, 63}, {65, 67}, {69, 67}, {72, 62}, {74, 57}, {78, 52}, {82, 48}, {85, 43},
      {87, 38}, {90, 35}, {92, 35}, {91, 29}, {87, 24}, {81, 19}, {78, 16}, {75, 16}, {73, 19}};
  const auto point = [&](fui::Point p) {
    return fui::Point{int16_t(r.x + p.x * r.width / 113), int16_t(r.y + p.y * r.height / 71)};
  };
  const auto segment = [&](fui::Point a, fui::Point b, int weight = 2) {
    s.target().line(point(a), point(b), weight, ink);
  };
  for (unsigned i = 1; i < sizeof(outline) / sizeof(outline[0]); ++i) segment(outline[i - 1], outline[i]);
  static constexpr fui::Point fingers[] = {{16, 52}, {25, 41}, {32, 36}, {39, 32}, {22, 60}, {32, 47},
                                           {39, 40}, {47, 35}, {35, 61}, {41, 51}, {47, 46}, {55, 41}};
  for (unsigned finger = 0; finger < 3; ++finger)
    for (unsigned i = 1; i < 4; ++i) segment(fingers[finger * 4 + i - 1], fingers[finger * 4 + i], 2);
  static constexpr fui::Point cuff[] = {{73, 16}, {87, 1},  {93, 1},  {109, 19}, {111, 25}, {97, 38},
                                        {93, 38}, {90, 28}, {82, 21}, {77, 16},  {73, 16}};
  for (unsigned i = 1; i + 1 < sizeof(cuff) / sizeof(cuff[0]); ++i)
    s.target().triangle(point(cuff[0]), point(cuff[i]), point(cuff[i + 1]), ink);
  for (unsigned i = 1; i < sizeof(cuff) / sizeof(cuff[0]); ++i) segment(cuff[i - 1], cuff[i]);
  segment({11, 18}, {20, 27});
  segment({1, 26}, {11, 34});
}
void picture(toybox::Screen& s, const Model& m, fui::Rect r) {
  // The picture and numbers describe this petting, even if a care tick runs while it is open.
  auto pose = petsleep::Pose::Happy;
  switch (m.affection.reaction) {
    case pet::Need::Sick:
      pose = petsleep::Pose::Sick;
      break;
    case pet::Need::Hungry:
      pose = petsleep::Pose::Hungry;
      break;
    case pet::Need::Sleeping:
    case pet::Need::Tired:
      pose = petsleep::Pose::Tired;
      break;
    case pet::Need::None:
    case pet::Need::Company:
    case pet::Need::Dirty:
      break;
  }
  const auto& p = *m.profile;
  const auto age = pet::stage(p.readingSeconds);
  if (p.species == pet::Species::Silva && age == pet::Stage::Adult &&
      (m.affection.reaction == pet::Need::None || m.affection.reaction == pet::Need::Company)) {
    icon(s, r, referenceicons::SILVA_AFFECTION);
    return;
  }
  const bool sleeping = m.affection.reaction == pet::Need::Sleeping;
  const unsigned mask = sleeping ? petsleep::artIndex(p, 0) : petsleep::bodyIndex(p.species, age, pose, 0);
  const unsigned id = 3 + unsigned(p.species) * 16 + (unsigned(age) - 1) * 4 + std::min(unsigned(pose), 3u);
  const bool compact = r.height < 220;
  const int band = compact ? 60 : 74;
  const auto image = rect(r.x + 30, r.y + band, r.width - 60, r.height - band - 2);
  bool drawn = m.sleepArt.draw && m.sleepArt.draw(m.sleepArt.context, s.target(), image, mask + 1, false);
  if (!drawn && (sleeping || pose == petsleep::Pose::Sick)) {
    const unsigned scene = 3 + unsigned(p.species) * 12 + (unsigned(age) - 1) * 3 + (sleeping ? 1 : 2);
    drawn = (m.habitat.draw && m.habitat.draw(m.habitat.context, s.target(), image, 483 + scene * 2, false)) ||
            (m.scenes.draw && m.scenes.draw(m.scenes.context, s.target(), image, scene, false));
  }
  if (!drawn) art(s, m, image, id, speciesIcon(p.species), false);
  const int handW = compact ? 88 : 110, handH = compact ? 55 : 69;
  const auto hand = rect(r.x + r.width / 2 - handW * 35 / 100, r.y + 1, handW, handH);
  if (!m.art.draw || !m.art.draw(m.art.context, s.target(), hand, PETTING_HAND_ART, false)) pettingHand(s, hand);
  icon(s, rect(r.x + r.width / 4 - 8, r.y + 15, 26, 26), icon_hearts_32);
  icon(s, rect(r.x + r.width * 3 / 4 - 9, r.y + 2, 32, 32), icon_hearts_32);
}
}  // namespace
void affectionWindow(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const int width = std::min(int(r.width) - 8, wide ? 712 : 446);
  const int height = std::min(int(r.height) - 16, wide ? 338 : 660);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  doubleFrame(s, box);
  const auto title = rect(box.x + 12, box.y + 12, box.width - 24, 58);
  celebrationBanner(s, title, tr(STR_PET_STROKE), icon_hearts_32, false, true);
  const int lineY = title.bottom();
  const auto footer = rect(box.x + 18, box.bottom() - 72, box.width - 36, 54);
  const auto result = wide ? rect(box.x + box.width / 2, footer.y - 82, box.width / 2 - 24, 66)
                           : rect(box.x + 30, footer.y - 88, box.width - 60, 70);
  const auto copy = wide ? rect(result.x, lineY + 15, result.width, result.y - lineY - 25)
                         : rect(box.x + 28, lineY + 18, box.width - 56, 82);
  const auto image = wide ? rect(box.x + 18, lineY + 14, box.width / 2 - 38, footer.y - lineY - 26)
                          : rect(box.x + 26, copy.bottom() + 10, box.width - 52, result.y - copy.bottom() - 20);
  prose(s, copy, reactionText(m.affection), wide ? 4 : 5, fui::TextAlign::Center);
  picture(s, m, image);
  sparkle(s, image.x + 12, image.y + image.height / 2, 18);
  sparkle(s, image.right() - 12, image.y + image.height * 2 / 3, 16);
  frame(s, result);
  icon(s, rect(result.x + 12, result.y + (result.height - 34) / 2, 34, 34), icon_hearts_32);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_AFFECTION_RECEIPT), unsigned(m.affection.before),
                unsigned(m.affection.after), unsigned(m.affection.after - m.affection.before));
  text(s, rect(result.x + 58, result.y + 8, result.width - 70, result.height - 16), w.label, 0);
  button(s, w, footer, tr(STR_PET_CONTINUE), Back, 0, true);
}
}  // namespace petui
