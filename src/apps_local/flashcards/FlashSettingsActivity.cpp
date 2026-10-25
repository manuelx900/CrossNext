#include "FlashSettingsActivity.h"

#include <Logging.h>
#include <Memory.h>

#include "../../CrossPointSettings.h"
#include "../../components/SideButtonNavigation.h"
#include "../ui/PixelMenuTheme.h"
#include "FlashTextSize.h"

std::unique_ptr<Activity> FlashSettingsActivity::create(GfxRenderer& renderer, MappedInputManager& input,
                                                        flashcards::TextApp app) {
  auto child = makeUniqueNoThrow<FlashSettingsActivity>(renderer, input, app);
  if (!child) LOG_ERR("FLASH", "OOM: text settings (%u bytes)", unsigned(sizeof(FlashSettingsActivity)));
  return child;
}
void FlashSettingsActivity::onEnter() {
  Activity::onEnter();
  const bool words = model.app == flashcards::TextApp::Words;
  model.question = originalQuestion =
      flashcards::validTextSize(words ? SETTINGS.wordsQuestionSize : SETTINGS.flashcardQuestionSize);
  model.answer = originalAnswer =
      flashcards::validTextSize(words ? SETTINGS.wordsAnswerSize : SETTINGS.flashcardAnswerSize);
  model.smart = originalSmart = words ? SETTINGS.wordsSmartSize : SETTINGS.flashcardSmartSize;
  requestUpdate();
}
bool FlashSettingsActivity::saveChanges() {
  if (model.question == originalQuestion && model.answer == originalAnswer && model.smart == originalSmart) return true;
  if (!SETTINGS.saveToFile()) {
    LOG_ERR("FLASH", "Cannot save text sizes");
    return false;
  }
  originalQuestion = model.question;
  originalAnswer = model.answer;
  originalSmart = model.smart;
  return true;
}
void FlashSettingsActivity::onExit() {
  saveChanges();
  Activity::onExit();
}
void FlashSettingsActivity::back() {
  RenderLock lock;
  if (saveChanges())
    finish();
  else {
    model.error = true;
    requestUpdate();
  }
}
void FlashSettingsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    back();
    return;
  }
  if (!ready) return;
  int x = 0, y = 0;
  if (!sidebuttons::poll(interactions, mappedInput, x, y, false)) return;
  freeink::ui::InputSnapshot tap;
  tap.touchReleased = true;
  tap.touchX = x;
  tap.touchY = y;
  const auto event = sidebuttons::route(interactions, tap, mappedInput, *this, false);
  if (event.action == flashui::SettingsBack) {
    back();
    return;
  }
  RenderLock lock;
  const bool words = model.app == flashcards::TextApp::Words;
  if (event.action == flashui::SmartSize) {
    model.smart = !model.smart;
    (words ? SETTINGS.wordsSmartSize : SETTINGS.flashcardSmartSize) = model.smart;
  } else if (event.action == flashui::QuestionSize && event.value >= 0 && event.value < flashcards::kTextSizeCount) {
    if (model.question == event.value) return;
    model.question = (words ? SETTINGS.wordsQuestionSize : SETTINGS.flashcardQuestionSize) = event.value;
  } else if (event.action == flashui::AnswerSize && event.value >= 0 && event.value < flashcards::kTextSizeCount) {
    if (model.answer == event.value) return;
    model.answer = (words ? SETTINGS.wordsAnswerSize : SETTINGS.flashcardAnswerSize) = event.value;
  } else
    return;
  model.error = false;
  requestUpdate();
}
void FlashSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, true);
  target.setFont(3, UI_10_FONT_ID);
  target.setFont(2, flashcards::textSizeFont(model.question));
  target.setFont(4, flashcards::textSizeFont(model.answer));
  const freeink::ui::InputSnapshot none;
  toybox::Frame frame(target, target.deviceContext(), none, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  ready = false;
  flashui::textSettings(screen, model);
  ready = true;
  toybox::reportOverflow(interactions, "Flashcard text settings", &renderer);
  renderer.displayBuffer();
}
