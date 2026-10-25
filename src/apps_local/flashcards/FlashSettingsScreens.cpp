#include "FlashSettingsScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "FlashTextSize.h"

namespace flashui {
namespace {
namespace fui = freeink::ui;
using fui::makeRect;
void choices(toybox::Screen& screen, fui::Rect band, uint8_t selected, fui::ActionId action) {
  const int gap = 6;
  const int extent = band.width - gap * (flashcards::kTextSizeCount - 1);
  for (int n = 0; n < flashcards::kTextSizeCount; ++n) {
    const auto value = flashcards::kTextSizeOrder[n];
    const int start = n * extent / flashcards::kTextSizeCount + n * gap;
    const int end = (n + 1) * extent / flashcards::kTextSizeCount + n * gap;
    const auto cell = makeRect(band.x + start, band.y, end - start, band.height);
    const bool current = value == selected;
    if (current) screen.target().fill(cell, dashboard::ink);
    screen.target().stroke(cell, dashboard::ink, 2);
    char label[24];
    std::snprintf(label, sizeof(label), tr(STR_CARD_POINT_SIZE_FORMAT), unsigned(flashcards::kTextPoints[value]));
    fittext::draw(screen.target(), cell.inset({2, 3, 2, 3}), label, 3, 1, fui::TextAlign::Center,
                  current ? fui::Color::White : fui::Color::Black);
    screen.frame().hit(cell, action, value, fui::InputTouch);
  }
}
void preview(toybox::Screen& screen, fui::Rect box, bool question) {
  fui::TextStyle style;
  style.font = question ? 2 : 4;
  box = box.inset({8, 8, 8, 8});
  const int height = screen.target().lineHeight(style.font);
  if (height <= 0 || box.height < height) return;
  style.maxLines = std::min(5, int(box.height) / height);
  style.align = fui::TextAlign::Center;
  screen.target().text(box, question ? tr(STR_FLASH_SIZE_QUESTION_SAMPLE) : tr(STR_FLASH_SIZE_ANSWER_SAMPLE), style);
}
void panel(toybox::Screen& screen, fui::Rect box, bool question, uint8_t value, bool words) {
  dashboard::frame(screen, box, 2);
  fittext::draw(screen.target(), makeRect(box.x + 12, box.y + 10, box.width - 24, 36),
                question ? (words ? tr(STR_WORDS_QUESTION_SIZE) : tr(STR_FLASH_QUESTION_SIZE))
                         : (words ? tr(STR_WORDS_ANSWER_SIZE) : tr(STR_FLASH_ANSWER_SIZE)),
                0, 1);
  choices(screen, makeRect(box.x + 12, box.y + 56, box.width - 24, 48), value, question ? QuestionSize : AnswerSize);
  preview(screen, makeRect(box.x + 8, box.y + 116, box.width - 16, box.height - 124), question);
}
}  // namespace
void textSettings(toybox::Screen& screen, const TextSettingsModel& model) {
  pixelui::begin(screen);
  pixelui::Header header;
  const bool words = model.app == flashcards::TextApp::Words;
  header.title = words ? tr(STR_WORDS_TEXT_SETTINGS) : tr(STR_FLASH_TEXT_SETTINGS);
  header.back = screen.device().hasTouch;
  pixelui::header(screen, header);
  backui::publish(header.back, pixelui::geometry(screen.device()).back);
  auto body = screen.body();
  const bool wide = screen.device().width > screen.device().height;
  const auto done = makeRect(body.x, body.bottom() - 56, body.width, 56);
  const auto smart = makeRect(body.x, body.y, body.width, 50);
  dashboard::frame(screen, smart, 2);
  fittext::draw(screen.target(), makeRect(smart.x + 10, smart.y, smart.width - 100, smart.height),
                tr(STR_CARD_SMART_SIZE), 0, 1);
  fittext::draw(screen.target(), makeRect(smart.right() - 84, smart.y, 74, smart.height),
                model.smart ? tr(STR_UI_ON_387D7A) : tr(STR_UI_OFF_AD5048), 0, 1, fui::TextAlign::Center);
  screen.frame().hit(smart, SmartSize, 0, fui::InputTouch);
  const int helpHeight = wide ? 28 : 56, top = wide ? 94 : 122;
  fittext::draw(screen.target(), makeRect(body.x + 4, body.y + 58, body.width - 8, helpHeight), tr(STR_CARD_SMART_HELP),
                3, 2);
  body.y += top;
  body.height -= (model.error ? 112 : 72) + top;
  if (wide) {
    const int width = (body.width - 12) / 2;
    panel(screen, makeRect(body.x, body.y, width, body.height), true, model.question, words);
    panel(screen, makeRect(body.x + width + 12, body.y, body.width - width - 12, body.height), false, model.answer,
          words);
  } else {
    const int height = (body.height - 12) / 2;
    panel(screen, makeRect(body.x, body.y, body.width, height), true, model.question, words);
    panel(screen, makeRect(body.x, body.y + height + 12, body.width, body.height - height - 12), false, model.answer,
          words);
  }
  if (model.error)
    fittext::draw(screen.target(), makeRect(done.x, done.y - 48, done.width, 40), tr(STR_FLASH_SIZE_SAVE_ERROR), 3, 2);
  dashboard::action(screen, done, tr(STR_DONE), SettingsBack, nullptr, false);
}
}  // namespace flashui
