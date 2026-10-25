#pragma once
#include "../../activities/Activity.h"
#include "../ui/SectionNavigation.h"
#include "../ui/ToyboxScreen.h"
#include "HabitsCore.h"
#include "HabitsStore.h"

class HabitsActivity final : public Activity {
 public:
  HabitsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Habits", renderer, mappedInput) {}
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  sectionnav::Input navigation;
  enum class View : uint8_t { Today, Presets, Detail, Menu, Delete, Clock, StorageError, Notice };

  void load();
  void show(View view);
  void back();
  void notice(const char* message, View returnTo);
  void askName(bool rename);
  void askDate();
  void addHabit(const char* name);
  bool save();
  bool acceptName(habits::NameResult result, View returnTo);
  void refreshDate();
  int pageCapacity() const;
  static int deviceDay();

  // Activity-owned, bounded working storage: about 3.7KB for committed data,
  // a proposed edit and the repository's I/O buffer. No per-check-in allocation.
  habits::Store store_;
  habits::Store draft_;
  habits::Repository repository_;
  View view_ = View::Today;
  View noticeReturn_ = View::Today;
  const char* notice_ = "";
  int today_ = -1;
  int displayedDay_ = 0;
  int selected_ = 0;
  int selectedDay_ = -1;
  int historyStart_ = -1;
  int page_ = 0;
  int presetPage_ = 0;
  bool manualDate_ = false;
  unsigned long lastDatePoll_ = 0;
  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
