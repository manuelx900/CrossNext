#pragma once
#include "../ui/PagedText.h"
#include "../ui/ToyboxScreen.h"
namespace quotesui {
namespace fui = freeink::ui;
enum : fui::ActionId { Tab = 1, Sort, Open, Favorite, Page, Remove, ConfirmRemove, Cancel, Back };
struct Row {
  char title[192]{}, author[128]{}, preview[320]{};
  int index = 0, page = 0, count = 0, favorites = 0;
  bool favorite = false;
};
struct ListModel {
  const Row* rows = nullptr;
  int visible = 0, total = 0, books = 0, favorites = 0, tab = 0, page = 0, pages = 1;
  bool inBook = false, oldest = false;
  const char* title = "";
  const char* author = "";
  int bookTotal = 0, bookFavorites = 0;
};
int capacity(const fui::DeviceContext&);
void list(toybox::Screen&, const ListModel&);
struct DetailModel {
  const char* title = "";
  const char* author = "";
  const char* body = "";
  const char* saved = "";
  int page = 0, index = 0, total = 0, bodyPage = 0;
  bool favorite = false;
};
int detail(toybox::Screen&, const DetailModel&, pagedtext::Work&);
void message(toybox::Screen&, const char*, bool confirm = false);
}  // namespace quotesui
