# Illustrated request keepsakes

`jobs.json` maps the 300 authored request IDs to 133 object illustrations and
defines the single currency coin. Object names, claim identities and narrative
text remain in `campaign/requests.json`; this file only selects their artwork.
Different needles are separated into sewing, compass and clock-hand subjects.
The leaf seal has its own wax-seal illustration.

All masters were created with the built-in `image_gen` tool, with true transparent
backgrounds. Each unchanged PNG in `masters/` has adjacent JSON recording its exact
prompt, original source path, dimensions and SHA-256. The final currency uses
`coin-v2.png`. Four object corrections also use `-v2.png`; the first drafts are
retained as unused visual iterations.

Run from the repository root:

```
python tools_local/quest/build_keepsake_icons.py
```

The packer checks original hashes, trims transparent gutters, contains the object
inside an 88-pixel square and centers it in a 96-pixel monochrome bitmap. Its white
matte becomes transparent when the renderer paints the icon. It writes
`src/apps_local/quest/QuestKeepsakeBitmaps.h`, inspection PNGs in `previews/` and
`packed.json`. The 134 compiled bitmaps occupy 154,368 static bytes before pointer
tables; there is no new heap allocation, image cache, framebuffer or SD resource.
Rendered sizes are controlled by the existing reward and collection cards.

Missing originals or mismatching provenance stop a delivery build. For a temporary
layout study only, `--preview --output <separate-cache-header>` permits missing
objects and marks them in the adjacent JSON. Such partial output must not be
copied into a device or delivered simulator. Generation and packing do not modify
the hero, PET, ornaments, or existing artwork packs.

Twelve regional medals have distinct artwork: the original forest medal and eleven new region-specific emblems. Their request IDs and reward names remain unchanged.
