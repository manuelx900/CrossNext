#pragma once

#include "ReadingGoal.h"
#include "ReadingStatsScreens.h"

namespace readingui {
void goalCard(toybox::Screen& screen, freeink::ui::Rect rect, const Model& model);
void goalCalendar(toybox::Screen& screen, freeink::ui::Rect rect, const Model& model);
void goalWallpaper(toybox::Screen& screen, const readingstats::Summary* summary, int day, unsigned goal,
                   int startDay = 0);
void goalOverlay(freeink::ui::DrawTarget& target, const freeink::ui::DeviceContext& device,
                 const readinggoal::WallpaperData& data);
}  // namespace readingui
