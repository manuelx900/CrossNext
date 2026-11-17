# PET artwork pipeline

The firmware composes PET screens from separate four-level grayscale assets. Silva, Nox
and Ember identify the three species internally; the visible companion name is
entered when a life begins. Illustrations contain no interface labels.

## Source assets

- `assets_local/pet/png/`: 102 original one-bit PNGs for eggs, life stages,
  poses and the original object set.
- `assets_local/pet/hogar/`: the room and 240 furnishings, with coordinates,
  anchors and masks used by the habitat builder.
- `assets_local/pet/animation/`: breathing and egg-motion atlases.
- `assets_local/pet/farewell/` and `collection/`: farewell and trophy artwork.

Source metadata records provenance and image hashes. Current masters live in `REDISEÑOS/PET/assets/v2/` and its referenced sibling
directories. Set `CROSSNEXT_PET_MASTERS` to that v2 directory when building from a
worktree. Its provenance file is required; the legacy sources alone cannot
reproduce the current artwork.
Keep the original notices and asset provenance when changing artwork.

## Build and installation

Run the commands in [the asset README](../../assets_local/pet/README.md) from
the repository root. The six outputs are `graphics.bin`, `home.bin`,
`habitat.bin`, `breathing.bin`, `farewell.bin` and `collection.bin` under
`resources/apps/pet-v2/`. Install all six under `/apps/pet-v2/` on the microSD.
Missing or incompatible contents show the app installation notice; personal
save data is stored separately under `/.crosspoint/pet-v2/`.

Use smooth black contours, clean gray fills and enough margin to preserve ears,
tails and supports. Keep characters and furnishings separate. Current exports
use CNART003 with two-bit coverage, not the old one-bit CNART002 artwork. Default
builders fail if current masters are missing, preventing a silent legacy export.
`--legacy` explicitly selects the old compatibility pipeline for fixtures only.
Stable sprite and object IDs must not be reordered when replacing an image.

See [the filename list](pet-artwork-list.csv) for the original image names and
[the PET guide](pet.md) for current screen and save behavior.
