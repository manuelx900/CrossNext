# Interruptible main-loop waits

On X4 Pro and Sticky, physical input edges wake a statically allocated HAL
semaphore. The main task waits for either that signal or the earliest declared
deadline. The ISR only signals; input decoding, touch I2C, rendering and storage
stay in their existing task contexts. Render completion also signals the main
loop. Pending signals are not cleared before waiting, so an edge arriving
between the input snapshot and the wait still causes another service pass.

This blocks a FreeRTOS task; it does not enable automatic light sleep, change
the panel waveform or add another task or framebuffer. The one static semaphore
and its handle live with the global HAL object in internal RAM, without an application
heap buffer. The shared SDK GPIO service, also used by display BUSY, can allocate
once: up to 392 bytes for its handler table, 8 for its interrupt handle and 16
for a new vector descriptor, plus allocator overhead. There is no per-pin or
per-loop allocation. Stack or caller-owned storage cannot replace those SDK
objects through its public API. The shared service remains installed for its
firmware lifetime. Diagnostic builds add one four-byte loop counter. The exact
static RAM delta and ISR placement must be checked in the native build map.

## Activity contract and compatibility

`Activity::nextLoopWakeDelayMs(now)` returns an optional relative delay.
An absent value retains the previous 10 ms polling, 50 ms idle polling and
three-second CPU-saving threshold. This is the default for games, network
activities and other screens that have not declared deadlines. Zero means
service work again immediately; it is different from an absent deadline.
`ActivityManager` queries mutable activity state under `RenderLock`.

Three screens opt in:

- Home checks its dashboard date at least once a second. Button/touch input
  wakes it sooner; held navigation remains on the fast input-service path.
- EPUB reading wakes for automatic turns, deferred manual turns, feedback
  expiry and the next-page prefetch admission time. Active builds and panels
  retain fast service. An incremental build parked at its look-ahead window
  can wait; it does not need to finish the entire chapter first.
- Pomodoro wakes for the next displayed second, phase completion, a debounced
  save after 350 ms, or periodic persistence after 30 seconds. It still uses
  elapsed time, pauses on exit/sleep and prevents automatic sleep while running.
  Queued navigation and an unfinished paint keep fast service. Its existing
  awake/full-speed policy while running is retained.

Opted-in screens use the 300 ms idle CPU-saving threshold. Rendering and
continuous work keep their existing power locks/`skipLoopDelay()` behavior.
All waits have a one-second safety bound and include the auto-sleep deadline.
Mapped input also contributes the pending capacitive Home single-tap deadline.
When double tap is enabled, the single action is due strictly after its existing
350 ms window; a second held contact retains fast input service instead. Deferred
Home actions from blocking transfers request an immediate service pass. An emitted
Home action counts as user activity, restoring normal CPU speed before dispatch
even when its physical release occurred before the 300 ms idle threshold.
Debounce is serviced within 6 ms (subject to the RTOS tick); held physical,
touch, multi-touch and capacitive Home contacts within 10 ms. The SDK provides
this input-state deadline without performing I2C or assuming an IRQ polarity.
Other controllers and unsupported boards retain polling. Failed semaphore
initialization or a checked per-pin installation failure also falls back to
polling. Rollback removes only this HAL's handlers, preserving the shared display
service. The GPIO service uses Arduino's existing interrupt flags and the board
profiles; GT911 is not reconfigured.

Connected serial/USB and enabled tilt shorten the wait. Developer Mode and the
serial command bridge retain 10 ms service, and USB-exclusive storage keeps its
existing loop. The simulator has no device input semaphore and retains a 10 ms
SDL service bound. These modes must not be used to claim battery-idle wake rates.

## Verification and limits

The focused suite is `host-tests/idle-wait/run.sh`. It checks deadline ordering,
overdue work, 32-bit clock wrap and the real SDK input deadline implementation,
including capacitive Home and multiple contacts. Existing Pomodoro persistence
and phase tests remain applicable.

Session artifacts are in `qa-artifacts/idle-deadlines-2026-10-25/`. The actual
firmware activity integration harness compares four one-minute phases using
10 ms service against the real Pomodoro deadline and timer core. Both reach
the same phase, remaining time, completed count and running state, including
clock wrap. The controlled schedule needs 24,000 versus 240 service passes;
this is a deterministic host scenario, not a measurement of the device loop,
current or battery life. It also checks dirty-save deadlines, queued input,
reader timers and legacy policy for Chess, Othello, Underhand and Toy Battle.

With diagnostic logging, `LOOP Passes: elapsed=...ms count=...` reports real loop
passes over the memory-log interval. USB/serial bridge polling and rendering
affect it. Normal release images omit this counter. No numerical battery or
input-latency improvement has been established on hardware.

## Device checks

1. Compare the supplied normal before/after images on the same device, SD card,
   brightness, book and radio settings. Keep progress/state backups. To compare
   battery-idle behavior, disconnect USB and disable Developer Mode and tilt.
   A USB-powered measurement exercises the shorter USB service bound instead.
2. Open an EPUB with completed pages; wait idle, then try brief side-button
   presses, repeated presses, held navigation, capacitive Home tap/hold,
   touchscreen taps, drag, swipes and multi-touch. Repeat in all orientations
   and with remapped buttons. Each gesture must act once, with no short action
   after a long action. Repeat while a chapter is still building or a menu is up.
   With the default Home tap action and double-tap frontlight action, a single
   capacitive Home tap must not wait for the one-second idle heartbeat. Its
   action follows the existing double-tap window; double tap and tap-then-hold
   must still produce only their configured action. Disabling double tap keeps
   the single action immediate. Measure input dispatch separately from reader
   cleanup, SD writes, Home loading and physical display refresh.
3. Test first-page rendering, the successor prefetch after 400 ms, automatic
   turns and feedback dismissal. Compare input-to-completed-screen video, not
   just parser times. Watch for missed input, an extra turn or a persistent
   high-rate loop after feedback expires.
4. In Pomodoro, compare against an independent clock through focus, short break
   and long break, with automatic starts both enabled and disabled. Check pause,
   resume, stage selection, a settings change followed by immediate sleep,
   Back/return and restart. Existing progress/save semantics must remain.
5. Test Chess thinking, Othello's placement preview, Toy Battle, a timed game,
   Pet animations and network play. Their undeclared deadline uses the previous
   cadence and CPU policy. Check held buttons and taps during a paint too.
6. Check USB plug/unplug, charging completion, serial commands, Developer Mode,
   tilt turning, auto-sleep and wake. Keep the USB Drive handoff test separate.
   The diagnostic serial bridge deliberately keeps 10 ms polling.
7. Repeat open/close and sleep/wake cycles. Compare internal heap, PSRAM, largest
   blocks and task high-water marks. No new heap buffer or task should appear.
   Current, IRQ reliability, touch behavior and input latency require hardware
   confirmation; successful builds and host tests do not establish them.
