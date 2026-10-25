# Navigation menus

The active reader reserves the bottom-edge upward swipe for a text panel over the current EPUB/TXT page, or a page panel over the current XTC/XTCH image. A horizontal top rule separates book content from the Contents/Font/Layout/More controls and Back to reading button; there is no replacement preview screen. Contents selects chapters in place; More groups search, bookmarks/notes, location, statistics and sync. Center taps no longer open reader menus. See [reader-text-settings.md](reader-text-settings.md) for controls, format limits and verification.

Settings, Browse Files, File Transfer, Games and Apps use the same pixel style
as Home: a black header, framed rows, left-aligned icons and inverted selection.
Settings shows its four rectangular category tabs and battery status. Its header
does not display the firmware version or `DEV-SIMULATOR`; About retains version
information.

Library (Recent Books on Home) uses the same header and framed list, with
Recent, Title and Author sort tabs. The header keeps battery status above
Search and Rebuild. Rows show file-type icons and titles; the bottom counter
tracks the selection. Titles preserve their case in a subtly heavier Noto Sans face,
with the existing fallback chain for characters outside the bundled cut. Metadata,
search, author grouping and file paths retain their original text. Only the
visible row window is formatted.

Browse Files keeps the current path at the bottom. File and folder names keep
their case; folders use the same Noto Sans size in bold. Filesystem paths and
rename/delete operations retain their original bytes. Long labels wrap to two
lines or end in visible dots. The registered standard UI fallback keeps
non-Latin characters readable when a suitable built-in or SD font is available.

File Transfer keeps the same Wi-Fi, Calibre, hotspot and board-supported USB
choices, including their descriptions. Touch opens a row; the existing physical
button and Back behavior remains with the shared UI hosts.

Games and Apps page left for next and right for previous, including from the left edge. A right swipe on the first page returns Home, or closes the show/hide selector first; the header arrow performs Back from any page. Touch page controls and side-button paging remain available. The
header counter and numbered controls identify the current page. The highlighted
row follows the remembered item; the side buttons still change pages. The gear
opens the show/hide selector, where row checkboxes change visibility and Done
or Back returns to browsing. Empty folders keep a large target for reopening
that selector. The black PLAYER shortcut below Games is removed.

Games, Apps and their screens show a left Back arrow in touch headers. It
feeds the existing logical Back input: in a visibility selector it closes the
selector, and inside an app it follows that app's existing navigation. Headers
with their own leading navigation control, such as Wikipedia article history,
keep that control. Physical Back and remapping continue to use the same route.

The layouts use the oriented viewable area. The rendering helpers live in
`src/apps_local/ui/PixelMenu*`; Settings and the two system lists continue to use
UiTabListActivity/UiListActivity and their published interaction tables. Their
fixed row-formatting workspace is reused for the activity's lifetime, with no
per-file display array or new allocation on repaint. Temporary font bindings
are restored after each build so the global theme keeps its normal fonts.

Local checks:

```sh
./host-tests/pixelmenu/run.sh
./host-tests/ui/run.sh
./host-tests/home/run.sh
./host-tests/backgesture/run.sh
```

Use [Start-Simulator.cmd](../Start-Simulator.cmd) on this Windows checkout, or
the usual `scripts_local/sim-shot.sh` on a supported native simulator host.
The browser simulator validates layout and navigation, rather than physical
e-ink timing or SD persistence.

Before using a hardware image, check all four orientations: open every menu,
change Settings tabs and a reversible toggle, scroll to the last setting and
back, open a nested folder with a long or accented name, and cancel a file
action. In Games, visit every page, hide/show a game, return with Done and Back,
then open a game from a later page. Confirm the same game reopens and the lower
PLAYER bar never appears. In Apps, open Habits and return. Verify these with
remapped logical buttons where available and inspect internal heap/largest
blocks across repeated entry/exit. No hardware verification is implied by a
browser or host-test pass.

In Library, check all sort tabs, both sort directions, grouped titles/authors,
search and clearing search, rebuild completion, a long title and an accented
name, book actions and scrolling to the last entry. Tap Back from a search and
then from Library. In Games and Apps, test the header arrow from the folder,
visibility selector, app menu and an active app view; it should behave like the
physical Back button at each level. Watch for overlapping titles, unwanted app
launches or a Back target surviving into a screen without that arrow.

Home customization and reading statistics are described in [reading-stats.md](reading-stats.md). Library list rows use a larger regular face with up to two title lines, preserving the title's case. The author is no longer appended to each title; author sorting and group headings remain available. Recent uses one frame; Title and Author keep each heading outside its own group frame. Book lists have a two-pixel outer rectangle and full-width row divisions, with the selected row filled black. Games and Apps retain their own heavier frames. App/game headers leave a clear gap below the black band; no separate horizontal rule is drawn there.


Browse Files uses the same shared book row metrics as Library: a 32-pixel icon and 10pt Noto Sans. File titles have a small optical stroke increase, generated into the one-bit font, while retaining the original advances and line metrics. The dedicated book-list family reuses the same bold folder face and standard UI fallback; Murdle and other apps retain their regular face. Every row reserves two lines plus eight pixels of vertical padding, giving identical 64-pixel heights for short and wrapped names. Twelve-pixel side padding and icon gaps leave more width for the title. Filenames keep their case; the small file-type label omits the leading dot. No metadata author row is added. The firmware picker uses these same shared rows, including original-case names, bold folders, small extensions and uniform height. Home Menu uses the larger body face for titles and the smaller tile face for descriptions. Screen transitions and row actions clear inherited tap feedback, so only the current navigation selection is filled black. Settings remains checked but disabled, with a smaller explanatory subtitle.

The reader's Contents sheet uses the regular document-selector face for chapter titles, preserving their case. Roman chapter numbers keep their existing button face. Titles keep a consistent size, with at most two lines, rather than shrinking to fit. Portrait pages contain four chapters and landscape pages contain two; the pager and selection mapping use that same capacity for EPUB and fixed-layout readers. Long titles receive the SDK's visible ellipsis after the available lines.

Continue Reading has previous/next buttons. Previous moves toward newer recents, Next toward older ones; unavailable arrows are disabled. Browsing does not reorder history; opening the card opens its displayed book. A fixed ten-entry index map refers to the existing RecentBooksStore; Home copies metadata only for the displayed book and generates/caches only that cover. Classic Home still exposes one book selection plus its menu rows. Cover-grid themes retain their existing layout.

The top-edge panel delegates to fork-owned QuickSettingsScreen: white title, four icon buttons, straight brightness/warmth tracks and collapse chevron. Active Dark Mode and Light tiles are filled. Display > Quick settings customizes the four slots. Refresh closes the sheet and cleans the underlying screen. Rotate toggles between normal portrait and landscape based on the live screen orientation, rebuilding the overlay for the new orientation. Sliders support dragging and one-percent +/- steps; tapping the sun toggles the light. Existing HAL/persistence/touch-restore behavior stays shared. The reader status bar shows whole-book percentage, enabled by default under Reader > Customise Status Bar > Book Progress Percentage; this differs from panel battery percentage.

New games and save/device contracts: [Pocket games](pocket-games.md). Free-play puzzle behavior and CPZ2 migration/reset: [Chess puzzles](chess-puzzles.md).
