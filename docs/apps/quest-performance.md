# Aventura Lectora: rendering and input budget

The hero's name and rank occupy a plain band above the Home illustration. The
illustration keeps its preceding width and height and moves down into the space
previously used by the experience strip. Scale, crop and artwork are unchanged.

Select the Level tile to open the experience window. It shows progress within
the current level, remaining experience and a progress bar. At level 150 it
shows total experience and the maximum-level message. The window overlays the
same hero menu and blocks its controls until dismissed. Done, the header back
arrow or physical Back closes it. It uses existing activity ownership, work
buffers, translated labels and scene caches, without a new image buffer or a
profile write. Check both orientations, long names and RTL languages on the
physical display.

The activity retains at most 512 KiB of optional sprite pixels in PSRAM: 96 KiB of base art, 64 KiB of story art, 160 KiB each for guardians and rewards, and 32 KiB for hunting outfits. These allocations reuse SdBitmapPack's activity-owned arenas and are released on exit. The existing read-ahead/small-read fallback remains available on allocation failure and boards without PSRAM. No second framebuffer is allocated.

SdBitmapPack evicts the least recently used sprite, compacts its bounded byte arena, and retains other sprites. Cached pixels are still checksum-verified. Existing callers retain their requested capacities; the per-pack upper bound is 160 KiB.

Seen-item, favorite and Saga bookmark changes are coalesced for 1.5 seconds and saved by the existing render worker when idle. Pending changes are also flushed on app exit and before sleep. A critical action includes pending preferences in its immediate verified primary journal save. The internal alternate slot retains recovery protection; no external backup copy is created. An abrupt power loss during the coalescing interval can lose those cosmetic changes; purchases, rewards and battle progress retain immediate persistence. The primary save path and format are unchanged.

Touch presses and releases are captured before attempting RenderLock. Coordinates carry the generation shown when their contact began; a contact from an older screen is discarded. The main loop owns the coordinates and the render worker publishes the displayed generation atomically. The shared interaction reveal gate remains in place. This allows a tap during idle SD work or a focus-only repaint to be handled afterwards without routing it against a different screen.

Moving the physical-button focus in a confirmation rebuilds its interaction table but clips drawing to the old/new button area and uses the existing panel-window API. Screen transitions keep normal rendering. Driver-required full refresh fallbacks still apply, including inverted output. The three combat frames and manual continuation are unchanged.

The hunt board computes its four available IDs once. A shop page obtains its count and up to four visible items in a single catalog pass. Filled rarity stars draw bounded scanline spans, avoiding native fillPolygon allocations per triangle.

## Checks

Run host-tests/quest-performance/run.py with a host C++ compiler, or supply --compiler and --node for Emscripten. It checks input generations, sprite eviction/pixel equality/failure fallbacks, catalog queries, existing hunt rewards and backup recovery. The study-suite UI checks cover opacity, return flows, purchase confirmations, victory records, page bounds and attack frames.

On X4 Pro, compare repeated visits to Home, Shop, an item, Campaign and Hunts with the same SD card. PERF logs separate queued, panel and composition time for slow repaints; include action/save time when measuring touch-to-visible latency. Check internal/PSRAM free bytes and largest blocks before entry and after repeated exits. Verify all four orientations and inverted output; focus updates must not erase neighboring content or worsen ghosting. Tap during background saves and during transitions; it must act once on the intended screen. After changing a favorite, leave/sleep/reopen and confirm persistence. Also verify purchases and victories survive reopening and backup recovery.

The full reading-history scan at app entry and full composition on actual screen transitions remain. These changes do not claim a measured millisecond reduction on physical hardware; retain the device logs when comparing builds.
