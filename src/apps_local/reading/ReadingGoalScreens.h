#pragma once

#include "ReadingStatsScreens.h"

namespace readingui {
void goalCard(toybox::Screen& screen, freeink::ui::Rect rect, const Model& model);
void goalCalendar(toybox::Screen& screen, freeink::ui::Rect rect, const Model& model);
void goalWallpaper(toybox::Screen& screen, const readingstats::Summary* summary, int day, unsigned goal);
}  // namespace readingui
