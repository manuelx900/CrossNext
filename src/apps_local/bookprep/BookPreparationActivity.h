#pragma once

#include <atomic>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "BookPreparation.h"
#include "BookPreparationScreens.h"

class BookPreparationActivity final : public Activity {
 public:
  BookPreparationActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("BookPreparation", renderer, input) {
    setInterfaceFontSection(menufont::Section::Settings);
    setEdgeBackEnabled(false);
  }
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void runIdleWork() override;
  void prepareForSleep() override;
  bool preventAutoSleep() override { return bookprep::running(phase.load()); }
  bool handleHomeGesture() override;

 private:
  enum class Command : uint8_t { None, Start, Cancel };
  std::atomic<Command> command{Command::None};
  std::atomic<bookprep::Phase> phase{bookprep::Phase::Ready};
  std::unique_ptr<bookprep::Job> job;
  // Fixed family/size snapshot (~8.4 KiB) shares the activity's checked allocation.
  // Registry rediscovery cannot invalidate names while a long job is running.
  bookprep::FontSelection fonts;
  bookprepui::View view = bookprepui::View::Options;
  ReaderRenderSpec baseSpec;
  EpubEngine engine = EpubEngine::Modern;
  int currentFont = 0, activeFont = -1, page = 0;
  bookprep::FontSelection::Cursor cursor;
  bookprep::SizeMode sizeMode = bookprep::SizeMode::Single;
  uint8_t formats = bookprep::AllFormats;
  uint8_t requestedPointSize = 14, activePointSize = 14;
  bool ready = false, preparationFontLoaded = false;
  uint32_t revision = 1;
  std::atomic<uint32_t> paintedRevision{0};
  const char* error = nullptr;
  char countsText[72]{}, detailText[128]{}, fontText[96]{};
  toybox::Interactions interactions;
  unsigned long lastPaint = 0;
  void action(freeink::ui::ActionId action, int value = 0);
  void configureJob();
  bool loadFont(int index, ReaderRenderSpec& spec);
  void restoreReaderFont();
  void show(bookprepui::View next);
};
