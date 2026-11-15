#pragma once
#include <FreeInkUI.h>

namespace collectionui {
inline constexpr int ReturnHome = -1;
// UiListActivity owns action 1; four shared routes fit the six-handler host.
enum Action : freeink::ui::ActionId { Tab = 2, Command, Options, Page };
enum CommandValue : int { ToggleView = 0, New, Menu, Save };
template <class App>
void bindActions(App& app, typename App::ActionHandler handler, void* context) {
  for (const auto action : {Tab, Command, Options, Page}) app.on(action, handler, context);
}
}  // namespace collectionui
