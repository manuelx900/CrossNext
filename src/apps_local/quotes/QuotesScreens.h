#pragma once
#include "../ui/PagedText.h"
#include "../ui/ToyboxScreen.h"
namespace quotesui {
namespace fui = freeink::ui;
enum : fui::ActionId {
  Tab = 1,
  Sort,
  Open,
  Favorite,
  Page,
  TextPage,
  QuotePage,
  TextSettings,
  PickFont,
  PickSize,
  SetAlignment,
  SaveTextSettings,
  Options,
  ConfirmRemove,
  Cancel,
  Back,
  BookTab
};
struct Row {
  char title[192]{}, author[128]{}, preview[320]{};
  int index = 0, page = 0, count = 0;
};
struct ListModel {
  const Row* rows = nullptr;
  int visible = 0, total = 0, books = 0, favorites = 0, tab = 0, page = 0, pages = 1;
  bool inBook = false, oldest = false, bookFavoritesOnly = false;
  const char* title = "";
  const char* author = "";
  int bookTotal = 0, bookFavorites = 0;
};
inline constexpr int kMaxRows = 6;
struct TextSettingsModel {
  const char* font = "";
  uint8_t pointSize = 14, alignment = 0;
  bool saveFailed = false;
};
void textSettings(toybox::Screen&, const TextSettingsModel&);
int capacity(const fui::DeviceContext&, bool books = false);
const char* savedDateFormat();
void list(toybox::Screen&, const ListModel&);
struct DetailModel {
  const char* title = "";
  const char* author = "";
  const char* body = "";
  const char* saved = "";
  const char* dateFormat = nullptr;
  int page = 0, bodyPage = 0, quoteIndex = 0, quoteCount = 1;
  pagedtext::Alignment alignment = pagedtext::Alignment::Left;
  bool favorite = false;
};
int detail(toybox::Screen&, const DetailModel&, pagedtext::Work&);
void message(toybox::Screen&, const char*, bool confirm = false);
}  // namespace quotesui
