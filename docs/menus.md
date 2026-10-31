# Navigation menus

The active reader reserves the bottom-edge upward swipe for a text panel over the current EPUB/TXT page, or a page panel over the current XTC/XTCH image. A horizontal top rule separates book content from the Contents/Font/Layout/More controls and Back to reading button; there is no replacement preview screen. Contents selects chapters in place; More groups search, bookmarks/notes, location, statistics and sync. Center taps no longer open reader menus. See [reader-text-settings.md](reader-text-settings.md) for controls, format limits and verification.

Settings, Browse Files, File Transfer, Games and Apps use the same interface style
as Home: a black header, framed rows, left-aligned icons and inverted selection.
Settings shows its four rectangular category tabs and battery status. Its header
does not display the firmware version or `DEV-SIMULATOR`; About retains version
information. Appearance keeps Recent books and Interface font at the bottom,
with the same border weight as the theme selector.

Settings > Appearance > Interface font offers Noto Sans (the default),
Ubuntu and Noto Serif. Ubuntu uses regular interface labels and bold
headings. It covers Home, files, Library, File Transfer, statistics, games,
apps, Settings and the reader's control panels. Selecting a section opens the same option popup as other settings, without
leaving the section list. A confirmed choice is applied and saved on the microSD;
Back or an outside tap dismisses the popup without changing it. Missing or invalid General preferences use Noto Sans; missing or
invalid section overrides follow General. If saving fails, the
previous choice stays active and an error asks you to check the card. The book's
reading font remains a separate setting. Dedicated counters and the calculator
use bundled Noto Sans cuts; illustrations are unchanged. Saved Pixel/Jersey 25
choices migrate to Noto Sans, while saved Ubuntu and Noto Serif choices remain.

General sets the default. Home, Files, Library, File Transfer, Reading Stats,
Games, Apps, Settings, Reader controls and Control panel can each follow General
or choose one of the three families. For example, select General > Ubuntu and Games >
Noto Sans. Child dialogs inherit their section; closing them restores the previous
section's font. Selecting Use general removes that section's override. Both the
general choice and overrides persist on the microSD.

En español: Ajustes > Apariencia > Letra de la interfaz permite elegir Noto Sans
(la predeterminada), Ubuntu o Noto Serif.
Cada sección abre un selector emergente. Al elegir se guarda el cambio; volver
o tocar fuera lo cierra sin cambiar nada. La letra del libro se ajusta
por separado en el lector.
General define la letra predeterminada. Cada sección puede usarla o elegir
una de las tres familias. Por ejemplo: General > Ubuntu y Juegos > Noto Sans. «Usar la general»
vuelve a aplicar la elección principal a esa sección.

Library (Recent Books on Home) uses the same header and framed list, with
Recent and Filter tabs. Recent retains up to 30 opened books and supports list or covers. Filter opens metadata categories and value rows before showing only matching books. Titles wrap to two lines, without metadata subtitles. See [Library](cover-themes-library.md#library) for category and Back behavior. Search and rebuild retain their existing actions. Only the visible row window is formatted.

Browse Files keeps the current path at the bottom. File and folder names keep
their case; folders use the same regular Noto Sans size. Filesystem paths and
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

Home customization and reading statistics are described in [reading-stats.md](reading-stats.md). Library list rows preserve title case and allow two lines. Framed and Carousel use rectangular frames and row divisions; Carousel 2 and Bookshelf use separator-based lists. Games and Apps follow their section theme.


Browse Files uses the same shared book row metrics as Library: a 32-pixel icon and 10pt Noto Sans. File titles have a small optical stroke increase, generated into the one-bit font, while retaining the original advances and line metrics. The dedicated book-list family keeps its standard UI fallback; Murdle and other apps retain their regular face. Every row reserves two lines plus eight pixels of vertical padding, giving identical 64-pixel heights for short and wrapped names. Twelve-pixel side padding and icon gaps leave more width for the title. Filenames keep their case; the small file-type label omits the leading dot. No metadata author row is added. The firmware picker uses these same shared rows, including original-case names, regular-weight folders, small extensions and uniform height. Home Menu uses the larger body face for titles and the smaller tile face for descriptions. Screen transitions and row actions clear inherited tap feedback, so only the current navigation selection is filled black. Settings remains checked but disabled, with a smaller explanatory subtitle.

The reader's Contents sheet uses the regular document-selector face for chapter titles, preserving their case. Roman chapter numbers keep their existing button face. Titles keep a consistent size, with at most two lines, rather than shrinking to fit. Portrait pages contain four chapters and landscape pages contain two; the pager and selection mapping use that same capacity for EPUB and fixed-layout readers. Long titles receive the SDK's visible ellipsis after the available lines.

Continue Reading has previous/next buttons. Previous moves toward newer recents, Next toward older ones; unavailable arrows are disabled. Browsing does not reorder history; opening the card opens its displayed book. A fixed ten-entry index map refers to the existing RecentBooksStore; Home copies metadata only for the displayed book and generates/caches only that cover. Classic Home still exposes one book selection plus its menu rows. Cover-grid themes retain their existing layout.

The top-edge panel delegates to fork-owned QuickSettingsScreen: white title, four icon buttons, straight brightness/warmth tracks and collapse chevron. Active Dark Mode and Light tiles are filled. Display > Quick settings customizes the four slots. Refresh closes the sheet and cleans the underlying screen. Rotate toggles between normal portrait and landscape based on the live screen orientation, rebuilding the overlay for the new orientation. Sliders support dragging and one-percent +/- steps; tapping the sun toggles the light. Existing HAL/persistence/touch-restore behavior stays shared. The reader status bar shows whole-book percentage, enabled by default under Reader > Customise Status Bar > Book Progress Percentage; this differs from panel battery percentage.

New games and save/device contracts: [Pocket games](pocket-games.md). Free-play puzzle behavior and CPZ2 migration/reset: [Chess puzzles](chess-puzzles.md).


Interface font settings separate the Games and Apps menus from their contents.
Open either section to choose its menu font or an individual game/app font.
“Use menu font” follows that menu's section setting, which can itself follow
General. A specific choice applies to that item's child screens; opening another
section restores that section's font. Settings use stable item keys, so changing
menu order or hiding an item does not move its font choice. New fields are
optional: existing settings keep their section inheritance.

Settings categories show a single regular-weight title. The list outline ends
below the last category, without enclosing unused space. Settings details and
File Transfer titles use the selected family's real bold face; descriptions stay
regular. Home options in Framed/Carousel and the Games/Apps
folder lists use regular weight. Home book titles and authors also use regular
weight. Noto Sans 14 and
Noto Serif 11/14 have matching one-bit bold cuts,
generated with `tools_local/toybox/gen_ui_bold.py` from the bundled OFL sources.
Book text fonts remain independent. Appearance's bottom actions use smaller
icons and labels while preserving the theme border and touch target.

Library author groups and alphabet headings use bold. Book rows keep regular
weight in both filtered and recent lists; collapsed author groups retain bold.

Shared selectors, page controls, Home frames, settings panels and app/game panels
use two-pixel outlines, matching the SD Card list. Standalone buttons use the same
outline; adjoining list rows use one-pixel edges to avoid doubling their shared
divider. Adjacent task rows share a single two-pixel divider. Selection remains
visible through fill, check marks and explicit interaction states. Game artwork
and board selection marks retain their contrast. Flashcard deck and folder names use one consistent size and show an
ellipsis when a name exceeds one line.

Finished and Recent book rows keep identical outlines. Their current navigation
selection is indicated by the right-hand arrow only on non-touch devices,
without a line beside the book icon. Touch devices have no persistent cursor.

In every theme, Browse Files shows the `microSD/` path at bottom left and its count at bottom right. Library shows only the book count at bottom right, replacing the page arrows. Swipes and physical paging remain available. Long filenames
wrap to two lines. Library keeps Recent and Filter above the list.
Choice popup titles are bold and separated from choices by space rather than
an option-row rule. Settings focus uses the same gray rule as other rows.
Finished/Recent arrows stay unhighlighted on touch devices. Non-touch devices
retain their selection marker. Book detail dates share a line with their label.
Pomodoro uses a shorter stage selector with a continuous selected fill and
gives the recovered space to the timer card in Carousel 2 and Bookshelf.
