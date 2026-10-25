# PET artwork pipeline

The firmware composes PET screens from separate monochrome assets. Silva, Nox
and Ember identify the three species internally; the visible companion name is
entered when a life begins. Illustrations contain no interface labels.

## Source assets

- `assets_local/pet/png/`: 102 original one-bit PNGs for eggs, life stages,
  poses and the original object set.
- `assets_local/pet/hogar/`: the room and 240 furnishings, with coordinates,
  anchors and masks used by the habitat builder.
- `assets_local/pet/animation/`: breathing and egg-motion atlases.
- `assets_local/pet/farewell/` and `collection/`: farewell and trophy artwork.

Source metadata records provenance and image hashes. Original upload archives
and local generation notes are not needed to build the distributed packs.
Keep the original notices and asset provenance when changing artwork.

## Build and installation

Run the commands in [the asset README](../../assets_local/pet/README.md) from
the repository root. The six outputs are `graphics.bin`, `home.bin`,
`habitat.bin`, `breathing.bin`, `farewell.bin` and `collection.bin` under
`resources/apps/pet/`. Install all six under `/apps/pet/` on the microSD.
Missing or incompatible contents show the app installation notice; personal
save data is stored separately under `/.crosspoint/pet/`.

Use pure black and white, a consistent pixel-art outline and enough margin
to preserve ears, tails and object supports. Keep characters and furnishings
separate so placement, removal and life-stage changes remain visible. The
builders prepare one-bit output, bounds and masks without changing source PNGs.
Stable sprite and object IDs must not be reordered when replacing an image.

See [the filename list](pet-artwork-list.csv) for the original image names and
[the PET guide](pet.md) for current screen and save behavior.
