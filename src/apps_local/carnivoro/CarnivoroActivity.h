#pragma once
#include "CarnivoroScreens.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
class CarnivoroActivity final : public Activity, protected UiAppHost {
 public:
  CarnivoroActivity(GfxRenderer& r, MappedInputManager& i) : Activity("Carnivoro", r, i), UiAppHost(r) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void prepareForSleep() override;

 private:
  carnivoro::State state, pending, verified;
  carnivoroui::Model model;
  carnivoroui::Geometry geometry;
  pagedtext::Work wrap;
  uint8_t wire[carnivoro::kSaveBytes]{};
  enum class PendingOperation : uint8_t { None, Accusation, Replay, Reset };
  PendingOperation pendingOperation = PendingOperation::None;
  bool writable = true, dirty = false;
  unsigned long changedAt = 0;
  static void screen(UiScreen&, void*);
  enum class ReadResult : uint8_t { Valid, Invalid, Future, IoError };
  ReadResult read(const char*, carnivoro::State&);
  bool save(const carnivoro::State&);
  void load();
  bool flush();
  void changed();
  void dispatch(int, int = 0);
  void openCase(int);
  void back();
};
