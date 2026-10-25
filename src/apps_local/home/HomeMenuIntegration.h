#pragma once

#include "HomeMenuStore.h"
#include "HomeScreen.h"

enum class HomeMenuItem;
class GfxRenderer;
class MappedInputManager;
namespace shelf {
struct Item;
}
namespace homemenu {
const shelf::Item* shortcutItem(Id id);
const char* label(Id id, bool plugins = false);
homeui::Icon icon(Id id);
HomeMenuItem item(Id id);
Id idFor(HomeMenuItem item);
int count(bool services);
bool launchShortcut(Id id, GfxRenderer& renderer, MappedInputManager& input);
}  // namespace homemenu
