#pragma once

#include "Dashboard.h"
#include "FitText.h"

namespace requiredresources {
inline void build(toybox::Screen& screen, const char* folder, freeink::ui::ActionId back,
                  freeink::ui::ButtonProps& button) {
  namespace fui = freeink::ui;
  const auto body = screen.body();
  fittext::draw(screen.target(), fui::makeRect(body.x + 16, body.y + 20, body.width - 32, 60),
                tr(STR_QUEST_RESOURCES_REQUIRED), pixelui::kBody, 2, fui::TextAlign::Center);
  fittext::draw(screen.target(),
                fui::makeRect(body.x + 20, body.y + 96, body.width - 40, std::min(180, int(body.height) - 230)),
                tr(STR_APP_RESOURCES_COPY), pixelui::kSmall, 12, fui::TextAlign::Center);
  dashboard::text(screen, fui::makeRect(body.x + 20, body.bottom() - 122, body.width - 40, 40), folder);
  button = {};
  button.label = tr(STR_QUEST_BACK);
  button.text.font = pixelui::kBody;
  button.text.align = fui::TextAlign::Center;
  button.styles = pixelui::buttonStyles();
  button.action = back;
  screen.button(button, fui::makeRect(body.x + 20, body.bottom() - 64, body.width - 40, 52));
}
}  // namespace requiredresources
