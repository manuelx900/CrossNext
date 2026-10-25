#pragma once

#include "../ui/ToyboxScreen.h"
#include "FlashTextSize.h"

namespace flashui {
inline constexpr freeink::ui::ActionId SettingsBack = 451, QuestionSize = 452, AnswerSize = 453, SmartSize = 455;
struct TextSettingsModel {
  flashcards::TextApp app = flashcards::TextApp::Flashcards;
  uint8_t question = 0, answer = 0;
  bool smart = true, error = false;
};
void textSettings(toybox::Screen&, const TextSettingsModel&);
}  // namespace flashui
