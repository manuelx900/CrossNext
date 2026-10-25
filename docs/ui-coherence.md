# Settings and app UI consistency

Settings sublists use the same compact pixel-label cut, 52 px base rows,
28 px toggles/icons and measured viewport as Display. Long labels may wrap,
so row height can grow when required for readable text. About, reading text,
clock, language, keyboard and option popups follow this scale. The top-level
category header and reader preview retain their own hierarchy.

The Focus home carousel adds upper spacing and moves the cover/title group
down. Words, Flashcards, Tasks and Habits follow the supplied reference layouts;
landscape uses columns where vertical stacking would hide controls. These
screens use existing fixed UI/prose fonts and one framebuffer. Task pagination
retains its existing cold-layout vectors, reserving a bound before adding the
two section headings. No new background task or persistent format is introduced.

Check on device in both orientations: compact settings and About; long task
lists before/after a checkbox edit; all Habits presets and calendar navigation;
Flashcards question/answer ratings and Undo; the Focus title above its menu.
Try Murdle with a fresh card, a valid save, a protected newer-version save and
a temporarily unavailable card. Load failures must preserve files; a retry
must recover access when the card becomes available. Monitor MRDL logs for a
rejected version/size or write error. Keep a backup of user data for fault tests.

Host UI checks: `host-tests/coherence/run.sh`. The existing UI, Habits, Notes,
Murdle and Study suites cover their shared geometry, storage cores and scheduler.

## Interface review for X4 Pro

Shared controls keep separate semantic sizes: compact settings, normal menu
rows, content descriptions and large readouts. Games keep their own boards.
Hearts and Solitaire menus now bind the existing 14px control face explicitly,
use a 20px content heading, and use 62px action rows. Solitaire presents its existing games/wins/streak
record as three aligned readouts instead of a long caption. Both menus use
the existing bounded numeral renderer, so large counts remain inside their cells. Their board font bindings
and card geometry are unchanged. No font asset, buffer, task or save format is
added; the existing fixed font-slot table is rebound only for these menus.

Check both languages with a saved and a fresh game: full menu headings, Rules,
New game, opponent strength and draw mode; complete a game and return through
the menu. The general audit also exercises root categories, home/library, reader
sheets, every registered game entry and small-tool screens with the available
host renderers. A host pass does not verify physical panel refresh or touch.
