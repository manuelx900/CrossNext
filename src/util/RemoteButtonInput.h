#pragma once

// One logical click per main-loop frame; never carry it into a newly opened screen.
enum class RemoteButton { None, Confirm, Back, Next, Previous, SideLeft, SideRight, Power };

struct RemoteButtonInput {
  RemoteButton button = RemoteButton::None;
  bool used = false;

  bool click(const RemoteButton next) {
    if (next == RemoteButton::None || used) return false;
    used = true;
    button = next;
    return true;
  }
  void reset() {
    button = RemoteButton::None;
    used = false;
  }
};
