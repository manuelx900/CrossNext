#pragma once

#include <FreeInkUI.h>
#include <I18n.h>

#include <atomic>
#include <cstring>

#include "../../../lib/GfxRenderer/PaintClock.h"
#include "KeyboardStyle.h"

class MappedInputManager;

namespace keyboardview {
namespace fui = freeink::ui;

// Activity-owned, bounded modal surface. Capture precedes the owner's render
// lock, so a release observed during a slow e-ink paint keeps its paint stamp.
class Alternates {
 public:
  // Twenty-four choices cover a complete Vietnamese vowel or kana group.
  // The extra 240 bytes stay in the owner workspace; holding never allocates.
  static constexpr unsigned Capacity = 24;
  static constexpr int16_t FirstChoice = 30000;
  struct Contact {
    uint32_t painted = 0;
    int16_t x = 0, y = 0;
    bool held = false, ended = false, tap = false;
    bool cancel = false, next = false, previous = false, confirm = false;
  };
  struct Result {
    bool changed = false;
    char output[16]{};
  };
  bool active() const { return phase_.load() != Phase::Closed; }
  void reset();
  bool open(const fui::KeyboardKey& key, Language language, fui::Rect anchor, bool physical = false,
            bool genericLetters = true, bool allowHalfSpace = false);
  bool capture(const MappedInputManager& input);
  bool capture(Contact contact);
  unsigned count() const { return count_; }
  const char* choice(unsigned index) const { return index < count_ ? choices_[index] : ""; }

  template <size_t N>
  bool open(const fui::KeyboardLayout& layout, int value, Language language,
            const fui::InteractionBuffer<N>& interactions, bool physical = false, bool genericLetters = true,
            bool allowHalfSpace = false) {
    for (unsigned row = 0; row < layout.rowCount; ++row)
      for (unsigned col = 0; col < layout.rows[row].count; ++col) {
        const auto& key = layout.rows[row].keys[col];
        if (key.value != value || key.kind != fui::KeyKind::Normal) continue;
        const auto* hits = interactions.publishedData();
        for (size_t i = 0; i < interactions.publishedCount(); ++i)
          if (hits[i].value == value)
            return open(key, language, hits[i].rect, physical, genericLetters, allowHalfSpace);
      }
    return false;
  }

  template <size_t N>
  Result poll(const fui::InteractionBuffer<N>& interactions) {
    Result result;
    while (queued_ && phase_.load() == Phase::Open) {
      const auto contact = queue_[first_];
      first_ = (first_ + 1) % std::size(queue_);
      --queued_;
      const auto* hits = interactions.publishedData();
      int hit = -1;
      for (size_t i = interactions.publishedCount(); i > 0; --i) {
        const auto& item = hits[i - 1];
        if (item.value >= FirstChoice && item.value < FirstChoice + int(count_) &&
            item.rect.contains(contact.x, contact.y)) {
          hit = item.value - FirstChoice;
          break;
        }
      }
      consume(contact, hit, result);
    }
    return result;
  }

  template <size_t N>
  void draw(fui::Frame<N>& frame, fui::Rect bounds, fui::ActionId action, fui::FontId font, bool gameStyle = false) {
    if (phase_.load() == Phase::Closed) return;
    if (phase_.load() == Phase::Open) {
      constexpr int gap = 4, padding = 6, height = 52;
      int keyWidth = 44;
      for (unsigned i = 0; i < count_; ++i) {
        if (std::strcmp(choices_[i], "\xe2\x80\x8c")) continue;
        keyWidth = std::max(keyWidth, int(frame.target().measureText(font, tr(STR_KEY_HALF_SPACE), {}).width) + 12);
        break;
      }
      const int columns = std::max(1, std::min(int(count_), (bounds.width - 2 * padding + gap) / (keyWidth + gap)));
      const int rows = (count_ + columns - 1) / columns;
      const int width = std::min(int(bounds.width), columns * (keyWidth + gap) + 2 * padding - gap);
      const int totalHeight = rows * (height + gap) + 2 * padding - gap;
      const int x = std::clamp(int(anchor_.x + anchor_.width / 2 - width / 2), int(bounds.x),
                               std::max(int(bounds.x), int(bounds.right()) - width));
      const int y = std::clamp(int(anchor_.y - totalHeight - 6), int(bounds.y),
                               std::max(int(bounds.y), int(bounds.bottom()) - totalHeight));
      const auto panel = fui::makeRect(x, y, width, totalHeight);
      frame.target().fill(panel, fui::Paint::solid(fui::Color::White), gameStyle ? 0 : 10);
      frame.target().stroke(panel, fui::Paint::solid(fui::Color::Black), 2, gameStyle ? 0 : 10);
      button_.action = action;
      button_.styles = characterKeys;
      button_.text.font = font;
      button_.radius = gameStyle ? 0 : 8;
      button_.minTouchSize = 44;
      for (unsigned i = 0; i < count_; ++i) {
        char label[24];
        button_.label = displayLabel(choices_[i], label);
        char upper[24];
        if (gameStyle) {
          upperLabel(button_.label, language_, upper, sizeof(upper));
          button_.label = upper;
        }
        button_.value = FirstChoice + i;
        button_.state = int(i) == selected_ ? fui::StateSelected : fui::StateNormal;
        const auto rect = fui::makeRect(x + padding + (i % columns) * ((width - 2 * padding + gap) / columns),
                                        y + padding + (i / columns) * (height + gap),
                                        (width - 2 * padding - (columns - 1) * gap) / columns, height);
        fui::button(frame, rect, button_);
      }
      button_.label = nullptr;
    }
    if (!built_.load()) {
      builtAt_.store(paintclock::painted());
      built_.store(true);
    }
  }

 private:
  enum class Phase : uint8_t { Closed, Open, Closing };
  std::atomic<Phase> phase_{Phase::Closed};
  std::atomic<bool> built_{false};
  std::atomic<uint32_t> builtAt_{0};
  char choices_[Capacity][16]{};
  Language language_ = Language::EN;
  fui::Rect anchor_{};
  fui::ButtonProps button_{};
  Contact queue_[8]{};
  uint8_t first_ = 0, queued_ = 0, count_ = 0;
  int8_t selected_ = 0;
  int16_t lastX_ = 0, lastY_ = 0;
  bool openingTouch_ = false, openingConfirm_ = false, closingContact_ = false, trackedContact_ = false;
  bool captureOpeningTouch_ = false;
  bool visible(uint32_t paint) const { return built_.load() && int32_t(paint - builtAt_.load()) > 0; }
  void add(const char* value, size_t length);
  void addGroup(const char* group);
  void close(bool contact);
  void consume(Contact contact, int hit, Result& result);
  static const char* displayLabel(const char* value, char (&label)[24]);
};
static_assert(sizeof(Alternates) <= 2048, "Keep the alternatives workspace bounded");
}  // namespace keyboardview
