# Focus

Focus plans study tasks and records Pomodoro sessions. Start focus creates a new, paused Today task called Untitled. Press Start to begin counting; use the outlined pencil to rename it and −/+ to adjust planned sessions. Completing a session and completing a task are separate actions.

## Lists

Today, Projects and Tasks keep separate task lists. Creating or completing a task affects only its list. Projects contain their own tasks. Existing saves retain their tasks in their original projects; they are not copied into Today or Tasks. Today is a persistent list, not an automatically cleared daily schedule. The three areas share a fixed capacity of 96 task records and 12 projects. All and Completed use the same segmented selector throughout; All includes completed rows.

## Completed tasks

Settings > Delete completed tasks offers Today, Projects, Tasks or All sections, with counts, then asks for confirmation. Projects removes completed tasks within projects. It frees their task slots while retaining aggregate task, study time, session and planned-session counts, including project progress. The seven-day activity record is retained. Deleted names and individual hourly histories cannot be recovered through the app. Deleting a project separately also removes that project's retained progress.

## Appearance and statistics

The Focus home puts the navigation options first and Start focus at the bottom, without an illustration. Framed and Carousel use square corners; Carousel 2 and Bookshelf use rounded cards. The project list has compact cards with task counts, completed session counts and a progress bar. A project is completed when it contains tasks and all of them are marked done, including retained totals after completed tasks are deleted. Empty projects appear only under All. Project, Today and Tasks views share summaries, selectors, outlined Add controls and grouped task rows.

The timer groups its countdown, stage progress, accumulated study time and planned focus time. The chart button to the left of settings opens statistics for that focus only: all-time recorded study time, completed sessions, skipped focus stages, and today's 24 hourly study buckets. Breaks are excluded. Leaving the timer pauses it; opening its statistics keeps it running. Restart and Skip pause first and ask for confirmation. Cancel leaves the current stage paused. Confirmed Restart restores that stage's full duration; confirmed Skip moves to the next paused stage without earning a completed session. Both preserve already recorded study time.

On the timer, a short press of either navigation button starts or pauses the
current phase, matching Pomodoro. Confirm does the same when no other control
has button focus. Compatible Bluetooth page turners can use Turn page / move
forward, Previous page / move back or Select / activate. List navigation and
the Restart/Skip confirmations keep their own controls. Commands received
during a repaint stay in the existing bounded navigation queue; coalesced
short presses preserve their odd/even toggle result. Holds do not toggle the
timer repeatedly. No new input queue or timer workspace is allocated.

Statistics cover the last seven days, including today. Sessions and time sum that same period. Consistency is the percentage of those seven days with recorded focus time; partial sessions count as activity. The chart uses actual weekday initials, underlines today and scales its time axis to the recorded values. The message below it reports active days, with a separate invitation before the first session. Month/year filters and comparisons with previous weeks are unavailable because the existing save retains only seven daily records.

Settings shares its compact duration panel with Pomodoro, with outlined stepper controls and minute/session units. Delete completed tasks appears in its own section and opens the scope selector before confirmation. Done returns to the previous screen.

## Storage

`/.crosspoint/focus-a.bin` and `focus-b.bin` alternate verified saves. FCS4 uses 13,856 bytes per file, a generation counter and checksum. It stores task-list ownership, seven daily records (including Today-only time and sessions), aggregate counters, and each task's recorded seconds, fractional carry, skipped focuses and one dated set of 24 hourly buckets. FCS1–FCS3 saves migrate on the next changed successful write. Legacy tasks retain counts but explicitly lack earlier per-task time; the app never estimates it from session counts. Newer unsupported saves are not overwritten. Keep a microSD backup before installing older firmware, which cannot interpret FCS4.

Storage and rendering reuse activity-owned fixed buffers. State is 14,008 bytes on the host; the activity holds two states for rollback and one save buffer, bounded below 48 KiB by a compile-time check. It is allocated once with failure handling and freed when leaving Focus. These buffers are too large for the activity task stack; keeping them static would retain them while reading. No graph, timer tick or render path adds a heap allocation, task or network connection. Runtime heap headroom still needs device verification.

## Device checks

- Create different tasks in Today, Tasks and a project. Completing one must leave the others unchanged after leaving and reopening Focus.
- Start an unnamed Today focus, rename it, adjust sessions, then start and pause. Recorded study time should survive leaving and reopening Focus.
- Study across an hour boundary, open the focus's chart while running, and compare with a different focus. Only the consulted focus should contribute. Break time must not increase its study total.
- Try Restart and Skip, first canceling then confirming. The current/next stage must remain paused, study time must be retained, and a skipped focus must not earn a session.
- Complete tasks in all three sections and note statistics. Try each deletion scope; only its completed rows should disappear while aggregate time, session totals and project progress remain unchanged after restart. Cancel must leave rows intact.
- Check the selectors, settings, timer and confirmation text in English and Spanish, in both orientations and UI fonts. Watch serial output for OOM/save failures; compare free internal heap, PSRAM and largest blocks before opening Focus and after returning to the reader.

- In Focus Settings, adjust all four timer values with −/+, choose Done, then reopen Settings. The selected values should be retained. Check the completed-task deletion confirmation and Cancel before confirming a deletion.
