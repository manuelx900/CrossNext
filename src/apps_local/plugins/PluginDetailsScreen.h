#pragma once

#include "../ui/FitText.h"
#include "PluginCatalogScreen.h"

namespace pluginui {
template <typename Screen, typename Spec>
void details(Screen& screen, const Spec& spec, const char* summary, fui::PublicationPageProps& controls,
             const toybox::HeaderStatus& status, fui::ActionId primaryAction, fui::ActionId moreAction) {
  header(screen, spec.header.c_str(), status);
  const bool wide = screen.device().width > screen.device().height;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const auto actions = screen.takeBottom(52, 14);
  fui::TextStyle title;
  title.font = pixelui::kSmall;
  title.maxLines = 2;
  title.bold = true;
  fittext::draw(screen.target(), screen.takeTop(wide ? 36 : 64, 8), spec.title.c_str(), title, 2);
  fui::TextStyle body;
  body.font = 4;
  body.maxLines = 2;
  const int line = screen.target().lineHeight(body.font);
  if (!spec.author.empty())
    screen.target().text(
        screen.takeTop(fui::measureWrappedText(screen.target(), spec.author.c_str(), body, screen.body().width).height,
                       6),
        spec.author.c_str(), body);
  if (!spec.source.empty()) screen.target().text(screen.takeTop(line * 2, 6), spec.source.c_str(), body);
  if (!spec.metadata.empty()) screen.target().text(screen.takeTop(line, 12), spec.metadata.c_str(), body);
  const auto divider = screen.takeTop(1, 12);
  screen.target().fill(divider, ink);
  auto description = screen.body();
  body.maxLines = std::clamp<int>(description.height / std::max(1, line), 1, 16);
  description.height =
      std::min(description.height, fui::measureWrappedText(screen.target(), summary, body, description.width).height);
  screen.target().text(description, summary, body);
  const auto button = [&](fui::ButtonProps& props, fui::Rect rect, const char* label, fui::ActionId action,
                          bool primary) {
    props = {};
    props.label = label;
    props.action = action;
    props.text.font = pixelui::kSmall;
    props.text.align = fui::TextAlign::Center;
    props.styles = pixelui::buttonStyles();
    props.styles.normal.borderWidth = props.styles.selected.borderWidth = props.styles.focused.borderWidth =
        props.styles.active.borderWidth = 1;
    if (primary) props.state = fui::StateSelected;
    screen.button(props, rect);
  };
  const bool more = !spec.description.empty();
  const int width = more ? (actions.width - 12) / 2 : actions.width;
  if (more)
    button(controls.more, fui::makeRect(actions.x, actions.y, width, actions.height), tr(STR_DETAILS), moreAction,
           false);
  button(controls.primary, fui::makeRect(actions.right() - width, actions.y, width, actions.height),
         spec.actionLabel.c_str(), primaryAction, true);
}
}  // namespace pluginui
