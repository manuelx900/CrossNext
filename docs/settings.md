# Settings and home appearance

Open Settings from Home. Eight illustrated categories keep settings on the device:
Display, Reading, Controls, Library, Connectivity, System, Device and Appearance.
Titles use the bold 12pt UI font; values and help use the regular 10pt UI font.
A value appears beneath its title. A checkbox shows an on/off setting; an arrow
opens or cycles an option. Long labels wrap, and swipes or physical navigation
reach additional rows in either orientation. Group headings use text without an underline;
list borders and separators between options remain visible.

Reading separates text, reading behaviour, dictionaries and the status bar.
Library groups files, finished books and cache maintenance. Controls separates
buttons, touch, haptics where fitted, and keyboard layouts. Connectivity contains
Wi-Fi, reading sync and OPDS. System contains Home, language, sleep and the clock
when the device has one. Device contains firmware, extensions, information and
developer mode. Hardware-specific options remain conditional on the device.
The settings registry, JSON keys and existing actions are unchanged. Firmware
identifies itself as CrossNext 1.0.0 in About and on the boot screen; native
S3 and simulator builds take their version from the fork section in platformio.ini.

Appearance offers two home layouts: **Framed** and **Carousel**. Carousel is the
layout previously called Focus; its cover and book details sit above a separate
progress row. The older third layout is retired. Saved value 1 keeps Carousel;
legacy value 2 migrates to Carousel; an unsupported value falls back to Framed.
Framed displays known reading progress in a small box at the cover's upper right.
Unknown progress has no percentage. The box is painted after the cached cover,
so moving selection does not erase it.

The existing activity owns the reusable row/value vectors and navigation state.
Reference icons are constant one-bit masks (15,431 bytes); no per-frame
image decoding, additional framebuffer or background task is introduced. Checked
boxes and vertically centred trailing controls are opt-in SDK list properties;
other lists retain their previous rendering.

## Verification

Host checks cover all eight main destinations, both Appearance choices, touch
regions, actual font metrics, both orientations, and separated Carousel title,
author and progress geometry. Browser verification uses the same firmware source
with an isolated SD image. Verify on the device before publishing: open every
category, scroll to its final row, activate an enum and checkbox, return from a
child activity, change language, and use physical navigation where available.
Rotate and change Home layouts with a long book title; the author must stay above
the progress bar. In Framed, selection changes must retain the percentage.
No device build, flashing or hardware verification is implied by browser checks.

## Quick settings shortcuts

Display > Quick settings assigns four shortcuts from Night mode, Refresh, Rotate, Light, Warm, Wi-Fi, Lock screen, Sleep, Power off and Restart. Existing shortcut identifiers are preserved; the duplicate-assignment check now covers all ten choices. Lock screen keeps the existing sleep behavior and uses an outlined padlock. Sleep, Power off and Restart persist changed light preferences and use the shared power request flow, which flushes reader state and prepares storage before sleeping or restarting. They add no separate power owner or background task.

The leading light and warmth icons also toggle those controls directly. Light off displays 0% brightness; turning it on restores the preceding brightness. Warm off displays 0% warmth; turning it on restores the remembered positive warmth. The icons and checked state follow the active setting.

Bluetooth is omitted: the bundled SDK has an optional BLE HID host, but `FREEINK_CAP_BLE_HID_HOST` defaults to zero and these firmware configurations do not enable or integrate its pairing/input flow. A hardware radio alone does not provide a usable Bluetooth shortcut.

## Wi-Fi and clock

Open Connectivity → Wi-Fi to scan the available networks. Select a protected network, tap its password field to enter the password, and choose Connect. Show password controls visibility on the review screen; leaving that screen hides it again. Saved credentials retain their existing connection behavior.

While connected, select the current network to see its signal, protection, IP and MAC address. If an existing connection is absent from the scan, protection stays unknown (shown as “--” and a question mark) until it is observed; the UI does not assume an open network. Disconnect ends the current connection and keeps its saved password. Forget network asks for confirmation, removes that network's saved password and ends its current connection. Returning to Settings or closing the control center preserves the live Wi-Fi on/off state. An enabled radio continues using power and driver memory until switched off or an existing shutdown boundary applies. Sleep, restart and other network apps keep their existing connection lifecycle; enabling Wi-Fi does not add automatic connection on wake.

System → Clock → Timezone includes Punta Arenas (UTC−03:00 without seasonal daylight saving). It is the default when a zone has never been chosen and there is no older offset to migrate. Existing explicit zone choices and older nonzero offsets remain intact; select Punta Arenas there to replace a prior choice. These paths have simulator/host checks; real network association and device radio teardown still require hardware verification.

The timezone picker lists Punta Arenas directly after Santiago. Display order is mapped separately from the append-only stored zone IDs, preserving existing selections.

System → Clock → Manual date and time adjusts year (2020–2099), month, day, hour and minute without Wi-Fi or a required timezone-selection step. Confirm writes the hardware RTC in UTC using the currently configured zone, then updates system time; Back discards edits. Invalid dates and daylight-saving gaps are rejected. Manual time counts as a configured clock for reading statistics. RTC persistence and power-cycle behavior still require device checks.

The 12/24-hour setting applies to Home and shared pixel headers. Show in header controls the optional clock in those headers; Home keeps its dedicated clock. Reading → Manage fonts offers Installed fonts offline and Download fonts, which explicitly requires Wi-Fi. Reading sync and OPDS remain active features. English and Spanish UI languages select their corresponding initial keyboard even when an older saved mask omitted it. Wordle's English keyboard omits Ñ; Spanish includes it.
