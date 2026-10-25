#pragma once
#include <I18n.h>

#include "Cards.h"
namespace cardsui {
inline const char* suitName(cards::Suit value) {
  switch (value) {
    case cards::Suit::Clubs:
      return tr(STR_UI_CLUBS_B6FA15);
    case cards::Suit::Diamonds:
      return tr(STR_UI_DIAMONDS_6CB5FD);
    case cards::Suit::Spades:
      return tr(STR_UI_SPADES_E2DCE5);
    case cards::Suit::Hearts:
      return tr(STR_UI_HEARTS_29FBCB);
  }
  return "";
}
}  // namespace cardsui
