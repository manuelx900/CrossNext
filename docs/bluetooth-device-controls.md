# Bluetooth connection and device buttons

Bluetooth Settings distinguishes permission to connect from an active link.
The checkbox stores permission; the subtitle describes the current radio/link
state and identifies a connected device. Search results do not establish
compatibility: the implementation supports compatible Bluetooth LE HID input
reports, not arbitrary Bluetooth accessories or every BLE button device.

The compact help control explains compatibility, connected versus enabled,
connection lifetime, temporary pauses and button configuration. Cards retain
the current theme's corners: square in Framed/Carousel, rounded in Carousel 2
and Shelf. Main options and device names use regular-weight Ubuntu UI text. Only help
headings and the search/stop-search action use bold. Descriptions use the smaller
resident UI font. No downloaded font or image decoding is required.

While Bluetooth is allowed, the quick-settings sheet shows a centered status
in a black band with white text on one line, independently of its four shortcuts. The
user-selected compact arrangement adds space only in this enabled state;
disabled/unavailable Bluetooth keeps the original sheet height. The existing
resident Ubuntu 10-point UI font uses the sheet's otherwise unused clock slot
for the status and the light themes' compact captions;
the local font scope restores it afterward. No font or bitmap is added.
The status band opens Bluetooth Settings; returning closes the quick sheet.
A separate Bluetooth shortcut tile toggles the connection and shows status; its
selected state means connected. The band sits 14 pixels higher in portrait
Framed/Carousel, with all controls below moving by the same amount. Disabled
Bluetooth retains the previous compact geometry.
Enabling keeps the sheet open so startup remains visible. Disabling closes it:
the parent restores its page before the next, compact opening, without saving
a second framebuffer or adding a backdrop cache. "Disconnected · search stopped"
replaces the technical "radio resting" wording, with reconnection explained in
help. Short status labels retain pause/error causes; Settings provides details.
Other link-state changes
retain the same geometry and use the existing dirty-region repaint path.
Geometry changes rearm the existing reveal gate before rendering: moved controls
accept touch only after their paint completes. Ordinary connection-label updates
do not rearm it, preserving ongoing slider interaction.
The main loop snapshots this presentation under the render lock; rendering
consumes that byte without querying main-loop-owned radio policy state.

## Ownership and limits

The existing radio runtime owns BLE, its idle timer and connection events.
Activity transitions distinguish preserving a link from consuming its buttons:
Bluetooth Settings preserves the link but handles raw input itself; supported
menus and the quick sheet retain their existing logical-input arbitration.
Wi-Fi/USB reservations, internal-heap guards and exclusive reader/storage
transitions still take precedence. Returning Home preserves an existing link;
this does not promise uninterrupted BLE during network use or suspension.

Configuration stays in one activity: press/release a device button, choose an
action, save and configure another. Tap and hold (700 ms) are separate gestures;
capture waits 15 seconds after the last press. Disconnecting or changing device cancels it.
The activity streams the action/app registry into the existing list provider,
with no extra activity or full-list allocation. Failed settings writes restore
the in-memory table. Restoring a gesture reinstates its known profile default,
including wildcard report matching; unknown devices use generic HID behavior.

Limits remain four device tables, eight gesture bindings per device and two
distinct app destinations. Ordinary actions no longer consume an app destination.
The UI reports a full table or app limit instead of replacing another assignment.

Each configured gesture has an inset trash control. Deletion removes only that
entry, freeing a slot; the other tap/hold entry remains. Removal, restoring a
profile default and forgetting a paired device require confirmation, initially
focused on Cancel. These cold-path confirmations allocate the existing bounded
confirmation activity with `makeUniqueNoThrow`; they are not resident while
reading, and preserve BLE while taking ownership of input. Deletion rolls back
on SD-save failure. Device forgetting follows the existing radio/bond-store
failure contract. Cards use the same square/rounded theme corners without a
selection underline. Touch and row/edit targets do not overlap.

Bindings remain 32 bits and existing action IDs 0–13 retain their exact encoding.
Bits 26–27, formerly reserved, extend the action ID stored in bits 28–31. New
IDs 14–29 reuse existing host commands. Existing saved bindings remain readable;
older firmware rejects extended records, so new assignments need reconfiguration
after a downgrade. Config and RemoteTable remain 604 and 136 bytes respectively
on the 32-bit target. No radio task, framebuffer, cache or dependency is added.
Static RAM and activity sizes must be checked in the final native ELF; host
fixtures do not establish device peak heap or radio coexistence performance.

## Verification

`host-tests/bluetooth-clarity/run.py` exercises the production activity with
hardware/storage doubles and the shared raster renderer. It checks saved-record
compatibility, capture, tap/hold, rollback, profile restoration, connection
states, text bounds, help activation and both corner styles. The BLE policy,
runtime, configuration and source-contract suites remain separate; the
performance fixture checks quick-sheet dirty regions. These are not RF tests.

On X4 Pro, test a known compatible device and an unsupported accessory:

1. Enable Bluetooth, search and pair. Confirm permission and actual connection
   remain distinct; test disconnect, reconnect and disabled states.
2. Return Home, open settings and the quick sheet. An existing link should stay
   connected. The sheet's status must change when the device disconnects. Check
   the compact height with Bluetooth off and the centered black band when on.
3. Configure tap and hold without leaving the activity. Test advance in a book,
   menu and Pomodoro, then Back, lighting and a saved app. Reboot and reconnect
   to confirm persistence. Restore one gesture and check its default behavior.
4. Start Wi-Fi transfer, stop it, then reconnect Bluetooth. Test USB and sleep.
   Look for correct paused/off/connecting states and working input afterward.
5. Repeat transitions in all four orientations and the four themes. Verify
   square versus rounded cards, readable help, scrolling and touch targets.
6. Compare internal free heap, largest block and PSRAM before/after repeated
   transitions using serial diagnostics. A leak, shrinking largest block,
   startup refusal or reset is a failure even if static RAM remains unchanged.
   Check the render-task stack high-water mark while opening the menu and help;
   the shared UI control descriptors still require bounded render stack space.

## Repeated page presses and navigation cancellation

The activity manager owns a fixed ring of eight boolean page intents (ten bytes
before object alignment), not page data. Page presses accepted while the same
book is drawing are executed in order, one per completed page. Forward and
backward stay distinct. Capacity is bounded: a ninth outstanding press is
rejected and logged. Chapter skips and other shortcuts retain their existing
behavior. This changes software delivery, not BLE latency or E-Ink refresh time.

Local input, navigation, opening a reader menu, a reading overlay, an end-of-book
boundary or a lost link discards pending page intents. They cannot carry into
another activity. The input ring is main-loop-owned; the shared idle-work gate
is atomic. An atomic render-in-flight flag spans notification consumption through
render completion, including the interval before the render mutex is acquired. No page cache, framebuffer, radio task or radio policy is added or
reduced. The radio/EPUB PSRAM placement from the previous tested firmware stays.

Replace, push and pop invalidate cooperative idle work before waiting for the
render mutex. Workers test the atomic gate and owner pointer; a pending
transition cannot re-arm them. This removes waiting for the remainder of an
interruptible prefetch budget, but an individual decoder/SD call and ordinary
reader teardown still need to finish. Physical Home double-tap timing is
unchanged. The Bluetooth Reader Menu command opens the bottom sheet even when
the legacy menu preference was saved. A temporary atomic override survives child
Cancel callbacks that reopen the sheet. On resuming the reader without an overlay,
the override clears before local input, restoring the saved preference.

`host-tests/bluetooth-controls/run.py` executes production manager methods for
100 five-press bursts, alternating direction, busy frames, queue limits,
disconnects, overlays, boundary rejection and cancellation before all three
transition paths. The UI fixture covers confirmation Cancel/Confirm/save-fail,
button-slot reuse, hit geometry and themed cards. These are host checks, not
measurements of physical latency.

Language packages must be regenerated with `tools_local/language_packs.py bundle`.
English/Spanish remain built in; external `.cplang` packages include the new
confirmations and the previous Bluetooth/shortcut/web changes. New confirmation
copy was translated alongside the source. The hundred older missing entries in
36 external language sources were completed with automated translations and
format/placeholder validation; they have not had native-speaker review. The Ook
novelty catalogue retains its established English fallback for absent entries.
Existing fonts/keyboards remain in the language resource bundle.

Additional device checks:

1. While capturing, edit/delete a listed button immediately; Cancel keeps it,
   Confirm removes only that gesture. Reassign, reboot and verify persistence.
2. Cancel/confirm Forget; only confirmation should remove the bond and its
   configured buttons. Re-pair and check capture again.
3. With a settled book, press Next five times quickly: expect five completed page
   changes, then no further movement. Repeat backward and across a chapter.
   Press Home or touch a reader control during a queued burst: no later changes
   may execute in that screen. The E-Ink panel still refreshes sequentially.
4. Assign Reader Menu; expect the bottom sheet, including with the legacy saved
   preference. Cancel and use child screens, then return to the same sheet.
5. Open the upper sheet, tap its status banner and return. Check all four themes
   and orientations, the first tap after painting and the unchanged height when
   Bluetooth is off. No selection underline should appear in Bluetooth lists.
6. Return Home immediately after turning a page, and after waiting 400 ms so idle
   preparation can start. Compare time until the display starts changing. Keep
   the intentional Home single/double-tap distinction. Repeat with Wi-Fi transfer,
   sleeping, connected Bluetooth and a large EPUB to check memory stability.


## Help, Focus and translated controls

The compact How Bluetooth works button has centered text and no exclamation
icon. Its existing bounds and square/rounded theme rules are retained. Focus's
timer now accepts generic device navigation actions (Turn page / move forward,
Previous page / move back) and unfocused Confirm to start/pause, matching
Pomodoro. Reader-only page commands still apply only inside a book. Capture,
held-button repeat policy and list/modal navigation are unchanged.

The language refresh covers all 39 source catalogues (including English,
Spanish and the intentionally partial fictional Ook catalogue); 37 external
microSD packs are exported, with English/Spanish built in. Context corrections
include press versus printing, button assignments, Home navigation, stopped
search, Traditional Chinese script and European Portuguese wording. The audit
records precise corrections and validates placeholders/UTF-8. It is not a
native-speaker certification. Update only `/languages/*.cplang`; retain installed
fonts, keyboards and game data. Identical English/brand strings are intentionally
omitted from packs and use the existing fallback.

## Wi-Fi shortcut in quick settings

When Wi-Fi is connected to a network, quick settings shows a Wi-Fi icon between
the clock and battery percentage. Tap it to open the existing Wi-Fi management
screen. A disabled or disconnected connection has no header icon. Connection
changes use the panel's existing one-second status poll and repaint only the
affected header/tiles; opening this shortcut follows the same radio lifecycle
as the existing Wi-Fi tile.
