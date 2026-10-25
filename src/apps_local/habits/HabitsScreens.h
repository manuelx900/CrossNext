#pragma once
#include <I18n.h>

#include "../ui/ToyboxScreen.h"
#include "HabitsCore.h"

namespace habitsui {
namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionBack = 1,
  ActionAdd,
  ActionToggleToday,
  ActionOpen,
  ActionPage,
  ActionPreset,
  ActionCustom,
  ActionDate,
  ActionMenu,
  ActionRename,
  ActionDelete,
  ActionConfirmDelete,
  ActionDay,
  ActionHistory,
  ActionToggleDay,
  ActionRetry,
  ActionDismiss,
  ActionBrowseDay,
  ActionCalendar
};

constexpr UiLabel kPresets[] = {trLabel(STR_HABITS_EXERCISE), trLabel(STR_HABITS_EAT), trLabel(STR_HABITS_WATER),
                                trLabel(STR_HABITS_READ), trLabel(STR_HABITS_SLEEP)};
constexpr int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

struct TodayModel {
  const habits::Store* store = nullptr;
  int today = -1;
  int page = 0;
  bool manualDate = false;
  int displayedDay = 0;
  int focused = -1;
};

struct DetailModel {
  const habits::Store* store = nullptr;
  int index = 0;
  int today = -1;
  int historyStart = -1;
  int selectedDay = -1;
};

int todayCapacity(const fui::DeviceContext& device);
int presetCapacity(const fui::DeviceContext& device);
void buildToday(toybox::Screen& screen, const TodayModel& model);
void buildPresets(toybox::Screen& screen, int page);
int calendarDayAt(const fui::DeviceContext&, const DetailModel&, int x, int y);
void buildDetail(toybox::Screen& screen, const DetailModel& model);
void buildMenu(toybox::Screen& screen, const char* name);
void buildDelete(toybox::Screen& screen, const char* name);
void buildMessage(toybox::Screen& screen, const char* title, const char* message, fui::ActionId action,
                  const char* button);

}  // namespace habitsui
