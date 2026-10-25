# Pomodoro

Open **Apps > Pomodoro**. Defaults are 25 minutes of focus, a 5-minute short break and a 15-minute long break after every four planned focus stages. Start/Pause controls the current timer. Skip advances without crediting a focus session. Restart asks for confirmation, restarts the current phase and keeps the completed-session count.

Settings pauses the timer. Focus is adjustable from 1–180 minutes, short breaks from 1–60, long breaks from 1–120 and the long-break interval from 1–12 planned focus stages. Changing the current phase's duration restarts it in pause. Auto Start Breaks and Auto Start Focus are independent and off by default. Completion advances to the next phase and shows a notice; press Start unless its automatic-start setting is enabled.

The timer runs while the app is open and prevents automatic sleep while running. Leaving or manually sleeping pauses and saves. It is not a background alarm while reading or sleeping, and does not play a sound. Time in a temporarily opened control-center panel is counted on return; at most one completed phase advances, avoiding credit for unseen focus sessions.

The automatic-sleep hold applies while Pomodoro is the foreground screen. If the control-center panel remains open, that panel's normal idle timeout applies; sleeping then pauses and saves the underlying timer through the activity stack's existing sleep hook.

Timing uses unsigned monotonic elapsed milliseconds rather than subtracting one second per repaint. The screen refreshes when the displayed second changes; one in sixty paints requests a cleanup. Physical refresh rate, power cost and readability remain unmeasured.

The activity owns fixed state, UI interactions and save workspace. No worker, network or second framebuffer is added. Host ABI sizes are 24 bytes of core state and 104 bytes of repository state, excluding activity/UI. Checked nothrow activity allocation follows stack ownership; no per-tick container allocation is added.

Two verified 64-byte POM2 records, `/.crosspoint/pomodoro-a.bin` and `pomodoro-b.bin`, store settings, phase, cycle-stage index, remaining time and completed sessions. POM1 records migrate on load; both versions are readable. Loading always pauses. Running saves are throttled to 30 seconds and flush on completion, pause, skip, exit or sleep; unchanged records do not write. The newest valid generation wins; a torn write falls back to the older slot. Unknown future versions or wholly damaged files remain untouched and show Save Unavailable. Removing only those two files while powered off restores defaults.

Device checks: compare a one-minute focus/break cycle to a separate clock; test long-break intervals and both automatic switches. Pause during ordinary and cleanup paints. Exit/re-enter and sleep/wake should preserve the remaining time in pause. Missing SD should show Save Unavailable without crashing. Verify portrait/landscape controls, then monitor heap/largest block and render-stack high-water mark across repeated open/close cycles. Hardware verification is pending.

The separate stage strip above Focus shows the programmed cycle: numbered focus periods alternating with short breaks, ending in a long break. Three stages are visible at once; arrows expose the rest (up to 24 stages). The active stage is black. Tap a different stage to start its full programmed duration in pause without earning completed-focus credit. Tapping the active stage does nothing. Skip follows this same planned order; elapsed focus completion earns credit. Choosing a different stage changes where the next automatic transition goes. The stage and remaining time survive exit/restart. Changing the cycle length clamps the stage into the new cycle and resets it if its phase changes.

A short physical navigation-button press toggles Start/Pause on the timer
screen. A second press pauses the running phase; it does not select another
control. Stage-strip arrows share the strip's outer frame and use vertical
separators rather than individual small boxes. Timer settings remain touch controls.
