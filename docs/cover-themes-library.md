# Cover-focused Home themes and Library sorting

Settings → Appearance offers Framed, Carousel, Carousel 2 and Bookshelf. Carousel 2 uses a large central cover, neighboring books, reading progress and six shortcuts in two rows: Files, Library, Transfer, Statistics, Apps and Games. The top-center settings icon opens Settings. Physical navigation traverses these six shortcuts and Settings, with an unboxed Settings icon. Bookshelf displays a bookshelf with a current-book strip and a side menu; swipe horizontally to change shelf pages. It shows the most recently read books first, followed by newly added books without repeating them. Books without saved progress or a recent-reading entry display New; a previously opened book without a position cache displays 0%. It shows six books in portrait and three in landscape, on the number of pages chosen in Appearance (one to ten). Bookshelf displays Loading when rebuilding its index or generating missing thumbnails; returning with prepared resources does not show this popup. After a restart, a directory-only check compares eligible filenames and sizes with the saved index; additions, removals, renames or size changes trigger reconciliation. It does not parse book contents when the signature matches. Later Home visits reuse the index unless an app has marked it dirty. Library still provides access to every indexed book. Covers and their badges have small rounded corners; Framed and Carousel retain square controls. Carousel 2 and Bookshelf use light outlined buttons with subtle corner rounding. A larger current-book thumbnail crosses the full-width gray footer separator. The strip shows progress, locally recorded reading hours and minutes, and a progress track; it has no second full-width bottom separator. Reading time uses the existing cumulative snapshot once on Home entry, without scanning the journal or reloading on shelf-page changes. Without a local reading journal, the time is zero. If a journal exists but its disposable book snapshot is unavailable, a dash is shown until a later reading session recreates it. The side drawer uses larger shortcuts and a uniform stippled bookshelf behind it, with a close control, the configured shortcuts without a repeated date at the bottom. Opening the drawer keeps the clock and battery at the same position.

Carousel 2 and Bookshelf use a white header with black text and icons throughout the interface. Framed and Carousel retain their existing headers. The new themes also use regular, natural-case screen titles and separator-based Settings lists. Settings categories include a short description; section headings remain bold, while option names and values are regular; inside a category, compact regular labels use the smaller interface size, short current values align to the right and longer ones remain below the option. Existing checkbox behavior and setting actions are unchanged. Framed and Carousel also show category names and short descriptions, inside their rectangular menu. Screen titles and menu names use natural capitalization in every theme. Reading Statistics retains its existing card layout and typography. Apps and Games use regular labels, unboxed rows and gray separators in the new themes; Framed and Carousel keep their boxed menus. Appearance, Wi-Fi, interface-name settings and reader panels share the same light treatment. Option pickers use a stippled backdrop, thin separators and a black selected row with white text, with paging when options cannot fit; physical focus remains separate from the saved value. Numeric sliders use the same light panel and regular interface font. Empty states wrap their explanation rather than truncate it to a single line. The compact frontlight panel puts brightness and warmth before the shortcuts, with its battery aligned to the clock and separate space for the close handle. Reader panels use less height, show four chapters per portrait page and two in landscape, and use rounded outlined options. The per-book panel does not repeat a scope caption below its pager. Selected page tabs, including Reading Statistics, Reading Quest and PET, have a black background and white text. Settings categories use smaller titles and descriptions, while Apps and Games use the larger menu text. Todo lists and task rows use uniform names and separators. Physical focus uses a gray highlight or outline; selected game choices retain contrasting text over a black fill. Popup panels stay below the header so the light-header polarity cannot split a panel. These changes reuse existing actions and draw into the same framebuffer. Carousel 2's side covers bleed to the screen edges, with a stronger progress track and an unboxed Settings gear. Frontlight, orientation, interface-font choices and reading fonts remain independent. Existing saved Home choices are preserved; the retired experimental third layout migrates to Carousel unless the settings include the current layout-version marker.

The directory check uses one temporary checked allocation, including an 8 KiB duplicate-identity table shared in policy with the builder; it is released before rebuilding or preparing covers. Only the active Home layout owns display records and cover working storage. Bookshelf additionally retains up to sixty-one progress results in approximately 31 KiB of checked PSRAM for its sixty shelf slots and the featured book. It releases these results with the Home activity; returning from reading loads current progress. Existing layouts do not perform per-tile progress loads. Existing monochrome thumbnails and bounded cover caches are reused. Missing thumbnails may take longer on their first preparation. The new themes add no font files, background animation, network task or second display framebuffer. Header color uses the existing oriented output-polarity compensation, including asynchronous refresh and dark mode.

## Reader controls

Choosing a chapter offers Return over the destination page. It returns to the position before the chapter jump, including its text offset when available. It is temporary: turning a page, jumping elsewhere or using the return button removes it. EPUB and XTC/XTCH support this behavior in every Home theme; the return does not add a page-read event or change the book's completion state. Edge swipes keep their page-navigation behavior; only the visible Return control restores the source position.

Carousel 2 and Bookshelf use a rounded grab handle and joined rounded tab groups. Their app cards, action buttons, pagination controls and wallpaper thumbnails use the same rounded outline. Game-board cells, charts, separators and icon artwork retain their own geometry. The EPUB/TXT reader's More page shows five actions at a time; Book statistics and Finish book appear on its second page. XTC/XTCH retains its smaller, format-specific More menu. Font-family choices use a black selected row with white text. Framed and Carousel retain their rectangular controls and existing More pagination.

The frontlight panel reuses its stippled background between updates, ignores unchanged held-touch coordinates and initializes the clock from the first drawing. Slider release uses the fast refresh path. Bookshelf calculates its thumbnail height before the initial cover preparation, avoiding a second preparation caused only by discovering its layout during drawing. Missing resources and index changes still require preparation; device refresh and SD speed remain hardware-dependent.

## Files and Library lists

Carousel 2 and Bookshelf use unboxed rows with regular names, left icons and gray separators. Files displays the extension on the right instead of repeating it in the main name; Library shows only the title, without a file-size column. Long file names wrap to two lines and truncate with an ellipsis if needed. Files keeps the current microSD/ path at bottom left and the item count at bottom right. Library shows only the book count at bottom right. Both replace the page arrows. Swiping and physical navigation remain available. Framed and Carousel retain boxed Files rows and use the same path/count footer. Library reuses a bounded visible row window. Displaying these rows does not parse book contents.

## Library

The selector offers Recent, Collection and Filter. Collection stores virtual lists on the microSD;
adding, moving or removing a membership never moves or deletes a book. A new collection can start
empty or from a long-pressed folder in Files. Every editor row has a left-hand checkbox. A folder
checkbox selects or deselects its books and subfolders; tapping its name opens it for individual
changes. A dash indicates a partially selected folder. Selections survive visits to other folders
before Save. Folder checkbox states are inspected in bounded background steps; an unavailable
subtree does not block the editor or repeatedly open an error dialog. Folder imports
are snapshots, not synchronized folders. Cancel leaves the last saved membership unchanged.

Collections offer a card view and a compact list; their books offer lists and cached cover grids.
The collection view choice is saved immediately and restored after leaving Library or restarting,
independently of the Recent view choice. Collection names are centered above the keyboard input line.
Open a collection's three-dot menu to mark it as a favorite and keep it before other collections.
Only favorites display a star; the book count is plain text. Favorite books is the
editable built-in list. The former default Finished books collection is removed
from existing catalogs in memory without writing on open; the next ordinary catalog
edit persists that removal. Custom collections and reading completion records remain intact.
The book picker uses the same filename font and per-section font preference as Files.
Names can wrap onto two lines within their row, with the extension in a separate
right-hand column; folders use the same filename typography. Adding books uses
vertical swipe scrolling and a position indicator like Files, without a pager.
The reclaimed space and Files' text padding allow more rows. Returning from a
folder restores the previous selection and scroll position and presents its
known full/partial checkbox state in the first frame, rather than clearing it
and repainting the check later.

During an edit, a bounded PSRAM memo holds up to 64 exact folder paths and
selection summaries (about 66 KiB). Changes update known ancestor counts and
invalidate only affected unknown summaries. A return can reuse the visible file
checks without scanning before presenting its parent. Previously unknown
folder states are resolved in idle work; Back does not wait for a recursive
inspection. Known subtrees are skipped. An optional 8 KiB PSRAM prefix filter is populated while
loading or adding selected paths. It can prove that a folder has no selected
descendants without scanning it; collisions or unavailable memory use the exact
inspection path. Removing books leaves conservative extra matches until the
selection is empty. When no books are selected, showing unchecked folders
requires no recursive inspection. Other unknown folders are inspected in small
idle jobs on the existing render worker, cancelled on input and with batched
repaints. Initial uncached inspection and very large directories can still take
SD work; no device-latency result is claimed.

The folder browser enumerates each directory once and reuses its existing
128 KiB ordinal buffer. Two optional 32 KiB name snapshots plus their exact paths
(about 66 KiB) avoid rescanning ordinary directories on back and reading names
from SD on every repaint. Oversize directories use the existing disposable browse
file with buffered writes; failed optional snapshot allocation keeps that route.
Source-content revisions invalidate the directory and checkbox memos. These
workspaces are released when editing ends; saved membership and catalog formats
are unchanged. State-memo allocation failure retains saved data and reports the
existing collection error. The editor adds no worker task or framebuffer.
Long-press a book in Files, Library or a collection to change its favorite status or add it to a
collection. A collection's menu also provides rename, membership editing and collection removal.
Moving membership saves the destination first, so an interrupted move can leave the book in both
collections rather than lose its membership. Book files and reading statistics remain separate.

The catalog supports 128 collections, including the built-in favorites list, and 32,768 distinct book
paths per editable collection. The editor uses at most 32 folder levels. It keeps a bounded lookup
table in PSRAM (1 MiB plus 64 KiB for ordering); folder browsing adds 128 KiB of offsets. Allocation
failure shows an error and retains saved data. Leaving collections releases this workspace. No
background task or second framebuffer is introduced. The persisted paths must be updated through
the editor if books are renamed or moved outside the collection flow.

Catalog and membership files live under `/.crosspoint/collections/`. Each has two CRC-checked
generations; saving stages and closes a complete temporary file before publishing it over the older
generation, keeping the last valid copy. `edit.tmp` and
`browse.tmp` and `pending.tmp` are disposable working files. Back up this directory to preserve collections;
deleting the whole `/.crosspoint/` directory also removes them. A damaged pair is reported rather
than silently replaced with empty data.

All four themes use three tabs: **Recent**, **Collection** and **Filter**. Recent retains up to 30 opened books, in reading order. Its portrait list has nine rows with titles wrapping to two lines, without an author subtitle. Its header button switches between a book list and covers (nine covers per portrait page, four in landscape). New unread books appear in the indexed filters and on the Bookshelf Home shelf.

Filter first asks for Genre, Series, Author first name, Author last name or First letter of title. It then shows rows for values found in the microSD's metadata. Choosing one shows only matching books, with titles wrapping to two lines. Author choices use the same person's full name, ordered by first or last name. Series use their volume order. Missing metadata has a separate Unknown/No series/No genre choice; it is never invented from the filename. Back returns from books to values, then categories and Recent. These filters last for the current Library visit. The previous alternative Sort by layout is no longer offered.

Recent and collection cover pages share the full available grid area without a book-count footer
or a bottom Options button. Collection actions remain in the header menu. Existing smaller cached
miniatures can fill the enlarged grid without regeneration. Switching between Recent, Collection
and unused filters retains the parent index, its optional metadata snapshot and its cover buffer;
closing Library releases them. Selecting a filter still rebuilds the matching rows.

The header refresh button updates the index. The list/cover button changes the Recent view. Saved metadata and format preferences remain compatible; the filter categories do not include a Library settings entry. Files and Library use the existing index and bounded row windows; browsing does not parse every book again. Bookshelf's Appearance setting reads **Number of pages** and limits its bookshelf to one through ten pages. In other Home themes the same setting controls recent books. Only the visible shelf page prepares covers.

Shelf thumbnails fill their destination, cropping the center when aspect ratios differ; the smaller featured cover reuses its cached thumbnail with area sampling and monochrome reduction so the existing dither does not darken. A checked, reusable PSRAM row workspace of at most about 12 KiB is held only by the Bookshelf cover cache and released with Home. Header actions retain separate 44-pixel touch areas in the new themes, placed together to preserve title space.

Returning to Bookshelf Home reuses the retained library index and SD thumbnails. When its Home notice is enabled, the initial Loading covers notice appears while the book list is prepared; required index rebuilds or missing-thumbnail generation can need additional preparation. Adding books, changing thumbnail dimensions or removing cached resources can require preparation again. Carousel 2 and Bookshelf can retain up to three rendered cover regions in one optional, checked PSRAM arena of approximately 152 KiB, including validation metadata. A matching return validates at most seven source files and reuses the cover region; clock, battery and current reading information remain live. Missing PSRAM or invalid cache entries fall back to normal drawing. See [Home cover rendering](home-cover-rendering.md) for invalidation and device checks.

Choice popups use a thin black outer outline. The selected choice has a solid black
background, white text and no checkmark or separator beneath it. File-list paths abbreviate long
parent directories with ... while retaining the current folder at the left of
the footer, separately from its right-aligned item count.


With the Home notice enabled, each theme paints its existing book placeholders behind the initial
Loading covers notice, before loading the book list. The placeholders use the
normal cover positions without opening thumbnail files or
adding a display refresh. Once preparation finishes, the actual books replace
them; an empty library does not retain these temporary slots.

## Daily reading goal and sleep wallpaper

Reading Statistics → Today offers an optional daily reading goal (5–120 minutes). The card shows the configured target (for example, 30 min); today's recorded time remains in the reading-time card above it. The progress bar uses actual reading time, and the card retains Continue reading and its calendar button. Black means the goal was met. Today is gray until the goal is met, even before any reading. Past tracked days without a completed goal show a thick X; days before tracking began and future days stay unmarked. A visible legend explains the three states. Changing the goal reevaluates these days against the new target; it does not rewrite sessions, pages or reading time. PET has no daily reading target; its growth and rewards use recorded reading. The calendar uses the most recent 730 days for which dated reading history is available, including combined local and synchronized history. Older imported totals without dated records cannot fill calendar days.

Settings → Display → Sleep wallpaper → Reading goal shows today's minutes, progress and consecutive days meeting the goal. This static sleep screen updates when entering sleep. It uses the same recorded history and target as Statistics. Its disposable daily cache lives at `/.crosspoint/reading-history/goal-days.bin`; the first use rebuilds it, subsequent uses process only appended journal records. A changed snapshot, account, journal size/identity or calendar date invalidates it. The first use each day rebuilds dated totals so clock corrections cannot hide sessions. If journal contents were manually edited without changing their size or edge records, remove this cache to rebuild it. Storage errors show an error instead of a fabricated reading total.

Settings → Reading → Reading → Manga → Rotate XTC covers counterclockwise rotates only XTC/XTCH cover and thumbnail caches, with separate filenames so switching back preserves the original images. It does not rotate manga pages. The first use in either setting may generate missing images; later views reuse them.

Appearance uses equal-size illustrations for all four themes. Home accepts at most seven visible shortcuts. Existing eight-shortcut saves hide their last non-Settings entry while retaining custom labels and shortcut definitions. Bookshelf shows the configured date at the top left and the clock in the center; opening its drawer preserves those positions.

The daily goal is chosen from Today when no target is configured. Once set, Today displays the progress and Continue Reading; open its calendar button to change the target. The calendar keeps the target control at the bottom. Clock uses the same medium-size day/date face in both orientations; the time remains the main value.

If the device date is not yet available, the goal calendar shows the clock-setup hint and retains its target control; no dates or completion cells are invented.

Library lists show the book title on up to two lines in every theme, without a file-size column. Files and Library share their file-type icons. The arrow on the active Recent or Filter tab reverses the displayed order; filtering still opens categories, then their values, then matching books. Filter categories contain Genre, Series, author first name, author last name and title initial. Framed and Carousel draw one continuous filter frame with single separators. Files uses a larger folder path and a separate right-aligned item count in every theme; long parent paths abbreviate without overlapping the count. Theme illustrations use black artwork on white.

Framed and Carousel Home labels use natural capitalization, including book titles and authors; mixed-case metadata and custom names keep their spelling.

Filter metadata scans can reuse an optional PSRAM snapshot of the open index, up to 2 MiB. It is loaded in 32 KiB chunks the first time filters are opened and released when Library closes. Reopening or rebuilding the index discards it. Larger indexes or insufficient PSRAM retain the file-backed path. This avoids repeated small microSD reads without opening EPUBs during filtering.

Settings options retain their boxed groups, headings, values and checkboxes,
with the supplied reference icons added to the left. Reading > Reading > Manga
contains the existing XTC cover-rotation and direction options. Reader and global Text settings share horizontal page gestures and the
larger alignment selector. Text keyboards share rounded keys, separate numbers
and symbols, and a hold-to-choose accent surface. See [keyboard behavior](keyboard-input.md)
for language resources and device checks.


Appearance > Cover loading notice opens four independent checkboxes, one per Home theme. Bookshelf is enabled by default; Framed, Carousel and Carousel 2 are disabled. When enabled, Home can show its menu chrome and placeholders while preparing the book list. With a retained, current Bookshelf catalog, the first frame can already show the book grid. Framed places the notice at the top of its cover section; other themes use the usual centered notice. When disabled, Home reuses available artwork or prepares missing covers before presenting the grid. The Library loading notice is independent of this Home preference.

The Home module is Library in English and uses the corresponding Library translation in every available language. Its internal Recent tab remains Recent; user-defined Home labels are retained.
