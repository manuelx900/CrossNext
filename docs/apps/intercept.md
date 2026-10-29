# The Intercept

**How to play** introduces the setting and objective, then the player character,
Hooper on page 3 (the colleague exerting blackmail) and Harris on page 4
(the investigator), using
the existing story illustrations. The final guide page explains choices,
pagination and automatic saving. It does not reveal a solution or add a time
limit. Guide pages do not alter the saved story.

The interactive story requires `/apps/intercept/graphics.bin` from
`CrossNext-Resources.zip`. Extract the archive to the microSD root and merge
folders, then restart the reader. Keep `/apps/intercept/` complete. Without
compatible, readable resources, the app shows **Resources unavailable**, the
installation instructions, the folder path and a Back button. Play, choices
and restart are unavailable until the resources are installed and the app is
re-entered. A sprite read/checksum failure replaces the displayed scene with
that notice. Reading Adventure and PET use the same missing-resource screen.

The 46 scene IDs and three About/restart illustrations are CNART002 one-bit
sprites, built by `tools_local/intercept/build_resource_pack.py` from
`assets_local/intercept/png/`. The source story, translated text and node graph
remain in firmware. The existing INT1 save format is unchanged; progress stays
under `/.crosspoint/` separately from artwork. Installing or replacing the
resource folder does not replace saves.

`InterceptActivity` owns one `sdart::Pack`, opens it before loading progress and
closes it on exit. The loader validates count, dimensions, offsets, index CRC
and each image CRC before drawing it. Its optional PSRAM cache is 4,880 bytes
(49 records × 16 bytes + 4,096 bytes read-ahead), reused across frames. Fixed
scratch belongs to the activity; no extra framebuffer is allocated. Existing
small-read fallback handles PSRAM allocation failure.

Build all three app folders, fonts and dictionaries:

```
python tools_local/build_resources.py
```

Copy the ZIP contents to the microSD root. Never replace `/.crosspoint/`.

Device checks: remove `apps/intercept/graphics.bin`, enter The Intercept and
verify only Back/Volver exits; no Play or restart is available. Install the
complete folder, re-enter and verify the menu, every story branch and About
pages in both orientations. Re-enter with an existing save and confirm that
its chapter remains. Missing/corrupt PET packs should show the corresponding
`/apps/pet/` notice. Watch serial output for SDART read/CRC errors and confirm
heap/PSRAM returns after leaving either app. Hardware verification is required
before release.
