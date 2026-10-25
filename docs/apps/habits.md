# Habits

Open **Apps > Habits** to track daily habits. Tap **+ NEW** and choose a
suggestion, or **CUSTOM** to enter a name using the on-screen keyboard.
You can keep up to twelve habits, with names of up to 32 ASCII characters.

The daily screen has a date selector, a right-aligned completion count, and four compact habit cards per page. Portrait uses one column; landscape uses two columns. Each card shows its name, illustration and streak, without a repeated pending label. All five suggestions and the custom action fit on the new-habit
screen. Tap a habit's checkbox to mark the displayed day complete. Tap it again to undo. Tap the
name or arrow to open its progress screen. The list stays in creation order,
so checking in never moves the next habit away from your finger. Use the
page footer, the side buttons or vertical swipes to change pages.

The progress screen shows the current streak, best streak, lifetime check-ins,
and completion percentage over the last 30 days (or since the habit was
created, if more recent). Today is included in that percentage. A streak stays
alive while today is pending if yesterday was completed; a fully missed day
breaks it. Correcting an earlier day recalculates the streaks.

The calendar shows one full month at a time, with weekday columns and
previous/next month arrows. Filled dates are completed, outlined dates have no
check-in, and an inner border identifies today or the selected day. Tap a date,
then **MARK AS DONE** or **UNMARK DAY**. Future dates and dates before the habit was created cannot be edited.
The last 400 days remain editable; lifetime totals and streak records keep the
older results as well.

The gear on a habit's progress screen offers rename and delete. Renaming keeps
its history. Deleting requires confirmation and removes that habit's history.
Use the normal Back gesture to return to the daily list, then to Apps.

Habits reads the running system clock, with `HalClock` as a fallback if the
system date is unset. If the date is unset or earlier than saved history, it asks you
to check the date before recording anything. You can enter today's date as
`YYYY-MM-DD` without connecting to Wi-Fi. This manual date applies only to the
current Habits session; tap the date button to change it, and confirm it again
when reopening the app. Sync the device clock in Settings for automatic daily
rollover. A date change is painted before another check-in is accepted, so a
tap on yesterday's screen cannot accidentally change today's record. Habits
starts no network connection or background task.

Each edit is saved immediately through `HalStorage`. Two alternating files,
`/.crosspoint/habits-a.bin` and `/.crosspoint/habits-b.bin`, carry a version,
generation and checksum. A failed or interrupted write leaves the other copy
available. If neither copy is readable, the app preserves both and asks you to
check the card instead of replacing your history. Back up both files together.

The core, a proposed edit and the I/O buffer occupy about 3.7KB of bounded
activity-owned storage, in addition to the shared UI and keyboard. No tracking
containers grow during check-ins. `host-tests/habits/run.sh` checks dates,
streaks, history rollover, persistence faults and portrait/landscape controls.
