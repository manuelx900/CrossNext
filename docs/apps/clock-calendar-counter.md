# Clock, Calendar and Counter

All three tools use the shared header, with battery at the upper right, and the
oriented safe area. Large clock/counter digits use an 8,510-byte constant bitmap
subset of the bundled SIL-OFL Lexend Deca bold face. Normal UI retains the shared
fonts. Rendering allocates no new buffers and keeps the single framebuffer.

Clock switches between digital and analog faces. Rotate adapts both orientations;
leaving the app restores the prior orientation. The format chip switches between
24 hours and 12 hours with AM/PM. The existing checksummed 32-byte TLS1 store owns
count, analog mode and clock orientation. Reserved byte 18 stores the 12-hour
flag; prior files read as 24-hour format. Counter writes remain debounced.

Calendar shows localized month and weekday names, five or six week rows as needed,
and the selected day's full date. Tap a date to select it; Today resets selection
and month. Week start uses the existing preference. Rotate uses the shared tool
orientation preference. One grid hit region plus shared arithmetic geometry keeps
the 24-slot interaction table bounded; the table does not grow with day count.

Counter adds/subtracts one, offers one-step undo, and confirms reset. It stays
between zero and 999,999,999. Reset can be undone. Failed persistence remains
visible, and the existing writable/read-only rules are retained.

Verify on hardware before release: all orientations and bezel clearances, minute
and date rollover, 12-hour noon/midnight, sleep/wake, restored orientation after
exit, all calendar month starts and six-week months, increment/decrement bounds,
undo/reset, SD failure and reopen persistence. Hardware behavior and memory peaks
have not been measured by the browser or host tests.
