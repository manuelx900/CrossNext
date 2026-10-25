#include <cstdio>

#include "apps_local/study/StudyImageView.h"
#include "apps_local/ui/Rotation.h"
#include "apps_local/ui/SectionNavigation.h"
#include "util/HomeButtonInput.h"

static int checks = 0, failures = 0;
static void check(bool value) {
  ++checks;
  if (!value) ++failures;
}
struct Buttons {
  enum class Button { Back, Confirm, NavNext, NavPrevious };
  bool held = false, released = false, longPress = false, back = false;
  uint32_t duration = 0;
  bool wasReleased(Button b) const { return b == Button::Back ? back : b == Button::NavNext && released; }
  bool isPressed(Button b) const { return b == Button::NavNext && held; }
  bool wasLongPressed(Button b, unsigned) const { return b == Button::NavNext && longPress; }
  uint32_t getHeldTime() const { return duration; }
};
int main() {
  using Input = sectionnav::Input;
  using Kind = Input::Kind;
  Input input;
  Input::Event event{};
  for (int n = 0; n < 10000; ++n) check(input.push(Kind::Step, 1));
  check(input.pop(event) && event.kind == Kind::Step && event.amount == 10000);
  check(!input.pop(event));
  input.push(Kind::Step, 3);
  input.push(Kind::Step, -2);
  input.push(Kind::Section, 1);
  check(input.pop(event, 1) && event.amount == 1 && event.kind == Kind::Step);
  check(input.pop(event, 1) && event.amount == 1);
  check(input.pop(event, 1) && event.amount == 1);
  check(input.pop(event) && event.amount == -2);
  check(input.pop(event) && event.kind == Kind::Section);
  Buttons keys;
  keys.held = keys.longPress = true;
  keys.duration = 350;
  check(input.capture(keys, 350));
  check(input.pop(event) && event.kind == Kind::Section && event.amount == 1);
  keys.longPress = false;
  keys.duration = 400;
  check(input.capture(keys, 400) && !input.pop(event));
  keys.duration = 650;
  check(input.capture(keys, 650));
  check(input.pop(event) && event.kind == Kind::Section);
  input.begin(true);
  keys.held = false;
  keys.released = true;
  check(input.capture(keys, 700) && !input.pop(event));
  check(input.capture(keys, 800));
  check(input.pop(event) && event.kind == Kind::Step);
  input.push(Kind::Step, 7);
  keys.back = true;
  check(input.capture(keys, 900));
  check(input.pop(event) && event.kind == Kind::Back && !input.pop(event));

  for (int initial = 0; initial < 4; ++initial)
    for (bool right : {false, true}) {
      const int next = rotation::toggle(initial, initial, right);
      check(next == (initial + (right ? 1 : 3)) % 4);
      check(rotation::toggle(next, initial, right) == initial);
      check(rotation::pair(rotation::pair(initial, right), right) == initial);
    }
  for (bool right : {false, true}) {
    study::ImageView view;
    view.rotate(right);
    const study::ImageRef corners{1, 3, 2, 1};
    const auto placement = study::placeImage(corners, 0, 0, 2, 3, view);
    int sx = -1, sy = -1;
    check(placement.sourceAt(0, 0, corners, sx, sy));
    check(right ? (sx == 0 && sy == 1) : (sx == 2 && sy == 0));
    view.rotate(right);
    check(view.turns == 0);
  }
  const study::ImageRef image{1, 1086, 650, 136};
  for (int turns = 0; turns < 4; ++turns)
    for (int zoom = 0; zoom < 4; ++zoom)
      for (int width : {100, 480, 800})
        for (int height : {90, 300, 600}) {
          study::ImageView view;
          view.turns = uint8_t(turns);
          view.zoom = uint8_t(zoom);
          const auto placement = study::placeImage(image, 0, 0, width, height, view);
          const int extraX = std::max(0, placement.width - width), extraY = std::max(0, placement.height - height);
          for (int n = 0; n < 4; ++n) study::panImageThird(image, width, height, view, 1, 1);
          check(view.panX == -(extraX + 1) / 2 && view.panY == -(extraY + 1) / 2);
          for (int n = 0; n < 3; ++n) study::panImageThird(image, width, height, view, -1, -1);
          check(view.panX == extraX / 2 && view.panY == extraY / 2);
          check(!study::panImageThird(image, width, height, view, -1, -1));
        }
  check(unsigned(HomeButtonAction::OpenShortcut) == 13 && unsigned(HomeButtonAction::OpenDoubleShortcut) == 14 &&
        unsigned(HomeButtonAction::OpenLongShortcut) == 15);
  HomeButtonInput home;
  check(home.update(100, true, false, false, false, HomeButtonAction::Home, HomeButtonAction::OpenDoubleShortcut,
                    HomeButtonAction::OpenLongShortcut) == HomeButtonAction::Ignore);
  check(home.update(200, true, false, false, false, HomeButtonAction::Home, HomeButtonAction::OpenDoubleShortcut,
                    HomeButtonAction::OpenLongShortcut) == HomeButtonAction::OpenDoubleShortcut);
  check(home.update(500, false, true, false, false, HomeButtonAction::Home, HomeButtonAction::OpenDoubleShortcut,
                    HomeButtonAction::OpenLongShortcut) == HomeButtonAction::OpenLongShortcut);
  std::printf("Interface controls: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
