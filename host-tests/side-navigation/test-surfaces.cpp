#define main existingUiSuiteMain
#include "../ui/test_ui.cpp"
#undef main

int main() {
  checksRun = checksFailed = 0;
  I18N.setLanguage(Language::EN);
  for (int inset : {0, 8, 13, 20}) {
    for (int hidden = 0; hidden <= 6; ++hidden) {
      for (int visible = 1; visible <= 13; ++visible) {
        solitaire::Game game;
        solitaire::Game::Save save;
        bool used[52]{};
        const int pile = solitaire::kFirstTableau;
        for (int n = 0; n < visible; ++n) {
          const int rank = 12 - n;
          const auto suit = rank % 2 ? solitaire::Suit::Hearts : solitaire::Suit::Spades;
          const auto card = solitaire::makeCard(suit, rank);
          save.piles[pile][hidden + n] = solitaire::faceUp(card);
          used[int(suit) * 13 + rank] = true;
        }
        int down = 0;
        for (int n = 0; n < 52; ++n) {
          if (used[n]) continue;
          const auto card = solitaire::makeCard(static_cast<solitaire::Suit>(n / 13), n % 13);
          if (down < hidden) {
            save.piles[pile][down++] = solitaire::faceDown(card);
          } else {
            const int destination = save.counts[solitaire::kStockPile] < 24   ? solitaire::kStockPile
                                    : save.counts[solitaire::kWastePile] < 24 ? solitaire::kWastePile
                                                                              : solitaire::kFirstTableau + 6;
            save.piles[destination][save.counts[destination]++] =
                destination == solitaire::kStockPile ? solitaire::faceDown(card) : solitaire::faceUp(card);
          }
        }
        save.counts[pile] = hidden + visible;
        CHECK(game.restore(save));
        FakeTarget target;
        toybox::Interactions interactions;
        auto ctx = solitaireDevice();
        ctx.safeArea.bottom = inset;
        const fui::InputSnapshot empty{};
        toybox::Frame frame(target, ctx, empty, interactions);
        fui::ThemeTokens tokens;
        auto screen = solitaireScreen(frame, tokens);
        solitaireui::Layout layout;
        solitaireui::BoardModel model;
        model.game = &game;
        solitaireui::buildBoard(screen, model, layout);
        CHECK(!interactions.overflowed());
        int cursor = -1;
        for (size_t n = 0; n < interactions.count(); ++n) {
          const auto& hit = interactions.data()[n];
          if (fui::acceptsInput(hit.inputMask, fui::InputCoordinates) && hit.rect.x == layout.x[pile]) cursor = n;
        }
        CHECK(cursor >= 0);
        if (cursor < 0) continue;
        interactions.setFocusedIndex(cursor - 1);
        for (int card = hidden; card < hidden + visible; ++card) {
          fui::InputSnapshot next;
          next.focusNext = true;
          CHECK(!interactions.route(next));
          fui::Rect focused;
          CHECK(interactions.buttonFocusRect(focused));
          CHECK(layout.cardAt(game, pile, focused.y + focused.height / 2) == card);
        }
      }
    }
  }
  for (const auto size : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    FakeTarget target;
    toybox::Interactions interactions;
    auto ctx = device();
    ctx.width = size.width;
    ctx.height = size.height;
    const fui::InputSnapshot empty{};
    toybox::Frame frame(target, ctx, empty, interactions);
    toybox::Screen screen(frame);
    picrossui::MenuModel model;
    picrossui::PickerLayout layout;
    picrossui::buildPicker(screen, model, layout);
    CHECK(!interactions.overflowed());
    int tiles = 0;
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& hit = interactions.data()[n];
      if (!fui::acceptsInput(hit.inputMask, fui::InputCoordinates)) continue;
      CHECK(layout.indexAt(hit.rect.x + hit.rect.width / 2, hit.rect.y + hit.rect.height / 2) >= 0);
      ++tiles;
    }
    CHECK(tiles == layout.count);
  }
  std::printf("Real navigation surfaces: %d checks, %d failures\n", checksRun, checksFailed);
  return checksFailed ? 1 : 0;
}
