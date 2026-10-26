# Interface layouts and reader fonts

Settings > Appearance offers Framed and Carousel. Framed keeps the book card and divided menu, with known progress at the cover's upper right. Carousel is the former Focus layout: the most recent book centered between exactly two neighboring covers, separated reading progress and compact menu icons. The old third layout migrates to Carousel. Swipe horizontally, tap a neighboring-cover band, or use the arrows to change the selected recent book. The center opens the selected book. Carousel browses only the three most recent available books from the existing recently opened store and wraps in both directions. Other recent-book views retain their existing behavior. It never adds synthetic books or changes their order. Settings remains enabled and follows the saved Home menu order, alongside shortcuts. Settings > System > Home Menu > Reorder Menu Items changes that order. See [Settings](settings.md) for the current category menu.

The layouts share the existing Home dashboard, input host, cover generation and one optional B/W art cache. Neighbor art is streamed one bitmap at a time. The existing fallible cache covers the carousel's art region rather than only its center cover (roughly 25 KiB at the largest supported portrait layout, orientation/region alignment dependent). Cache failure paints the art again; it does not block navigation. No animation loop, task or second continuous framebuffer is added. Physical peak heap/largest block and refresh quality still need device measurement. CrossInk's carousel and the SDK's three-slot component were evaluated; the three overlapping cover positions and streamed bitmap ownership remain in the local Home renderer.

Both Home layouts show day and month on the left, a centered local clock,
and battery on the right. An old Library appearance setting falls back to Framed.
The indexed Library and the Files browser remain available through their normal entries.

## Indexed Library

The upper-right view icon on Recent switches between the divided list and cover grid. Its `libraryView` preference is stored with device settings and survives reboot; changing tabs does not change it. Title and Author retain their grouped lists. Recent is capped at thirty entries in either view, using the existing recently-opened pins followed by arrival order. This limits the view, not the library or files on the card. Title and Author still expose the complete index.

The grid shows nine cards per page in three columns. Swipe left for the next page or right for the previous page; tap a card to open it and hold to see the existing Recent actions. EPUB/XTC thumbnails are generated lazily, one queued parser at a time, and cached on SD. Documents without covers show their file-type icon and title. Title/author caption colors follow selection. Only one bitmap/file handle is used while painting. Search, removal, deletion and rebuild retain their existing behavior.

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
12-pixel gaps between rows or footer controls. The portrait sheet uses 76% of the
safe height in Contents, Font, Layout and More; this preserves two-line chapter
titles and the Aa preview while adding space. The font-family rows remain 56 pixels.
Layout uses 96-pixel stepper rows. In landscape, Line spacing and Margins sit in
two columns above Alignment; portrait stacks them vertically. The chapter/menu
pager has 44-pixel arrow targets.
