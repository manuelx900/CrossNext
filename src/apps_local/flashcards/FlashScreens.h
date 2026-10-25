#pragma once
#include "../ui/ToyboxScreen.h"
#include "FlashHistory.h"
#include "FlashProgress.h"
#include "FlashTextPage.h"
namespace studyui {
struct DeckModel;
}
namespace flashui {
namespace fui = freeink::ui;
inline constexpr fui::ActionId OpenDeck = 201, History = 202, Help = 203, Page = 204, Back = 205, Cards = 206,
                               OpenSession = 207, Filter = 208, Month = 209, ViewSession = 210, OpenCard = 211,
                               Rate = 212, Reveal = 213, Settings = 214;
struct DeckRow {
  const char* name = "";
  int total = 0, due = 0, fresh = 0;
  bool ready = false, folder = false;
};
struct LibraryModel {
  const DeckRow* rows = nullptr;
  int count = 0, totalDecks = 0, today = 0, page = 0, pages = 1;
  bool error = false;
};
struct HistoryRow {
  const char* deck = "";
  const char* date = "";
  flashcards::Session session;
};
struct HistoryModel {
  const HistoryRow* rows = nullptr;
  int count = 0, page = 0, pages = 1, sessions = 0, cards = 0;
  const char* month = "";
  const char* filter = "";
  bool error = false;
};
struct SessionModel {
  const char* deck = "";
  const char* date = "";
  flashcards::Session session;
  bool completed = false, error = false;
};
struct CardsModel {
  const char* deck = "";
  const char* labels[10]{};
  int count = 0, page = 0, pages = 1, perPage = 10;
  bool error = false;
};
struct ImportModel {
  flashcards::ImportProgress progress;
  uint32_t cards = 0, skipped = 0, elapsedMs = 0, tick = 0;
};
void header(toybox::Screen&, const char* title);
void importing(toybox::Screen&, const ImportModel&);
void library(toybox::Screen&, const LibraryModel&);
void deck(toybox::Screen&, const studyui::DeckModel&);
void history(toybox::Screen&, const HistoryModel&);
void session(toybox::Screen&, const SessionModel&);
void completed(toybox::Screen&, const SessionModel&);
void cards(toybox::Screen&, const CardsModel&);
void help(toybox::Screen&, int page);
int textPage(fui::DrawTarget&, fui::Rect, const char*, int, char*, size_t, TextPageCache* = nullptr, int fontKey = -1);
void interval(char*, size_t, int minutes, int days);
void duration(char*, size_t, uint64_t, bool known = true);
}  // namespace flashui
