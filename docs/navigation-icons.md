# Navigation icon family

The user-supplied reference marks live as local SVGs in
`assets_local/navigation-icons/`. `REFERENCE.json` records the ten screenshots,
their hashes and crop regions. Focus uses focus brackets and a central point;
Counter uses five tally marks. They have distinct silhouettes from Pomodoro and
Calculator. The references determine the arrangement; row and touch geometry
are unchanged.

Regenerate the committed assets with the project's Python environment:

```sh
python tools_local/navigation_art/generate.py
python tools_local/statistics_art/generate.py
./bin/clang-format-fix -g
```

Pillow and resvg are host-side generation tools. The firmware consumes packed
one-bit masks from flash, with the existing SDK icon and bitmap interfaces.
There is no SD lookup, SVG parser, allocation, cache or background task for
these icons. Only referenced variants need to survive the linker's data GC.

`NavigationIcons.h` supplies upright 24, 32 and 64 pixel masks. Settings options
use a 64 pixel canvas with a centered 44 pixel ink extent, matching the other
option-row icons inside their existing 52 pixel image slot. The generator
preserves aspect ratio, rounds the final mask once and shares the resources
between normal and inverted selection states.

The legacy `src/components/icons/` headers alias generated masks rotated
counter-clockwise for `GfxRenderer::drawIcon`. FreeInkUI consumes upright masks
and handles logical orientation. Keeping both encodings generated from the same
SVG prevents folders or radios from appearing sideways in a different theme.
`UiAppHelpers.h` routes file and directory lists, `HomeScreen.cpp` the Home
appearances, and `Shelf.cpp` the Apps/Games rows. Calibre's signal-bars mark is
chosen locally by NetworkModeSelection, independently of the Library mark.
StatisticsArt retains its public names and routes them to this shared family.

The stock Lucide `listIcons.h` and `ToyboxIcons.h` remain separate: their
generators must not overwrite these supplied reference marks. Their existing
callers outside the navigation surfaces retain their own contextual art.

## Verification on the device

Open Files, Library collection lists and the SD firmware picker. Directories
should share the rounded folder and TXT/Markdown the folded page with lines.
Check selected white-on-black rows as well as normal black-on-white rows.

Check Home in each appearance, both Games pages and both Apps pages. Focus,
Counter, Pomodoro and Calculator should be distinguishable. Check Wi-Fi,
Calibre and Hotspot in Send Files. In Statistics check Today, Data and goal
prompts. In Reading settings verify all option icons share the same visual
scale, center and text gutter as the other settings rows.

Repeat in all four orientations and with the supported interface scales.
Misrotation, cropped marks, a dark square behind a selected icon, or a changed
touch target are failures. Host previews are not physical-panel verification.
