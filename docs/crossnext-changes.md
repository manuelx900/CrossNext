# CrossNext: changes and upstream credits

[Resumen en español](../README.es.md)

CrossNext is based on CrossPlay, which is based on CrossPoint Reader. This is a
feature summary of the local fork, not a claim to have created the inherited
reader, games, FreeInk SDK or scheduling algorithms.

| Project | Inherited contribution |
| --- | --- |
| [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) | Reader engine, document handling, settings, storage and device firmware foundation. |
| [CrossPlay](https://github.com/ma-r-s/crossplay) | The local app/game shelf, its game implementations, study tools and FreeInk-based interface. |
| [FreeInk SDK](https://freeink.org/) | Underlying display, input and UI facilities. |
| [CrossInk](https://github.com/uxjulia/CrossInk) | Carousel/interface reference and unmodified optional Bitter/Lexend Deca font files, retaining their OFL notices. |
| [CrossDiTo](https://github.com/dito94/CrossDiTo) | Reference and assistance with firmware optimization. |
| [VCodex](https://github.com/franssjz/cpr-vcodex) | Reading-statistics/vocabulary workflow reference; its separate engine/cache was not imported. |

The integration base is CrossPlay **1.14.2**, with CrossPoint **1.6.5**. This is
a source derivative even when published as a new GitHub repository with a clean
initial history. Keep the inherited copyright and permission notices.

XTC/XTCH Library metadata support adapts [CrossPoint PR #4023](https://github.com/crosspoint-reader/crosspoint-reader/pull/4023) by [Eszter Schuffert](https://github.com/eszter007): XTCH files are indexed, and both formats contribute embedded titles and authors.

The local CrossNext source adds or adapts these flows:

- PET reading companion: care, reading-based stages, four minigames, 240 home
  objects and 50 progressively discovered memories. Home placement is reversible;
  inheritance preserves furnishings without granting new progress memories.
- Required microSD content packs for PET and The Intercept, using the same
  installation notice as Reading Quest and keeping saved games separate.
- Reader startup and next-page preparation, cached EPUB events, batched SD page
  and section-index writes, idle deadlines, glyph byte masks and Home-return
  input fixes. These are code-path changes; device timings and battery effects
  still require physical measurements.
- Shared black menu/search headers, clearer Library cover selection, and physical
  navigation tailored to files or pages rather than cycling unrelated controls.
- Library navigation separated from cold cover generation. Repeated presses
  update selection while cover work waits for idle time; existing covers remain.
- Horizontal pagination in the requested local apps, puzzle selectors and reading
  statistics. Recent books use five entries in portrait and three in landscape,
  with multi-digit minute values above their unit.
- Flashcards and Words typography settings: independent sizes from 10 to 18 pt,
  Smart sizing on by default, complete long-card pagination, and bold vocabulary
  section labels. Page-layout caches avoid repeating full text measurement.
- Anki images support zoom, two-dimensional dragging and rotation together, with
  Back returning to the retained card. New imports prepare separate high-resolution
  zoom data; ordinary cards keep their original preview path.
- Audited all 28 active app/game entry renderers; removed redundant work in
  Carnivore, Words, Reading Quest and Othello.
- Pomodoro's physical button starts and pauses the current phase; timeline
  arrows use separators within the shared strip.
- XTC/XTCH navigation can invert both swipes and page-tap zones; the reader's
  refresh-frequency control follows the text/menu layout.
- CrossNext-named installation images, source/resource archives, and a populated
  local simulator for screenshots, using synthetic statistics and saved progress.

Details live in [Flashcards](apps/flashcards-ui.md), [Words](apps/words.md),
[Pomodoro](pomodoro.md), [Library performance](library-performance.md),
[render audit](render-performance.md), [interface layouts](interface-layouts.md), and [the showcase guide](simulator-showcase.md).
These describe the current source tree; the clean export does not preserve the
upstream Git history. Hardware performance and power must be measured on each
supported device before publishing a release. Keep all original licence notices.


### Home and photo controls

- Home's physical navigation cycles only its menu options, retaining rapid presses
  during panel refresh; carousel/framed focus changes repaint only affected rows.
- Flashcard Photo uses a compact control beside the pager, freeing text space.
- Legacy imports can recover optional HD image data on Zoom from their unchanged
  original Anki package, preserving scheduling and review history.
- X4 Pro release files use `CrossNext-X4PRO-Firmware-v1.0.0`.

### Controls, wallpaper sets and faster section changes

- Updated monochrome logo shared by boot, fallback sleep and repository artwork.
- Full-screen Home gesture assignment with independent direct app/game targets.
- Brightness and warmth controls show their actual enabled state and retain the
  preceding nonzero setting for re-enabling; warmth is a configurable quick action.
- Left rotation by default, with a left/right preference and two-position tools.
- Zoom retains dragging and adds four arrows moving by thirds, below the image
  in portrait and beside it in landscape. Fit/normal view keeps more image space.
- Wallpaper preparation paints first, converts images incrementally with counts,
  and reuses thumbnail caches on later entries. A bounded multi-selection spans
  pages and confirms the random sleep set, including an All action.
- Statistics reuses recent/finished projections from its first history scan and
  reads journal blocks in both directions. Home avoids an unnecessary cover-grid
  repaint and stops its installed-plugin probe after its first positive match.
- Physical navigation is retained through busy rendering in the shelves,
  Statistics, Quotes, Habits, Notes/Tasks, Pomodoro and Reading Quest. Several help
  and puzzle page handlers avoid repainting when a clamped step changes nothing.

See [the design and X4 Pro test plan](interface-update.md) and
[the release notes](releases/CrossNext-v1.0.0.md). Presentation images supplied
by the maintainer live in `docs/images/` and are used by both README versions.

### Home and quick controls

- Returning Home reuses the reading percentage when the saved data is unchanged,
  without retaining a reader or parser in memory. Framed progress uses a compact
  outlined rectangle. Direct app/game shortcuts display their own icon and name.
- Light tiles and slider icons share the same toggle action. Warmth can enable
  a light that was off; both toggles retain their preceding intensity.
- Wallpaper All toggles only the selection; Done asks before saving. Invalid
  images are excluded, valid image dimensions are preserved, and the ordinary
  gallery uses the space occupied by the selection instruction while choosing.
- Carnivore keeps its English title and appears as Carnívoro in Spanish, retaining compatible saved-game IDs.
