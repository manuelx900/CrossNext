# Intercept artwork

`png/scene00.png` through `scene45.png` are the canonical effective illustrations
for the 46 story scene IDs. Forty-three were decoded losslessly from the prior
compiled Huffman/RLE sprites; `art-sources.json` records their input hashes.
Scene 00, 31 and 32 preserve the Barrack, Suspicion and Custody references from
`assets_local/mystery_art/`. Those three are reduced with nearest-neighbour
sampling to fit the existing SD renderer's 280-pixel row limit. Other scene
pixels are unchanged. Document, Disk and DocumentRestart are the three small
reference illustrations used in About/restart views.

Rebuild with `python tools_local/intercept/build_resource_pack.py` or the full
`python tools_local/build_resources.py`. Do not add bitmap arrays back to
`src/apps_local/intercept/InterceptArt.h`: it declares only stable SD IDs.
`tools_local/quest/build_expansion_art.py` also delegates to this pack builder.
Keep the 49 IDs and their meanings stable. Valid custom CNART002 packs are
accepted; CRC32 detects corruption, not authorship. The original story's MIT
license is in LICENSE.txt and is included in the distributed app folder.
