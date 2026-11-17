# Returning to Home

Returning from Games or Apps preserves the existing cover resources when only
the folder position or visibility list was saved. The two exact reserved files,
`/.crosspoint/shelf.cfg` and `/.crosspoint/shelf-hidden.cfg`, are classified as
navigation state by `StorageMutation`. They still advance the general content
revision. Books, thumbnails, settings and unknown paths remain resource writes;
remounting the card still invalidates every resource. No cache size or lifetime
is extended.

Settings saves each change at its existing edit boundary. Leaving an unchanged
Settings screen no longer serializes and writes the complete settings file.
One activity-owned flag retains failed saves, entry-time quick-resume
normalization and the existing fallback after entering child editors. A
successful save in Settings clears the flag. Browsing categories without editing
avoids both the redundant SD write and its cover-cache invalidation; delegated
editors retain their exit save even if they already saved themselves.

File Transfer retains existing font caches while its mode picker is open.
Committing to a network mode releases them before starting the connection;
server startup retains its second release after network selection, since drawing
SSID text can repopulate glyphs. Cancelling the picker does not scan plugin
subscriptions. Every exit ends mDNS, including failed HTTP startup that clears
the server pointer. Completed server sessions still close sockets before radio
teardown and refresh subscriptions.

The changes add no heap buffer, task, retained image or framebuffer. Settings
adds one boolean inside the existing activity. Icon artwork remains unchanged.
Go's turn banner uses the existing ink-centered fitting overload; its controls,
game rules and saved data are unchanged.

## Verification

The production HAL fixture tests the exact navigation paths, near misses,
resource writes, interleaved handles and rename boundaries. The menu-entry
fixture executes Settings entry, save and exit bodies with unchanged settings,
normalization, success and failed-save retry. The radio-services fixture executes
the picker/exit bodies and the commit prefix against recording services; actual
network drivers lie beyond that fixture's commit boundary.

Build `gh_release_x4pro` after review and inspect the native static-RAM report.
On the X4 Pro, keep the same book, orientation and font; repeatedly return from
Games, Apps, Settings and the uncommitted Transfer picker. The first tap after
each return must work. Repeat in landscape. Change a setting and reboot to
check persistence. Change a cover or transfer a book and confirm Home updates.
Then repeat Bluetooth → Wi-Fi transfer → Bluetooth, OPDS and Sync.

Check the Go banner with black and white turns, the review label, portrait and
landscape, and the selected interface font. Visible letters should have balanced
space above and below the black band. Simulator results do not measure e-ink
latency or establish physical radio behavior.


## Reader entry and exit follow-up (16 November 2026)

Compared with tag 1.3.1 (`fae3f195`), reader entry now restores per-book text
preferences and may select streamed SD fonts. Those operations preserve the
chosen layout and typography; this change does not bypass them or invalidate
layout, image, glyph or paragraph caches. First-time indexing and display BUSY
time remain necessary. A source comparison alone cannot establish a device
latency regression against 1.3.1.

Completion-history lookup and its partial-MD5 calculation run after the first
page has been displayed, rather than before the first render request. Opening
reader controls first forces the lookup before constructing their completion
actions; automatic end-of-book recording still follows it. The first-paint
host fixture asserts this ordering, not a hardware speed multiplier.

A reader-lifetime 33-byte ID and two HAL revision stamps avoid repeated hash
reads while resources and media remain unchanged. Failed reads, zero revisions,
resource changes and remounts recalculate. Nothing survives reader destruction.
New indexing/cache writes can deliberately invalidate this reuse.

Reader exit opts into exact serialized-state comparison using a 128-byte stack
chunk. Byte-identical state avoids a redundant SD write/flush; changed, missing,
truncated or unreadable state is written normally, and failed writes are retried
by subsequent saves. The entry crash guard, progress and session durability
remain synchronous. Other stores keep unconditional-write semantics.

Bluetooth's Reading menu command calls `openTextSettings()`, the bottom sheet
with Contents, Font, Layout and More. It no longer opens the compact progress
toolbar or needs a temporary override of the saved menu preference.

Measure on hardware with the same EPUB, resource pack, theme, SD card and warm
caches: open the book, leave immediately and after an idle interval, then reopen.
Also test a fresh chapter, end-of-book, changed font and restored sleep session.
The cooperative idle cancellation and deliberate Home double-tap interval from
the previous candidate are unchanged. No instantaneous-exit claim is made.
