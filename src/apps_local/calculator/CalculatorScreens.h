#pragma once
#include "../ui/ToyboxScreen.h"
#include "CalcEngine.h"
#include "CalcLayout.h"

namespace calculatorui {
calc::PadGeom geometry(const freeink::ui::DeviceContext& device);
void build(toybox::Screen& screen, const calc::Engine& engine);
}  // namespace calculatorui
