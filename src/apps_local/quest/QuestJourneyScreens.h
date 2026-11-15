#pragma once

#include "QuestJourney.h"
#include "QuestScreens.h"

namespace questui {
void buildJourney(toybox::Screen&, const Model&, Work&, freeink::ui::Rect);
void journeyScene(toybox::Screen&, const Model&, freeink::ui::Rect, unsigned encounter, bool restored = false);
const char* journeyTitle(unsigned encounter);
const char* huntTitle(int id);
const char* journeyResult(unsigned encounter);
const freeink::Icon& journeyIcon(questjourney::Kind);
}  // namespace questui
