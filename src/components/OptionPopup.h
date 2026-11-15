#pragma once
#include <I18n.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "RevealedInteractions.h"
#include "apps_local/ui/LightOptionMenu.h"
#include "apps_local/ui/PageIndicator.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

// Modal option picker drawn over the current screen (no clear) via
// fui::optionDialog. Touch hit-testing is the SDK's InteractionBuffer: each
// render registers the option buttons (plus a chrome guard rect) on the render
// task, and handleInput routes touch snapshots against that table on the loop
// task, gated by the uiReady handshake (same pattern as UiListActivity).
// render() builds into InteractionBuffer's non-published generation
// (beginPublishCycle()) and publishes it only once every hit() call for the
// frame is done (publish()), so handleInput()'s routePublished()/
// publishedData() reads on the loop task always see a complete table, never
// one render is mid-rebuilding. uiReady closes when show() replaces the
// popup's data, then stays open across ordinary repaints after the first
// publication so a release cannot be dropped during a highlight repaint.
class OptionPopup {
 public:
  void show(StrId titleId, const StrId* optionIds, int optionCount, int currentIndex,
            std::function<void(int)> onSelect) {
    title = I18N.get(titleId);
    headline.clear();
    ownedStrings.resize(optionCount);
    for (int i = 0; i < optionCount; i++) {
      ownedStrings[i] = I18N.get(optionIds[i]);
    }
    activate(currentIndex, std::move(onSelect));
  }

  void show(const char* titleStr, const char* const* options, int optionCount, int currentIndex,
            std::function<void(int)> onSelect) {
    title = titleStr;
    headline.clear();
    ownedStrings.resize(optionCount);
    for (int i = 0; i < optionCount; i++) {
      ownedStrings[i] = options[i];
    }
    activate(currentIndex, std::move(onSelect));
  }

  // As above, plus a subject line inside the dialog (a book or event title).
  // It wraps to several lines under the caption; the dialog grows to fit.
  void show(const char* titleStr, const char* headlineStr, const char* const* options, int optionCount,
            int currentIndex, std::function<void(int)> onSelect) {
    show(titleStr, options, optionCount, currentIndex, std::move(onSelect));
    headline = headlineStr ? headlineStr : "";
  }

  // Message dialog: a wrapped body under the (optional) title, like the
  // Wi-Fi forget-network prompt. Pass an empty title for a message-only look.
  void showMessage(const char* titleStr, const char* messageStr, const char* const* options, int optionCount,
                   int currentIndex, std::function<void(int)> onSelect, int maxMessageLines = 0) {
    show(titleStr, options, optionCount, currentIndex, std::move(onSelect));
    message = messageStr ? messageStr : "";
    messageLineLimit = std::clamp(maxMessageLines, 0, 32);
  }

  void show(StrId titleId, const std::vector<std::string>& options, int currentIndex,
            std::function<void(int)> onSelect) {
    title = I18N.get(titleId);
    headline.clear();
    ownedStrings = options;
    activate(currentIndex, std::move(onSelect));
  }

  void show(StrId titleId, std::vector<std::string>&& options, int currentIndex, std::function<void(int)> onSelect) {
    title = I18N.get(titleId);
    headline.clear();
    ownedStrings = std::move(options);
    activate(currentIndex, std::move(onSelect));
  }

  bool handleInput(MappedInputManager& input, const std::function<void()>& requestUpdate) {
    if (!active) return false;

    const freeink::ui::InputSnapshot snap = touchSnapshotFrom(input);
    const auto swipe = input.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right) {
      if (uiReady && interactions.publishedRoutable()) {
        interactions.routePublished(snap);
        const bool next = swipe == MappedInputManager::SwipeDir::Left;
        for (size_t i = 0; i < interactions.publishedCount(); ++i) {
          const auto& hit = interactions.publishedData()[i];
          if (hit.action == ACTION_PAGE && (next ? hit.value > selectedIndex : hit.value < selectedIndex)) {
            selectedIndex = hit.value;
            requestUpdate();
            break;
          }
        }
      }
      return true;
    }
    if (snap.touchPressed || snap.touchReleased || snap.touchHeld) {
      // Interactions are registered on the render task; only route once the
      // first render after show() has populated the table (uiReady handshake).
      if (uiReady) {
        // Ask the gate BEFORE routing, not after. A suppressed tap comes back
        // from routePublished() as a default-constructed event, which is
        // indistinguishable from "landed on nothing" -- and landing on nothing
        // is what the outside-tap branch below reads as DISMISS. Falling
        // through would turn a tap the panel could not have shown into a
        // dismissal, which is the gate causing a wrong action rather than
        // preventing one.
        if (!interactions.publishedRoutable()) return true;
        const freeink::ui::ActionEvent event = interactions.routePublished(snap);
        if (event && event.action == ACTION_PAGE) {
          selectedIndex = event.value;
          requestUpdate();
          return true;
        }
        if (event && event.action == ACTION_OPTION) {
          // Tap released on an option: select it, fire, dismiss.
          selectedIndex = event.value;
          active = false;
          if (onSelectCallback) onSelectCallback(selectedIndex);
          haptic_feedback::touchAction();
          requestUpdate();
          return true;
        }
        if (event && event.action == ACTION_CHROME) {
          // Taps on the dialog chrome (title, padding) keep the popup open.
          return true;
        }
        if (snap.touchReleased && snap.touchX >= 0) {
          // Tap released outside the dialog: dismiss without firing. Swipe-end
          // releases arrive with -1,-1 coords and fall through (no dismiss).
          active = false;
          haptic_feedback::touchAction();
          requestUpdate();
          return true;
        }
        if (snap.touchPressed) {
          // Touch-down on an option moves the highlight (route() latched the
          // hit as the active interaction; read it back, no re-hit-testing).
          const int16_t idx = interactions.activeIndex();
          if (idx >= 0) {
            const freeink::ui::Interaction& hit = interactions.publishedData()[idx];
            if (hit.action == ACTION_OPTION && selectedIndex != hit.value) {
              selectedIndex = hit.value;
              requestUpdate();
            }
          }
        }
      }
      return true;
    }

    return handleButtons(input.wasPressed(MappedInputManager::Button::NavPrevious),
                         input.wasPressed(MappedInputManager::Button::NavNext),
                         input.wasReleased(MappedInputManager::Button::Confirm),
                         input.wasReleased(MappedInputManager::Button::Back), requestUpdate);
  }

  bool handleButtons(const bool previous, const bool next, const bool confirm, const bool back,
                     const std::function<void()>& requestUpdate) {
    if (!active) return false;
    const int count = static_cast<int>(ownedStrings.size());
    if (previous) {
      selectedIndex = (selectedIndex - 1 + count) % count;
      requestUpdate();
      return true;
    } else if (next) {
      selectedIndex = (selectedIndex + 1) % count;
      requestUpdate();
      return true;
    } else if (confirm) {
      active = false;
      if (onSelectCallback) onSelectCallback(selectedIndex);
      requestUpdate();
      return true;
    } else if (back) {
      active = false;
      requestUpdate();
      return true;
    }
    return true;
  }

  bool processRender(GfxRenderer& renderer, const MappedInputManager& input) const {
    if (!active) return false;
    const auto popupLabels = input.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, popupLabels.btn1, popupLabels.btn2, popupLabels.btn3, popupLabels.btn4);
    render(renderer);
    renderer.displayBuffer();
    return true;
  }

  void render(const GfxRenderer& renderer) const {
    if (!active) return;
    namespace fui = freeink::ui;

    // Per-render target: a GfxRendererTarget is a renderer reference plus
    // three font ids, so rebuilding it here is trivially cheap and always
    // tracks the live orientation and uiScale fonts; a target held across
    // show() would stale-bind both after a rotation or scale change.
    fui::GfxRendererTarget target = makeUiTarget(renderer);
    refreshSharedUiThemeTokens(target);
    // Frame stores a const DeviceContext&; keep it in a local that outlives
    // the frame (a deviceContext() temporary would dangle).
    const fui::DeviceContext device = target.deviceContext();
    // Routing happens on the loop task against the member buffer; the frame
    // itself never dispatches, so it gets an empty snapshot.
    const fui::InputSnapshot noInput{};

    // Builds into the generation handleInput()'s routePublished()/
    // publishedData() aren't currently reading, so the loop task never sees
    // this table mid-rebuild — see publish() below and
    // InteractionBuffer::beginPublishCycle().
    // Before beginPublishCycle() and before the Frame's constructor clears:
    // the table still published at this moment is the one the panel has been
    // showing, and that is what the next tap is measured against.
    interactions.beginBuild();
    interactions.beginPublishCycle();
    fui::Frame<INTERACTION_CAPACITY> frame(target, device, noInput, interactions);

    pixelui::FontScope fonts(target, const_cast<GfxRenderer&>(renderer), I18N.getLanguage() != Language::EN);
    if (!chrome::lightHeader) target.setFont(pixelui::kSmall, toybox::kTileFontId);
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int count = static_cast<int>(ownedStrings.size());

    if (chrome::lightHeader) {
      lightui::ChoiceMenu menu;
      menu.title = title.c_str();
      menu.headline = headline.c_str();
      menu.message = message.c_str();
      menu.labels = ownedStrings.data();
      menu.count = count;
      menu.current = currentIndex;
      menu.focus = selectedIndex;
      menu.guard = ACTION_CHROME;
      menu.pageAction = ACTION_PAGE;
      menu.optionAction = ACTION_OPTION;
      if (messageLineLimit) menu.messageMaxLines = messageLineLimit;
      lightui::choiceMenu(frame, device.safeRect(), menu);
      interactions.publish();
      uiReady = true;
      return;
    }

    fui::DialogOption options[MAX_PAGE_OPTIONS];
    fui::OptionDialogProps props;
    props.title = title.empty() ? nullptr : title.c_str();
    props.headline = headline.empty() ? nullptr : headline.c_str();
    if (!message.empty()) {
      props.message = message.c_str();
      props.messageText.font = pixelui::kSmall;
      props.messageText.align = fui::TextAlign::Center;
      props.messageText.maxLines = messageLineLimit ? messageLineLimit : 6;
    }
    props.options = options;
    props.optionCount = static_cast<uint8_t>(std::min(count, MAX_PAGE_OPTIONS));
    props.verticalOptions = true;
    // Touch only: physical buttons stay on the legacy wrap/confirm path above,
    // so the buffer never competes with it for focus/confirm dispatch.
    props.inputMask = fui::InputTouch;
    props.titleText.font = pixelui::kSmall;
    props.titleText.bold = true;
    props.titleText.align = fui::TextAlign::Center;
    // Captions like "Remove from Recent Books?" overflow the narrow portrait
    // dialog in one line; let them wrap and the panel grow.
    props.titleText.maxLines = 2;
    props.headlineText.font = pixelui::kSmall;
    props.headlineText.align = fui::TextAlign::Center;
    props.headlineText.maxLines = 3;
    props.buttonText.font = fui::GfxRendererTarget::FONT_BODY;
    const int16_t innerPadding = static_cast<int16_t>(metrics.optionPopupInnerPadding);
    props.padding = fui::Insets{innerPadding, innerPadding, innerPadding, innerPadding};
    props.gap = static_cast<int16_t>(metrics.optionPopupItemSpacing);
    props.buttonStyles = pixelui::buttonStyles();
    props.buttonText.font = pixelui::kSmall;
    props.buttonText.maxLines = 2;
    // defaultPopupStyles() has no border, so opt in using the per-theme frame metrics.
    props.styles = fui::defaultPopupStyles();
    props.styles.normal.border = fui::Paint::solid(fui::Color::Black);
    props.styles.normal.borderWidth = pixelui::kBorder;
    props.styles.normal.radius = 0;
    props.styles.selected = props.styles.normal;
    props.styles.focused = props.styles.normal;
    props.styles.active = props.styles.normal;
    props.styles.disabled = props.styles.normal;
    props.buttonHeight = fui::clampI16(std::max(52, int(target.lineHeight(pixelui::kSmall)) + 12));

    // Fixed fraction of the screen, clamped by the theme's side margins; the
    // old max-text-width sizing is gone, long labels wrap inside the buttons.
    const fui::Rect screen = device.safeRect();
    const int16_t width =
        fui::clampI16(std::min<int>(screen.width * 3 / 4, screen.width - metrics.optionPopupDialogSideMargin * 2));
    if (count > MAX_PAGE_OPTIONS)
      props.buttonHeight =
          fui::clampI16(std::max(int(props.buttonHeight), 2 * int(target.lineHeight(pixelui::kSmall)) + 12));
    else
      for (int i = 0; i < count; ++i) {
        const auto size =
            fui::measureWrappedText(target, ownedStrings[i].c_str(), props.buttonText, width - innerPadding * 2 - 24);
        props.buttonHeight = std::max<int16_t>(props.buttonHeight, size.height + 12);
      }
    const int available = screen.height - 24;
    int height = fui::optionDialogHeight(target, props, width);
    int capacity = std::min(count, MAX_PAGE_OPTIONS), first = 0;
    const int pagerHeight = 52;
    if (count > capacity || height > available) {
      props.optionCount = 1;
      for (auto* style : {&props.messageText, &props.headlineText, &props.titleText})
        while (style->maxLines > 1 && fui::optionDialogHeight(target, props, width) + pagerHeight > available)
          --style->maxLines;
      const int chromeHeight = fui::optionDialogHeight(target, props, width) - props.buttonHeight;
      capacity = std::clamp((available - chromeHeight - pagerHeight + props.gap) / (props.buttonHeight + props.gap), 1,
                            std::min(count, MAX_PAGE_OPTIONS));
      first = selectedIndex / capacity * capacity;
      // Keep the outer panel stable, including a short final page: the host
      // may repaint only the popup, without restoring the screen behind it.
      props.optionCount = static_cast<uint8_t>(capacity);
      height = fui::optionDialogHeight(target, props, width) + pagerHeight;
      props.optionCount = static_cast<uint8_t>(std::min(capacity, count - first));
    }
    for (int i = 0; i < props.optionCount; ++i) {
      options[i].label = ownedStrings[first + i].c_str();
      options[i].action = ACTION_OPTION;
      options[i].value = static_cast<int16_t>(first + i);
      options[i].state = first + i == selectedIndex ? fui::StateFocused : fui::StateNormal;
    }
    const fui::Rect dialogRect = fui::centeredRect(screen, fui::Size{width, fui::clampI16(height)});

    // Chrome guard first, options after: route() scans newest-first, so the
    // option buttons win inside the dialog and the guard absorbs the rest.
    frame.hit(dialogRect, ACTION_CHROME, 0, fui::InputTouch);
    if (capacity < count) {
      target.fill(dialogRect, fui::Paint::solid(fui::Color::White));
      target.stroke(dialogRect, fui::Paint::solid(fui::Color::Black), pixelui::kBorder);
      props.styles.normal.borderWidth = 0;
      fui::optionDialog(
          frame,
          fui::makeRect(dialogRect.x, dialogRect.y, dialogRect.width, fui::optionDialogHeight(target, props, width)),
          props);
      const auto pager = pageui::draw(
          target,
          fui::makeRect(dialogRect.x + 12, dialogRect.bottom() - pagerHeight, dialogRect.width - 24, pagerHeight - 8),
          first / capacity + 1, (count + capacity - 1) / capacity);
      target.stroke(dialogRect, fui::Paint::solid(fui::Color::Black), pixelui::kBorder);
      if (first > 0) frame.hit(pager.before, ACTION_PAGE, first - capacity, fui::InputTouch);
      if (first + capacity < count) frame.hit(pager.after, ACTION_PAGE, first + capacity, fui::InputTouch);
    } else
      fui::optionDialog(frame, dialogRect, props);
    // Atomically make this generation the one handleInput() reads, now that
    // every hit() call for this frame is done.
    interactions.publish();
    uiReady = true;
  }

  bool isActive() const { return active; }

  // Close without firing the callback (the surface under the popup is going
  // away, e.g. its host screen closes from outside the popup's own input).
  void dismiss() {
    active = false;
    onSelectCallback = nullptr;
  }

 private:
  // Only the visible page needs controls; labels remain in the existing snapshot.
  static constexpr int MAX_PAGE_OPTIONS = lightui::ChoiceMenu::MAX_VISIBLE;
  static constexpr size_t INTERACTION_CAPACITY = MAX_PAGE_OPTIONS + 3;
  static constexpr freeink::ui::ActionId ACTION_OPTION = 1;
  static constexpr freeink::ui::ActionId ACTION_CHROME = 2;
  static constexpr freeink::ui::ActionId ACTION_PAGE = 3;

  void activate(int currentIndex, std::function<void(int)> onSelect) {
    const int count = static_cast<int>(ownedStrings.size());
    this->currentIndex = currentIndex;
    selectedIndex = currentIndex >= 0 && currentIndex < count ? currentIndex : 0;
    onSelectCallback = std::move(onSelect);
    message.clear();
    messageLineLimit = 0;
    uiReady = false;
    active = count > 0;
  }

  bool active = false;
  std::string title;
  std::string headline;
  std::string message;
  std::vector<std::string> ownedStrings;
  int selectedIndex = 0, currentIndex = 0;
  int messageLineLimit = 0;
  std::function<void(int)> onSelectCallback;
  // Written by the render task (frame registration), routed by the loop task;
  // uiReady closes the rebuild window exactly like UiListActivity::uiReady.
  mutable paintclock::RevealedInteractions<INTERACTION_CAPACITY> interactions;
  mutable std::atomic<bool> uiReady{false};
};
