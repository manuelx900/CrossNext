# Interface layouts and reader fonts

Settings > Appearance offers Framed and Carousel. Framed keeps the book card and divided menu, with known progress at the cover's upper right. Carousel is the former Focus layout: the most recent book centered between exactly two neighboring covers, separated reading progress and compact menu icons. The old third layout migrates to Carousel. Swipe horizontally, tap a neighboring-cover band, or use the arrows to change the selected recent book. The center opens the selected book. Settings > Appearance > Recent books selects how many available books both Home layouts browse, from one to ten (default three), using the existing recently opened store. Navigation wraps in both directions. Reducing this display limit does not remove books from the history or SD card. Other recent-book views retain their existing behavior. It never adds synthetic books or changes their order. Settings remains enabled and follows the saved Home menu order, alongside shortcuts. Settings > System > Home Menu > Reorder Menu Items changes that order. See [Settings](settings.md) for the current category menu.

The layouts share the existing Home dashboard, input host, cover generation and one optional B/W art cache. Neighbor art is streamed one bitmap at a time. The existing fallible cache covers the carousel's art region rather than only its center cover (roughly 25 KiB at the largest supported portrait layout, orientation/region alignment dependent). Cache failure paints the art again; it does not block navigation. No animation loop, task or second continuous framebuffer is added. Physical peak heap/largest block and refresh quality still need device measurement. CrossInk's carousel and the SDK's three-slot component were evaluated; the three overlapping cover positions and streamed bitmap ownership remain in the local Home renderer.

Both Home layouts show day and month on the left, a centered local clock,
and battery on the right. An old Library appearance setting falls back to Framed.
The indexed Library and the Files browser remain available through their normal entries.

## Indexed Library

The upper-right view icon on Recent switches between the divided list and cover grid. Its `libraryView` preference is stored with device settings and survives reboot; changing tabs does not change it. Title and Author retain their grouped lists. Recent is capped at thirty entries in either view, using the existing recently-opened pins followed by arrival order. This limits the view, not the library or files on the card. Title and Author still expose the complete index.

The grid shows nine cards per page in three columns in portrait, and four larger cards in one row in landscape. Swipe left for the next page or right for the previous page; tap a card to open it and hold to see the existing Recent actions. EPUB/XTC thumbnails are generated lazily, one queued parser at a time, and cached on SD. Documents without covers show their file-type icon and title. Title/author caption colors follow selection. Only one bitmap/file handle is used while painting. Search, removal, deletion and rebuild retain their existing behavior.

## Fonts

From a book, swipe up from its bottom edge, select Font, then tap Font Family. The same bottom sheet changes to a paged font list; the real book stays above it. The family header arrow returns to Font controls; Back to reading closes the whole sheet. The chevron, tap above, or downward swipe closes the sheet. Selecting a different family saves it and repaginates at the current content position. Selecting the already active family does no SD write or reflow. The list opens on the page containing the current family. It uses the existing built-in and installed SD registry, with no second full-screen font activity in this flow.

Bitter and Lexend Deca, including regular, bold, italic and bold-italic faces, are provided in `assets_local/reader-fonts/Reader-Fonts-SD.zip`. Copy its `fonts` directory to the SD root and reboot or refresh installed fonts. They complement the two built-in Noto families and use the existing vector-font loader on the X4 Pro. The browser simulator includes the same pack. Their eight TTF files total 659,500 bytes on SD; they are not embedded in the application partition.

Both families are licensed under SIL Open Font License 1.1. The exact original license is included as `OFL` in each font folder and the ZIP; its extensionless filename keeps it out of the book index. File hashes are in `assets_local/reader-fonts/manifest.json`. The files were copied without modification from the supplied CrossInk reference; neither firmware builds nor runtime need that reference directory. Keep the included licenses when redistributing the pack. These are openly licensed fonts, rather than fonts without copyright.

## Touch menus

Games and their pushed child screens disable the left-edge swipe-to-go-back policy. Header arrows and physical/logical Back still work. Horizontal swipes remain available to gameplay across the screen. Other screens keep their existing policy; Home carousels also reserve horizontal swipes for changing books, locally; this does not change their pushed panels. ActivityManager applies the active activity's policy on entry, push/pop and each input frame, preventing it from leaking to the next app.

Games and Apps use the same nine-row page limit, previous/current-total/next controls, saved item selection and horizontal page swipes (left for next, right for previous, including the left edge). A right swipe on the first page performs Back, returning Home or closing the show/hide chooser first. The header arrow performs Back from any page. The start menus for 2048 and Othello show a board preview above their normal controls. Previews do not advance or modify a saved game.

## Device checks

On X4 Pro, change both Home layouts, hide/add shortcuts from one through eight items, rename Settings, reorder entries including Settings, restart and confirm the saved order. Check that Carousel centers the visible shortcuts and Framed closes the last row with a bottom divider. Exercise empty history, one book, ten books, missing files, long titles and all orientations. Compare carousel selection with the book opened, including neighboring-cover taps and both ends. Repeated menu selection must not retain more B/W snapshots; watch internal heap, largest block, PSRAM and stack high-water.

In Library, toggle views, restart, page through more than thirty indexed files, search, sort, hold/delete/remove and rebuild. Recent must stop at thirty and Title/Author must retain access to the rest. Test EPUB/XTC covers and TXT placeholders, missing SD, thumbnail generation failure and rotation while a cover is queued.

Open the font list from real EPUB/TXT content, choose all four families and both weights, page through a longer installed list, return with the arrow and close using every exit. Check content position, cache regeneration, glyphs/italics, missing-family fallback and sleep/wake. Finally steer 2048 starting at the physical left edge: the swipe must move the game and the header arrow must still leave it. Physical verification remains pending.

## Grouped Settings

Settings opens with eight icon rows: Display, Reading, Controls, Library,
Connectivity, System, Device and Appearance. Each has a short explanation. The portrait
layout shows all eight; shorter orientations scroll using the shared list.
Back from a section returns to this category list; Back there returns Home.
Existing subactivities, setting values and web API categories remain compatible.
Reading contains separate monolingual Define and bilingual Translate selectors.
See [Words](apps/words.md) for the explicit optional-save workflow.

Library lists reuse the Files browser row styling and type scale. Title and author share one compact wrapping label; Recent, Title and Author keep their existing sorting, grouping and open actions.

The reader Font family picker uses the same prose typeface as Contents, natural-case
family names and 56-pixel selectable rows (option 1). Its Normal and Bold Aa samples are touch choices that update
the same font-weight setting and the Weight row; the selected sample is outlined. The family list remains paginated in both orientations.

The root folders apps, fonts, dictionaries and licenses are reserved SD resources.
Files and Library omit them regardless of Show hidden files; identically named
subfolders inside a user's Books folder remain ordinary content. An older Library
index is rebuilt once with the current visibility policy, preserving arrival history
through normal reconciliation. Resource files and reading progress are unchanged.

Games and Apps use a larger footer page selector: 64-pixel arrow controls and the
body type size for the page numbers. Their list and footer reserve the same space
in portrait and landscape; other screens retain their existing pager dimensions.

In the landscape Font panel, Weight sits above the Normal/Bold samples. Family and
Size share the other column. The sample boxes no longer have a vertical divider.
All reader-sheet sections leave padding above their separator rules and use
12-pixel gaps between rows or footer controls, except the compact landscape Contents panel, which uses 8-pixel gaps. The portrait sheet uses 76% of the
safe height in Contents, Font, Layout and More; this preserves two-line chapter
titles and the Aa preview while adding space. The font-family rows remain 56 pixels.
Layout uses 96-pixel stepper rows. In landscape, Line spacing and Margins sit in
two columns above Alignment; portrait stacks them vertically. The chapter/menu
pager has 44-pixel arrow targets.

## Landscape on Xteink X4 Pro

Landscape uses the oriented safe area rather than shrinking a portrait page.
Framed centers the landscape book title and author below the cover. Statistics
use a 40-pixel ceiling for landscape reading-time digits and retain the larger
portrait scale; page counts omit their icon in landscape to keep the value
aligned with adjacent metrics. Pomodoro's stage strip has one outer frame and
single shared dividers, with a continuous filled selection. Wallpaper previews
fit and center the complete image in the current orientation without upscaling;
notices reserve separate space for their actions.
Home gives covers more height and uses smaller title/author cuts. Landscape Carousel omits the resume bar and arrow buttons; tapping the central cover opens the book, and swipes or the neighboring covers change books. Library and
Wallpapers use four larger thumbnails per page. Contents shows four chapters
per page with compact chrome and the same chapter selection and page controls.
Reading statistics reserve enough height for streak values, and Chess centers
its side panels against the board. Word deletion places its explanation and
actions beside the word so the buttons cannot cover the explanation.

File Transfer places hotspot QR codes side by side; the joined-network view
places its URL beside the QR code. Calibre separates setup instructions from
transfer status. Keyboard keys and field placement adapt to the shorter screen;
long entries show the lines around the cursor with arrows for hidden text.
The complete entry is retained. PET Care places all four needs in the left column with one-line labels and values; landscape minigame choices use text. Flashcards reserves padding below the due/new captions. Words groups its reveal/return actions and keeps deletion help clear of the buttons. Card guides reserve space for the ace rule above the pager. Pomodoro uses larger bitmap numerals with fixed digit cells and a constant ink height throughout a countdown. The horizontal stage strip is compact so the countdown has more room. A storage-error notice leaves a smaller but fixed countdown band so its text stays clear. Portrait keeps its existing composition except for the stable countdown and compact File Transfer menu.

On the X4 Pro, check both horizontal rotations and both Home layouts. In Library
and Wallpapers, visit every page and rotate on the last page; covers, selected
items and touch targets must agree. In a book, page through Contents and More,
then enter and edit a long search, Wi-Fi password and URL. Verify the cursor and
scroll cues without losing text. Check File Transfer in joined-network and
hotspot modes and Calibre while receiving a file; both QR codes, addresses and
status must remain visible. Check statistics with nonzero streaks, game boards,
app tabs, dialogs, missing-resource notices, Settings, and return to portrait.

## Home button interval

Settings → Controls → Home Button Gestures includes a Double-tap interval picker.
The default remains 350 ms; 450, 550, 650 and 750 ms allow more time for a second tap.
A longer interval also delays the single-tap action by the same amount. The
selected interval is stored with the device settings and applies to the global
Home gesture handler. A swipe or long press still cancels a pending single tap.
Check a single tap, two taps and a long press from Home and from a book after
changing the interval; the double action must not also open Home.

## Book completion

At the end of an EPUB or XTC/XTCH book, the reader shows an open-book illustration,
End of the book and a Home button. The back arrow returns to the last page while
keeping the book marked as finished. It does not choose another book.

More shows Finish book for an unfinished book, or Mark as not finished for a
finished one. Both actions ask for confirmation. Finishing saves 100% progress;
reopening keeps the current reading position and removes the finished status.
Existing reading time, pages, sessions and reading days are preserved. Neither
action invents reading activity. Reading the last page after reopening keeps the
unfinished choice; advancing to the end again, or choosing Finish book, finishes it.
The journal keeps a reversible status record; the original reading records are
retained. Finished and Recent lists use the latest book state. CrossPoint Sync
uploads the explicit state change while retaining cumulative reading totals;
progress synchronization continues to use the furthest reading position.

On the X4 Pro, finish a book, return with the arrow and open More. Mark it as not
finished, restart and confirm it appears in Recent rather than Finished with its
reading totals unchanged. Finish it again and confirm a single entry in Finished.
Repeat with EPUB and XTC/XTCH, both horizontal rotations, and English/Spanish.
A microSD write failure must show an error rather than claim the status was saved.

## Compact landscape controls

The reader bottom sheet keeps the same tab-strip height across Contents, Font,
Layout and More in landscape. Settings lists show the start of the next row when
more content is available below the visible rows.

Picross reserves a separate side panel for tools and fits clues around the board.
Go, Othello and Connect Four group feedback and actions beside their boards.
Battleship uses the available height for the target grid, keeping fleet status
and firing controls to its right. Carnivore fits multiple statements when their
complete text fits. Wordle centers its keyboard and legend together.

Habits shows six entries per page on the X4 Pro, with a compact date and progress
area. Flashcards uses larger centered deck titles without a second-line caption
and orientation-dependent pagination. Card and due counts appear inside the
opened deck. The counter keeps a fixed numeral scale
through four digits and fits larger values to the display without clipping.

On the device, check both rotations, page through all habits and decks, open the
last item on each page, and rotate while on a later page. Check Picross clue and
cell alignment, Go intersections, Battleship placement and firing, Wordle keys,
and the final Pomodoro stage page. The painted targets must match touch input.
