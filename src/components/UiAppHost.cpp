#include "UiAppHost.h"

#include "SideButtonNavigation.h"
#include "UiAppHelpers.h"
#include "UiListLayoutCache.h"

namespace fui = freeink::ui;

UiAppHost::UiAppHost(const GfxRenderer& renderer)
    : uiTarget(makeUiTarget(renderer)), app(uiTarget, uiTarget.deviceContext()) {}

void UiAppHost::resetUi() {
  uiReady = false;
  // A new screen is not live until the panel shows it, not merely until the
  // table is rebuilt below. See UiAppHost::revealed().
  reveal_.arm();
  applySharedUiTheme(app, uiTarget);
  sharedUiListLayoutCache().prepare();
}

void UiAppHost::renderUi() {
  const auto device = uiTarget.deviceContext();
  app.setDevice(device);
  const auto context = uint64_t(device.width) | (uint64_t(device.height) << 16) | (uint64_t(device.orientation) << 32) |
                       (uint64_t(I18N.getLanguage()) << 40);
  app.setListLayoutCache(sharedUiListLayoutCache().get(), context);
  app.render();
  // Stamped on every build while a reveal is pending, so a render() that
  // rebuilds several times before its single displayBuffer() (UiListActivity
  // does, up to 8 passes) measures from the LAST build rather than the first.
  reveal_.markBuilt();
  uiReady = true;
}

UiAppHost::TouchRoute UiAppHost::routeTouch(const MappedInputManager& input, const bool withLongPress,
                                            const bool routeHeld, const bool sideButtons) {
  TouchRoute result;  // named apart from route() — cppcheck flags the shadow
  if (input.isTouchscreenLocked()) {
    app.cancelTouchContact();
    if (!sideButtons) return result;
  }
  if (!uiReady || !revealed()) return result;
  result.snap = touchSnapshotFrom(input, withLongPress);
  if (sideButtons) sidebuttons::append(input, result.snap);
  if (!result.snap.touchPressed && !result.snap.touchReleased && !(routeHeld && result.snap.touchHeld) &&
      !result.snap.focusNext && !result.snap.focusPrev && !result.snap.confirm && !result.snap.prev &&
      !result.snap.next) {
    return result;
  }
  result.routed = true;
  result.event = route(result.snap);
  return result;
}

fui::ActionEvent UiAppHost::route(const fui::InputSnapshot& snap) {
  if (!uiReady || !revealed()) return {};
  const auto event = app.route(snap);
  // Releases activate controls; held/pressed frames only highlight or drag.
  // Empty/disabled hit regions produce no event and therefore no feedback.
  if (event && (snap.touchReleased || snap.swipeLeft || snap.swipeRight)) {
    haptic_feedback::touchAction(event.longPress);
  }
  return event;
}
