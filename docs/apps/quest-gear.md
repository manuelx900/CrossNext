# Reading Adventure: campaign, equipment and scenes

PET and Reading Adventure now use [save generation 2](save-generation-v2.md). Older game files remain intact and are not imported automatically.

Reading Adventure keeps PET's rounded outlined cards and monochrome illustration style. The hero and PET stand together over a quieter grayscale home background, with one decoration. Encounters and reading-return attacks show the hero and enemy; the equipped PET still grants its passive bonus. There is no aura. The home screen shows their name and rank in a plain band above the unchanged-size illustration, four actions (Campaign, Missions, Equipment, Shop), and a bottom card showing the current campaign or active hunt. Battle cards use an outlined bow and a leading Campaign/Hunt label followed by reading progress on the same row. Pending campaign discoveries and rewards take priority; otherwise tapping an active hunt opens its detail, and Back returns to the hero, including after dismissing a cancellation dialog. Selecting Level opens an Experience window over the same hero menu; its background controls remain inactive until Done or Back closes it. The window shows level progress and remaining experience, or total experience and the maximum-level message at level 150. It also reuses the Equipment strip for total power, victory EXP bonus and coin bonus, with the same values and icons. The four action illustrations reuse the approved crossed swords, quest scroll, backpack and market stall reference. Overall campaign/book progress is available inside Campaign. Settings contains Profile, adventure reading data, equipment bonuses, and Start a new game with confirmation. Ordinary reading elsewhere in the reader advances the campaign. At the ending, Keep reading returns to the hero; it does not launch a separate timer or reader.

## Browser reading simulation

The browser simulator has a **Simular lectura** control above its scenario
selectors. Enter 0–1440 whole minutes and select **Simular lectura y volver**.
Positive values append one session to the existing virtual reading journal;
zero tests a return without reading. The current hero, active request, rewards
and campaign remain in the same running instance. The normal history scan
calculates progress and displays its reading/request reports. Close those
reports before adding another session. Scenario selectors still reset the
disposable test card. This control does not exist in native firmware.

The bridge queues one atomic command for the activity's loop task. Journal access
uses the existing `readingstats::recordSession` and `HalStorage` ownership;
the render task completes the command after the rescan is displayed. The
browser-only mailbox and preview flag add fixed storage; session writing reuses
the existing short-lived, fallible repository workspace. No persisted field,
extra task or framebuffer is added.

## Long campaign

The King of Oblivion erases stories to make people forget and obey him. The hero
discovers that reading weakens the monsters sent to destroy the world's books.
The hero travels to the king's fortress and defeats him to save the stolen books.
Finishing the campaign unlocks four royal rewards and opens the illustrated
epilogue before returning to the app.

The campaign has 12 sagas with ten Camino battles each. After the four-section
illustrated prologue, Camino highlights the available enemy with a thick black
border. Selecting it opens a short scene and the Fight action. An active fight
resumes directly, and completed entries are read-only. The Encounter tab alone
does not start a battle.

Each saga opens with its own illustrated introduction. Defeating the final
guardian grants an exclusive outfit, background, PET and decoration in the
same save as the victory. The celebration shows all four before the player
continues. Items already earned or bought remain in Equipment.

Saga offers an optional recap. At 0, 3, 6, 9 and 10 wins, it displays the complete
version for the latest reached milestone. Saga I also keeps the prologue at the
beginning. The Atlas records completed locations and their defeated guardians.

Each defeated guardian unlocks the next opponent after the player acknowledges
the compact victory screen. The screen shows the defeated guardian, applied
minutes, EXP, coins and any equipment. There are 73 themed enemy designs with
ready, weakened and defeated poses. The Shadow of the King guards the last
approach; the King of Forgetting is the final fight.

Reading uses the existing local journal. Only verified reading after the
saved cursor counts. Only the selected encounter and accepted Hunt advance. Extra seconds remain in reading statistics, without banking future wins or awarding passive EXP. Each completed attempt pays a fixed EXP prize once, when defeated. Back leaves a pending victory available without advancing. The reading report plays all three monster frames and settles on the pose for actual progress. A victory waits for Next. With no new reading there is no progress report. A Hunt advancing in the same session gets its own result. Manual simulator minutes follow the same combat credit rules.


Tall campaign invitation scenes fill the available height by cropping the sides of the regional backdrop. Heroes and companions keep their proportions and share the background ground line. The renderer restores its previous clip after drawing; it uses the same tiles and streaming buffers, with no new artwork records or full-scene allocation.

Missions presents a daily 15-minute goal, a weekly 120-minute goal (Monday through Sunday), and Hunts. Up to four eligible hunts are shown without pagination, with any active hunt first; a completed hunt remains in the history. Chapter discoveries remain part of campaign progression. These use only reading belonging to the current adventure. Daily and weekly rewards cannot be collected twice for the same calendar period. Chapter claims use 120 persistent bits. XP and coins earned before migration are retained; the old 1,500 claim bits remain solely for wallet compatibility.

## Equipment and artwork

The base catalogue has 101 outfits, 51 PET, 51 backgrounds and 60 decorations. Hunts add 74 campaign-guardian outfit identities, four illustrated exclusive outfits, 56 original bestiary outfits in male/female versions and the Eternal Ink Brooch. The other 57 expansion decorations remain reserved. Equipment and Shop share four categories. Hunt rewards appear in Equipment after collection. Opening a background in Equipment or Shop shows the full scene.

One decoration at a time appears behind the hero and PET. The saved elevated/ground fields retain their binary positions for compatibility; loading an older two-item scene keeps the active ground item, otherwise the elevated item, and unequips reserved items without erasing ownership. Each ornament has an authored preferred side; the layout chooses the alternative corner when it overlaps less of the characters' body cores. Decorations are painted before the unchanged opaque hero and PET masks. They never resize, displace or paint over either character. Placing any decoration replaces the previous one, across both anchors, and keeps owned items in the inventory. The enlarged anchor boxes are 124×140 for elevated ornaments and 124×108 for ground ornaments in the existing 480×320 scene camera; character masks still paint last. Decorations appear in the home scene, not regional battle scenes. Every item grants combat power, victory XP, coin fortune, or a combination. A background also has a regional affinity. Power combines level and equipment. Victory XP is capped at +30% and victory coins at +25%. Campaign time has a 40% floor, and Secret hunts a 75% floor. See [balance](quest-balance.md). Item previews show their bonuses and the resulting total combat requirement before purchase/equip. Victory XP bonuses affect monster prizes, not reading statistics. A victory freezes its coin reward; changing equipment after victory cannot increase it. Equipment changes can complete a battle if the newly reduced requirement is already met. Battle rewards also contribute to the existing XP-based level/coin progression.

The home scene uses a proportional 480-by-320 artwork coordinate space, independent of screen dimensions. Every hero source is fitted to 204 visible pixels of height in a 208-pixel frame. Outfits with wings or broad capes receive wider frames instead of being shrunk into 128 pixels of width. The apprentice and royal mage retain their approved dimensions. Opaque masks protect faces and clothing from scenery pixels. A separate battle arrangement reserves space for the hero and enemy, with a shared ground line. All layouts use the oriented viewable area and the existing UI fonts/input infrastructure.

The live prologue, brief Camino scenes and titles, complete Saga milestone versions and separate epilogue are authored in `assets_local/quest/campaign/paths.json`. Encounter names and the archived manuscript remain in `assets_local/quest/campaign/story.json`. Edit the corresponding file and regenerate; the generator never rewrites either source. Live budgets and reference resistance live in QuestBalance.h. UI copy lives in `tools_local/quest/seed_campaign_content.py`. Artwork sources are under `assets_local/quest/redesign/` (heroes, companions, backgrounds) and `assets_local/quest/campaign/` (story, enemies and exact image-generation provenance). Regenerate with:

```sh
python tools_local/quest/seed_campaign_content.py
python tools_local/quest/build_action_icons.py
python tools_local/quest/build_saga_covers.py
python scripts/gen_i18n.py
python tools_local/quest/build_resource_pack.py
```

The approved action, mission and path illustrations are fifteen one-bit flash masks (9,088 bytes), generated from source crops in `assets_local/quest/redesign/`. Twelve 176-by-112 four-level saga covers add 59,136 flash bytes. Their bounded row decoder draws directly through the existing target, without a temporary image buffer, heap allocation or new SD record. The first four covers use the supplied saga reference; the remaining eight reuse approved background emblems.

Missions frame their illustration, progress and coin/XP rewards. Equipment and Shop use larger figures, outlined wallets and bonuses, and status/price pills. Campaign tabs carry icons; Encounter places its reading instruction above the large battle scene, with applied/required minutes below and no battle progress bar. Path gives each chapter a framed emblem, and Saga uses Roman bookmarks and wide cover art. Profile, Settings, Reading Data and Bonuses share these outlined cards and icon hierarchy. Every completed path unlocks its objects in the shop alongside its EXP and coin prize; reading requirements keep their existing caps.

The required `apps/reading-adventure-v2/graphics.bin` is CNART003 with 1,103 records, 8,216,484 bytes. The original 1,071 sprites retain their pixels. Sixteen 112-by-144 decoration illustrations and sixteen opaque masks occupy IDs 1071–1102; their versioned masters and exact built-in image-generation prompts live in `redesign/masters/decoration-*-v1.png` and the sibling provenance JSON records. Twenty-four 240-by-320 tiles store the twelve saga scenes at IDs 1041–1064. The illustrated sword adds three 192-by-192 ink frames at 1065–1067 and three opaque masks at 1068–1070. Its master and exact generation prompt are `assets_local/quest/redesign/masters/combat-sword-v3.png` and `assets_local/quest/redesign/provenance/combat-sword-v3-prompt.txt`; the resource builder prepares the three rigid rotations while preserving alpha. Masters are `assets_local/quest/redesign/masters/saga-scene-00.png` through `saga-scene-11.png`; exact prompts and generation records are in the sibling `provenance` directory. Install the resource and current language packs alongside the firmware, following [SD resources](../sd-resources.md). An older pack is rejected with an install-resources message. The shared decoder streams artwork without an additional framebuffer or full-scene allocation. Index and read-ahead occupy 21,744 bytes in the existing PSRAM-aware allocation path, 512 bytes more than the preceding sword pack.

## Save and retirement

RQU6/RQU7 use two checksum-protected 512-byte records in `/.crosspoint/reading-adventure-v2/reading-quest-v6.dat`. RQU1 through RQU5 remain readable and their original files are retained. Migration preserves name, sex, XP, coins and equipment. It begins the new campaign at its first encounter with a cursor at the current reading total: historical reading is not awarded again. Existing reading totals remain visible until the user starts a fresh adventure. A corrupt newer save never silently falls back to an older format. Actions publish their new state only after a verified save.

The campaign record keeps introduced-book count at byte 400 and uses extension marker 6 at byte 401. Byte 402 records prologue acknowledgment; bytes 403–404 hold the number of introduced encounters (0–720). Markers 0 through 5 remain readable. Bytes 405–406 store sixteen decoration ownership bits, 407–410 store the two active decoration IDs, and 411–412 store a pending discovery. Original equipment IDs and the 25-byte ownership bank remain stable; the old empty-slot value 200 is translated at the codec boundary. Already owned equipment stays owned. Completing an ordinary path now unlocks purchases rather than restoring free path gifts. Saga and Hunt rewards keep their direct delivery. No old reading or EXP is recredited. Back up the save before downgrading to firmware that predates marker 6. An already started encounter retains its context acknowledgment; a save waiting at the next book still requires that book and its first encounter to be introduced. An untouched adventure opens the prologue. Reset clears all three checkpoints. Acknowledgments use the existing verified two-slot transaction and retain the 512-byte record size.

Start a new game requires confirmation in Settings. It clears this app’s XP, spending, rewards, purchases, name and campaign, then opens hero selection and naming with a new reading baseline. The reader’s books and general reading journal are preserved and are not counted again. A failed save leaves the previous game available.

The campaign follows the hero and has no character index. The PET and decorations still appear in the home scene and keep their normal equipment effects. Revised prose and title overrides have their own translation keys. An installed language pack that predates the new story falls back to the new English text for those entries.

The current host profile is 7192 bytes and the encoded RQUA record is 8192 bytes; the earlier 512-byte formats remain importable. Store
scratch and drafts are activity-owned. Narrative pagination borrows translated
strings and uses the existing line buffer. The selected encounter identifies the
current Camino preview. The return report and animation clock use bounded
activity state.

## Verification before publication

Automated suites: `python host-tests/quest-campaign/run.py` and `python host-tests/quest-redesign/run.py`. Rebuild real-firmware browser scenarios with `python tools_local/quest/build_simulator_fixture.py`; open `/qa-reading-adventure/`. These are seeded virtual-card scenarios, not real user progress. The three return scenarios seed twenty minutes of new reading for combat, pending context and victory. The Discovery and Decorated scene scenarios expose the fairy reward and one equipped home decoration. Collection includes all 60 active decorations. The standalone `/qa-reading-campaign/` remains a design prototype.

Back up the SD card, then test on X4 Pro before publication:

- Fresh setup requires sex then a name, and persists the chosen hero/fox/forest. Existing saves retain purchases and do not recredit old reading.
- Complete the prologue and Saga I introduction: Camino must open with the first path available. Opening Encounter before choosing it shows the hero and a path action. Select the available enemy, check its illustrated scene, then select Fight. Verify that the final fight of the saga shows the four rewards, followed by the next saga's introduction.
- Read while the prologue or an outcome is pending: full reading statistics must remain, but no passive EXP or future-enemy credit is granted. Navigation cannot defeat another enemy. Migrate old partial slots within the same chapter without resetting gear, requests or reading.
- Compare the apprentice, royal mage and broad winged/deity outfits on Home and in the encounter. Their visible heights and ground line must remain consistent, with complete silhouettes. Home keeps a readable PET; encounters and attack summaries show only the hero and enemy.
- Read normally, reopen Adventure, and verify a summary of only the newly recorded time and any fixed victory EXP. The monster cycles through its three poses, then settles according to progress: ready below 50%, weakened from 50%, and fallen from 90%. The last pose alone does not award a victory; the reading target must still be met. Check that Next waits until the animation finishes. Exit and reenter without new reading: progress and rewards must not duplicate.
- Win a battle and check applied/required minutes without a battle progress bar. The victory must wait for Siguiente camino. Check EXP, coins, shop unlocks and saga equipment, exit and reopen the pending victory without duplicate rewards, then choose the next enemy. Verify one active decoration, equipment replacement, missions, shop level gates and balances after restart.
- Check all four rotations, touch and logical physical navigation, all Campaign tabs, first/last catalog pages, large pets, and the King of Forgetting's three poses. Text and figures must remain separated; scenery must clear on refresh.
- Cancel Start a new game, then test the campaign ending on a backed-up test save. Reopen the saved ending. Start again must show setup, zero adventure stats and unchanged general reading history.
- With missing graphics show the SD-resource message; with write failure show an error without granting/publishing a new reward or reset. Corrupt one save slot and verify recovery from the other. Do not delete a real save to test migration.

Browser and host checks do not verify physical e-ink ghosting, SD power-loss behavior, touch calibration, or runtime heap. On hardware, unexpected restarts, duplicated rewards, old figures left on the screen or progress changing without reading are failure signs.

Path illustrations are delivered in `apps/reading-adventure-v2/story.bin` alongside `graphics.bin`. Copy both from the resource bundle; a missing, incompatible or unreadable pack shows the resource-installation screen and prevents campaign actions. The story pack preserves the save format; current battle targets are provisional. This separate 4,611,864-byte pack has one original illustration for each of the 120 current paths and stays below the per-pack 8 MiB limit. Its 240-record index and read-ahead use 7,936 bytes of PSRAM while Adventure is open, plus the pack object's existing fixed member buffers. This is 1,536 bytes more index storage than the former 72-scene pack. It streams two 240×320 tiles per scene through the existing renderer, without a scene-sized allocation or an extra framebuffer.

Test battles in several sagas and all four rotations. Verify victory fits on one screen and opens the next unselected path. Test page boundaries and complete text separately in the optional archive. Remove only the new story pack on a test card: the install-resources screen must appear without changing progress. Restore the file and reenter. Repeated entry/exit should release the new pack buffers; monitor runtime heap and SD errors on the device.

The editorial sequence is documented in [the narrative guide](../../assets_local/quest/campaign/narrative-guide.md). The current campaign text is in `paths.json`. The three seals open the way to the fortress, and the last fight defeats the King of Forgetting.

Long narrative text is measured and drawn with the SDK's uncapped text-area walker, so a paragraph longer than the chrome layout's 16-line limit continues across pages instead of being ellipsized. The Activity-owned work area gains one fixed 224-byte line buffer; there is no new heap allocation, save field, artwork file or task. A battle result displays its illustration only on the first text page.

The 120 paths each contain one brief approach. The Markdown copy also includes every Saga version and the separate epilogue. Export it with `python tools_local/quest/export_story.py`.

On a test card, finish the prologue, choose the wolf in Camino and press Fight below the illustration. Read, verify the victory rewards and return to Camino to select the next enemy. Check partial positive sessions, excess-minute handling and zero-minute returns. Check that the completed episode has a checkmark and cannot be opened. Open Saga to read the latest unlocked version. Verify reference-size regular status text,2px dividers with14px side gaps, and round right-side completion badges. Side buttons and horizontal swipes must change text/list pages without advancing a battle.


## Atlas, requests and returning to the journey

Missions keeps daily and weekly goals and adds the Hunts board in place of
character requests. Four illustrated cards fit per portrait page, two in landscape.
The active hunt stays first with an In progress badge. Cards show a regional
guardian, reading goal, EXP and coins. Tap one for the battle scene, progress,
EXP, coins and reward. A hunt starts from the current reading total. A single
session can progress the selected campaign fight and the hunt independently.
Cancelling or replacing an unfinished hunt requires confirmation. Once defeated,
the guardian can only be claimed. After claiming, the enemy appears in Recuerdos
as a historical victory and can be fought again in the same adventure. Each distinct
guardian becomes available after its first defeat in Camino. Each new attempt starts
from fresh reading and keeps its accepted goal, including a repeated creature.

The Atlas is reached from Saga. Its regional cards list completed locations and
open their defeated guardians. Viewing the Atlas performs no save. If a battle
and a hunt both advance in one session, the battle report appears first.

Equipment and Shop center the page control and place information immediately
before the wallet. Their attack and currency symbols use the app's bold outline
style. The header follows the same interface policy as PET: black for Frame and
Carousel, white for Carousel 2 and Bookshelf, including naming and shared section
overrides.

The canonical task source is `assets_local/quest/campaign/requests.json`.
`request_content.py`, called by the normal campaign generator, validates its 300
stable IDs, bilingual text and unique item names and generates `QuestRequestData.h`
and translated keys. Changes to
request IDs would change collection identity and must not reorder existing IDs.

`QuestRequests.h` declares the bounded request policy; `QuestCampaign.cpp` owns
acceptance, progress and reward guards. The activity uses its existing draft and
verified-save transaction before publishing a claim or acceptance. Request claims,
active ID and reading baseline occupy a previously reserved 48-byte extension of
the existing 512-byte record. Schema 3 and earlier saves migrate with no accepted
request or collection claims. Equipment, balances and reading cursors survive.
The in-memory Profile is 464 bytes. Expansion ownership and the ending acknowledgment add 16 aligned bytes to the earlier 424-byte request profile; twelve saga reading anchors add another 24 bytes. Across the activity's three existing profile copies, the combined increase is 120 bytes over that request-only baseline, of which 72 bytes belong to the saga anchors. No new task, framebuffer, SDK/HAL path or runtime dependency is introduced. The three new pack objects retain their bounded member buffers, and their index/read-ahead allocations use the existing PSRAM-aware pack path. Flash and panel behavior are verified
separately; no runtime-heap or power improvement is claimed.

Physical checks before publication:

- Reopen without reading: go directly to the hero. Read and reopen: see the
  minutes report; Continue and Back must not open a recap afterward.
- Accept a known request, read part of its goal, restart and verify the retained
  progress. Finish it and collect once. Restart: EXP, coins and its keepsake must
  remain, and collecting again must be impossible. A failed save must keep the
  earlier valid claim/acceptance and leave a retry possible.
- On an older backed-up save, check wallet, equipment and atlas progress before
  accepting a new request. Later patrons and chapter titles must stay hidden.
- Visit partial and completed regions, all atlas pages and a chapter ending;
  no white erasure blocks or pasted landmark may cover the illustration.
- Inspect request details, rewards, both catalogue tabs, centered pagers and
  wallet information in portrait and landscape, using touch and mapped buttons.
- Check all four home layouts, naming, settings-button focus, both hero sexes,
  and repeated entry/exit. Missing text, duplicated rewards, stale focus, SD
  errors or unexpected restarts are failure signs. Keep existing resources and
  saves; no cache deletion is required.


## Expansion rewards and campaign completion

The canonical expansion catalogue is `assets_local/quest/expansion/catalog.json`.
The catalogue retains its historical 289 job identities. Only 166 active masters are consumed: 101 decorations, 60 three-pose enemy strips, the royal outfit sheet, background and PET, and two exclusive victory scenes. The 120 earlier chapter scenes and three request scenes are retired and excluded from the builder. Existing equipment and enemy identities retain their pixels and IDs.

Run `python tools_local/quest/build_expansion_art.py` to produce the expansion
CNART003 packs. The complete resource builder includes them automatically:

| Pack | Records | Size | Role |
| --- | ---: | ---: | --- |
| `rewards.bin` | 593 | 7,379,560 bytes | Decorations, royal equipment, exclusive hunts and victory illustrations |
| `story.bin` | 240 | 4,611,864 bytes | One current-story scene for each of the 120 paths |
| `guardians.bin` | 360 | 6,255,384 bytes | Three ink poses and masks for 60 new guardians |

Each remains below the decoder's 8 MiB per-pack limit. The active packs reuse
the normal index and 4 KiB read-ahead buffers. No retained pixel cache is
requested. Packing crops transparent gutters, scales
artwork and encodes existing four-level coverage; it does not invent or duplicate
illustrations. The explicit `--preview` option allows incomplete layout fixtures;
those packs are marked as previews and must never be distributed.

Legacy 512-byte records use schema marker 5 for the expansion ownership bank at bytes 461–473 and ending acknowledgment at byte 474. Equipment and request identities retain their original positions during import. Already-owned objects stay owned. Ordinary completed paths unlock shop purchases; only completed saga sets and already-collected Hunt decorations are restored as direct rewards. Hunt rewards retain their outfit, usable decoration, EXP and coins.

Acknowledging the final story enters Peace and grants the King of Forgetting armor,
recovered throne background, crown dragon PET and Crown of Oblivion decoration.
The selected scene-first celebration allows opening any of those rewards or
returning to the hero. The ending acknowledgment is saved through the normal
draft/verified-save transaction. A read-only card or failed acknowledgment save
still allows leaving the screen for the current visit. Existing completed saves
receive the same rewards and celebration on their next successful load.

Peace and imported closed campaigns continue to track reading, daily and
weekly goals, accepted requests, equipment and the recovered atlas. Campaign
encounters stay frozen. New reading opens the minutes-read report with the equipped
home scene, including its decorations; it does not replay the victory celebration
or promise a next encounter in a closed journey. No new
reading opens the hero directly once the ending has been acknowledged. Restart
is optional under Settings, with confirmation, and resets the profile as before.

Additional physical checks before publication:

- Install all five artwork packs. Remove one expansion pack temporarily: the
  installation screen must appear before profile actions; restore it and reopen.
- With a backed-up old save, compare name, wallet, equipment and reading cursor.
  Earned new ornaments should appear without a second EXP/coin reward.
- Inspect elevated and ground decorations one at a time with wide outfits, the largest PET and both hero
  sexes in all orientations. Character pixels and proportions must remain intact;
  the ornaments should have readable black contours and fit their corners.
- Complete a mapped request, restart, and verify its usable decoration and original
  keepsake persist. Collecting twice must be impossible, including after a failed save.
- Finish the final story. Verify the exclusive celebration and all four royal items,
  then return to the hero, reopen without reading, and reopen after new reading.
  Daily/weekly claims and requests must remain usable; the last enemy must stay finished.
- Follow the battle episodes of several regions, checking ready, weakened and fallen
  guardian poses. Check the preview before a fight, the completed location in the Atlas
  and the appropriate recap in Saga. Later story revelations must remain hidden.
- Repeat entry/exit while monitoring free internal heap and PSRAM over Serial.
  A steady loss, failed resource read, restart, blank reward or clipped character is
  a failure sign. No book-cache deletion or save reset is required for this update.

## Equipment detail and campaign re-reading

Equipment and Shop share the selected object-first detail layout: a framed illustration, three benefit cards and one purchase/equip/remove action. The encounter-time comparison appears only for an active campaign battle. The generic post-campaign reading-benefit box is omitted, and owned items show their ownership state once inside a larger illustration panel. Unowned items keep their purchase/source information. Outfit catalogue cards show item power with the sword icon; other categories keep their primary bonus. Long landscape names receive more width when two lines would otherwise overflow. Mission targets, progress and request summaries use the intermediate menu text size. Text is bounded by its allocated region and restored clip; labels cannot paint into neighbouring cards. Request objects use purpose-specific illustrations (including compass, needle, map, clock, mirror, crown and balance scales), rather than reusing a key-shaped tool for unrelated objects.

Selecting an available saga opens its current version. Saga I starts with the
prologue. The outlined page count and contents button support rereading. Previous
and Next turn pages, while the header Back action returns to the menu. A bookmark
for an older version opens the start of the current version.

Each saga remembers its current version and page. The prologue keeps its own
positions within Saga I. Page, seen-item and favorite changes are coalesced for
1.5 seconds and saved by the idle render worker. Leaving the app or entering
sleep flushes pending changes; critical actions include them in their immediate
verified save. See [the persistence and rendering budget](quest-performance.md).
Re-reading does not alter campaign progress
or rewards. The text walker borrows the selected translation and paginates
through the existing line buffer. Camino introductions use their own current-story scene. The Atlas and battle outcomes keep the regional scenery and appropriate guardian pose.

The 512-byte profile retains the twelve 16-bit bookmarks at bytes 476–499.
An old chapter bookmark opens the current Saga recap. The latest extension
marker is 6.

## Simulator inspection

`python tools_local/quest/build_simulator_fixture.py` uses the shared exporter and installs the version-6 QA catalogue. `python tools_local/quest/export_simulator.py --output .cache/quest-simulator` produces codec-validated fixtures in a separate output folder for all120 campaign battles, both sexes and six moments, plus the standard scenarios. The QA page selects saga, chapter, its associated guardian, moment, orientation, starting panel and an equipment object. WebAssembly-only environment hooks choose the renderer orientation on entry and select the initial UI after the normal profile/history load; they do not bypass a reading summary or unseen ending. These hooks are absent from device builds and modify only the disposable virtual card. The collection scenario owns all 60 active decorations for paging checks. Changing a QA scenario resets that virtual card and its bookmarks; leave and reopen a saga inside the same running scenario to inspect position restoration.


## Legacy requests and save compatibility

A save may contain an active request for minutes, pages, sessions, a finished
book or days. Its original metric remains valid until it is cancelled or
collected. Any hunt started or accepted as a replacement uses reading minutes.
Keepsakes and collected rewards retain their existing claim bits. Reward claims
and confirmation use the app's verified save transaction.
The Keepsakes catalogue shows rarity without a repeated-victory count over its
stars. Selecting a beast opens its record, where the ×N badge shows the number
of victories. PET and Reading Quest use the same header Back arrow geometry and
painter as the other menus, with the existing header polarity.

The legacy request-goal extension in schema 5 uses the same 512-byte record.
Its optional eight-byte
extension at 500–507 uses bit 63 as its presence flag, bits 9–62 for the baseline
of a non-minute goal, bits 3–8 for reported progress, and bits 0–2 for the goal
kind. The original requestStarted field always retains its seconds baseline.
For these legacy records, writing with older software may
drop the additional goal metadata and resume that active request in minutes.
Missing extensions preserve the original minutes behavior. Existing bookmarks,
equipment, balances and claim bits keep their positions. The current host Profile
is 488 bytes. Marker 7 moves reported progress to its full 16-bit field so the
Secret hunt can retain its 1,800-minute target.
The reusable reading report is 48 bytes. No new heap buffer or task is allocated.

The shop lists unowned items whose level or path requirement has been met.
Story and request rewards are earned through those activities. Equipment still
shows everything already owned. Encounter opens the encounter section directly;
a cleared path shows the hero, PET and region, with a continuation action.
Atlas regions list campaign victories in portrait cards with standing guardians.

The new keepsake illustrations and single coin are packed at 96×96 monochrome
pixels by tools_local/quest/build_keepsake_icons.py. Original transparent PNGs,
exact prompts and source SHA-256 hashes live in assets_local/quest/keepsakes/.
The generated header contains only static const bitmap data. No SD artwork pack
changes or extra image workspace are needed. Run the packer without --preview
for delivery; it requires every original and verifies its provenance checksum.
Preview mode writes to a separate explicit cache path and must not be shipped.

The simulator has fixtures for old goals, hunt-only returns and
combined battle/hunt returns, and a level selector covering every level through 150. Per-scenario
history journals are built through the production record codec. These controls
reset only the virtual card; on-device profiles are not modified by the website.

Before device publication, back up the card and check: an old active minutes
request, new minute goals, interrupted two-report returns, once-only collection,
level-gated shops, long item details, cleared encounters, and distinct atlas
chapter endings. Repeat in both orientations with touch and mapped buttons.
Clipping, missing illustrations, repeated rewards, lost progress or a restart
are failure signs. No book cache deletion or profile reset is required.

## Campaign and request continuity

The former authored request data remains available as reference. New hunts use
minutes. The 512-byte profile keeps its existing layout. Campaign extension
version 6 assigns two spare request-claim bits to the Shadow and King hunts and
translates the older no-active-hunt value on load. Existing final-region claims
keep the enemy they originally fought. Firmware that only understands extension
version 5 cannot open version 6 records. The original battle budget remains
available, and current targets use the provisional chapter-opening budgets.

The keepsake set contains twelve distinct regional medals. Eleven new medals join the existing forest medal; other related objects may intentionally share an illustration family. The obsolete `moments.bin` is removed from distribution; its 123 earlier-story illustrations are not loaded or rebuilt. No second framebuffer, image cache or activity task is added.

## Path selection, reading summary and counters

The route is prologue → saga opening → choose an enemy in Camino → illustrated scene → Fight → read → reading report → Next → victory and rewards → next Camino. The last victory in a saga shows four reward cards, with the background represented by its theme icon. Its full scene is available from the Equipment detail. In Encounter, the reading instruction sits above the battle
scene and the progress card below it. Path rows retain the vertical separator
and circular completion mark. The selected encounter persists across reloads.

Campaign totals use the verified adventure reading baseline and the current
regional equipment bonus; unused session time does not prepay later opponents. Future targets can change with
equipment choices. Combat thresholds and the length of the campaign remain
provisional. The illustrated Camino scene explains what the enemy is blocking. The optional Saga recap grows at its five victory milestones and longer passages continue onto another page.

In the simulator, verify a new game and complete the prologue. Visit Encounter
without selecting a path, then tap the first enemy in Camino and select Fight. Simulate partial
reading, then enough to win. Check the full meter, reward and next unselected
enemy. Start a hunt and simulate reading that advances both it and a fight.
Verify the defeated hunt moves to Recuerdos and cannot be started again. Check legacy requests, royal rewards and continued
reading after the final battle. Repeat in portrait and landscape. This delivery
builds the simulator only.

## Hunt ranks

Basic hunts are repeatable campaign rematches. Each grants an outfit named for
the defeated guardian, rendered with the corresponding existing wardrobe style,
plus its earlier XP, coins and keepsake rewards.
The Storm Owl is Elite and requires 105 accumulated minutes. The Crystal Stag
and Ashwing Phoenix are Legendary and require three and five hours. These four
exclusive beasts, including the Secret dragon, have separately illustrated
ready, weakened and fallen poses. Their outfits have male and female variants
matching the existing avatar artwork.

The Dragon of Eternal Ink requires thirty accumulated hours. Each collected
hunt from the third onward can reveal it, with a one-in-eight deterministic
roll based on the saved adventure and reading cursor. The twelfth collected
hunt guarantees discovery. The successful collection and discovery share one
verified save. Once discovered, it is always listed after any active hunt.
Pausing it preserves exact seconds and permits other hunts. Finishing it grants
its outfit and the Eternal Ink Brooch. Ordinary hunt replacement retains its
existing progress-reset confirmation.

Extension marker 7 stores the discovery flag at 279, paused seconds at 280–283,
the full 16-bit report counter at 284–285, and four new claims at 286. The first
302 claim bits retain their original positions. Markers through 6 retain their
old active-hunt sentinel and six-bit report semantics. Hunt outfits derive
ownership from their existing guardian claims and use item IDs 321–398. Item
399 is the brooch. The reserved empty item value remains 320.

The rewards pack adds 42 streamed records for twelve beast poses and masks,
eight avatar sprites and masks, and the brooch. Rebuild it with
`python tools_local/quest/build_expansion_art.py`. The approved masters are in
`assets_local/quest/hunts/`. No extra activity, framebuffer or rendering cache
is created. The enlarged index uses the existing SD pack allocation.

Test a new and a legacy save, a partly read Secret hunt across pause/restart,
a thirty-hour completed report, both avatar sexes and both orientations.
Verify that all claimed hunts remain read-only and only a fresh adventure
clears their history.

## Expanded bestiary

The hunts catalogue contains 56 additional creatures, bringing exclusive hunt
beasts to 60 alongside the campaign revanches. They cover forest, tidal, frost,
dune, clockwork, night and astral habitats. Each new beast has three separately
drawn poses: ready, weakened and defeated. The existing return animation uses
these poses and settles on the actual reading progress.

New hunts unlock gradually with campaign victories. The board keeps its limit
of four available hunts, with the active one first and the discovered Secret
next. Campaign creatures and exclusive beasts share the board. Each new victory
grants the displayed XP, coins and a named outfit. These 56 outfit rewards have
original monster-inspired illustrations in male and female versions, with
independent ownership and rank bonuses. Claimed
hunts remain in the read-only history until a fresh adventure.

`assets_local/quest/bestiary/catalog.json` is the source for the 56 names,
outfit identities, difficulty, durations and unlocks.
Run `python tools_local/quest/build_bestiary.py` after editing it, then rebuild
the art with `python tools_local/quest/build_expansion_art.py`. Seven generated
masters and their prompts are kept in the same asset directory.

Save extension 8 keeps the 512-byte profile format and all earlier item IDs.
It stores the 60 exclusive claim bits in bytes 286–293, below the existing
checksum. Version 7's empty hunt value is migrated from 306 to 362; existing
active hunts and claims remain intact. The brooch stays item 399; new outfits
use 400–455. RAM profile size rises from 488 to 496 bytes. No new task, activity,
framebuffer or pixel cache is added. The existing streamed rewards pack now has
593 records (9,488 bytes of index entries) and occupies 7,379,560 bytes on SD,
within the pack reader's 8 MiB limit. Internal heap and physical e-ink refresh
behavior still require device verification.

The simulator's Bestiary scenario can select all 56 new creatures. Accept one
and inject partial reading, complete its target, collect its rewards, then check
Equipment and the read-only history. Check both orientations and both heroes,
and load a version 7 save before testing a new adventure.

## Original bestiary outfit artwork

Each bestiary beast has its own outfit with two newly drawn avatar sprites.
The 112 figures preserve the existing hero faces, proportions and monochrome
contours. Helmet, cloak and armor motifs come from the corresponding beast.
Item IDs 400–455, ownership, statistics and save format remain unchanged, so an
already earned outfit displays its new illustration after updating resources.
The PET and the existing wardrobe are unchanged.

Seven masters are in `assets_local/quest/bestiary-outfits/`; `provenance.json`
records the generation prompts, hero/monster references and master checksums.
Run `python tools_local/quest/build_bestiary_outfits.py` to rebuild the separate
`apps/reading-adventure-v2/bestiary-outfits.bin` resource. The normal app-resource
builder includes it. Copy this file to the card with the other app resources.
It is required by the updated app; older resource sets are incomplete.

This pack contains 224 records: 112 figures and 112 masks at the existing
160×208 avatar canvas size. Its 1,867,288-byte SD file keeps the existing packs
below their 8 MiB limits. The activity owns the additional Pack from entry to
exit, reusing the existing streamed bitmap renderer. It requests one 7,680-byte
PSRAM block for its 3,584-byte index and 4,096-byte read-ahead; allocation failure
uses the existing small-read path. No retained sprite cache, framebuffer or
task is added. The host-stub Pack object measures 1,256 bytes; native heap and
PSRAM measurements remain device checks.

Preview all male/female variants, equip them and check the home scene, campaign
battle, hunt, reward card and name editor. Confirm that a loaded claimed hunt
still owns its matching outfit and that replacing the art never grants another
reward. On device, check masks over busy backgrounds and enter/leave the app
repeatedly while watching internal heap and PSRAM.

## Current path illustration sources

Each path ID in `assets_local/quest/campaign/paths.json` maps to the same ID in
`assets_local/quest/campaign/paths-v2/catalog.json`. The 120 independent originals
and their exact image-generation prompts, source hashes and generation records
live under `paths-v2/masters` and `paths-v2/provenance`. Scenes use the current
region, obstacle and actual guardian anatomy. The hero is off camera so the
illustrations work with either hero and any equipped outfit. The prologue and
dynamic three-pose battles are unchanged.

Run `python tools_local/quest/build_story_art.py` to pack the complete 3:2 scenes.
It checks source-story identity, per-image provenance and unique four-tone pixels
before writing 240 bounded tiles. Old 72-scene masters and the 123 retired
expansion scenes are removed from active source directories. A local recovery
archive is kept at `.cache/quest-path-art/retired-art.zip`, outside distributed
app resources. Replacing the old story pack and dropping `moments.bin` reduces
the SD artwork set by 2,882,424 bytes. Remove an older card's
`apps/reading-adventure-v2/moments.bin` when copying the new resource set.

In the simulator, preview the first and last path of every saga, then select
Camino, start a fight and return from partial reading. The approach must use its
own scene; the fight must retain its animated monster and equipped hero. Reopen
Saga I to check that its prologue remains available. On hardware, verify all
orientations, clean refreshes and pack release across repeated entry/exit.

## Hunt change confirmation

Changing hunts compares the active creature and the new one side by side. Each
card shows its monster, full name and reading target; the active card also shows
accumulated progress. The warning remains outside the cards, above the two
actions. Keep current hunt receives the initial focus. Cancellation uses the
same card style. Ordinary changes reset progress; the secret hunt keeps its
existing pause/resume behavior. No new state or artwork pack is stored.

Preview EN/ES in portrait and landscape, including long monster names and a
failed save. The warning and names must remain complete, Keep must preserve
progress, and changing must stay disabled when storage is unavailable.


## Current collections and recoverable saves (RQUA / v10)

Available Hunts shows at most four creatures on one screen, with no page controls.
The accepted hunt is first; its black In progress label replaces the minutes in
that card, leaving the illustration clear. Open it to see accumulated and required
reading. The remaining cards mix unlocked campaign rematches with exclusive beasts.
Campaign creatures unlock after defeating them in Camino. They are Basic, except
Saga XII creatures are Elite and the Shadow and King are Secret. Exclusive beasts
use higher ranks. Each accepted attempt is collectible once; unlocked creatures can then be hunted again indefinitely.

Before acceptance, hunt detail shows only the creature centered on its regional
background. Accepting begins the hero-versus-monster view. Its header Back and
Return to menu action return to the game home screen. Switching or cancelling
keeps the existing confirmation and Secret pause rules.

Memories alone keeps pagination. Each creature opens a history card with its cumulative win count and Hunt again,
showing the first (standing) form, latest victory date, reading accumulated while that
hunt was active, books finished during combat when recorded, equipment rewards
and the EXP/coins awarded at collection. Atlas now uses the same portrait card
grid for campaign victories, grouped by Saga; opening a card shows a victory
record instead of replaying a story. Saga remains the narrative archive.
Reading is recorded in seconds and displayed rounded up to minutes. Later balance
changes do not rewrite the stored numeric reward. RQU8 retains its saved hunt
dates and reading. Older formats have no such hunt metadata; campaign records and
book counts were not stored before RQU9. Missing values display Not recorded or
are omitted; no date, reading or book count is fabricated.
Legacy duplicate hunts are grouped by creature, retaining the actual claimed
source for its reward display. Their original claims and wallet remain intact.

Equipment defaults to reverse acquisition order and has a Favorites filter in
each category. Open an owned item to clear New; its detail star toggles Favorite.
Favoriting, equipping or reopening does not change acquisition order. Migrated
possessions are appended in catalogue order with no New labels because their
acquisition dates are unknown. New rewards are appended once. Restart clears
favorites, New flags, acquisition order and creature history with the adventure.

Narrative wording remains unchanged. Prologue, Camino and Saga use the larger
menu text face, increased line spacing and justified soft-wrapped lines. Paragraph
endings stay left aligned; excessive gaps fall back to left alignment. Pagination
and saved section bookmarks continue to use the existing measured text walker.
All Adventure settings buttons have a fourteen-pixel inset from the safe right
edge; the clock shares this extra margin.

New saves use two checksum-protected 8192-byte records in
`/.crosspoint/reading-adventure-v2/reading-quest-v10.dat`. The first512 bytes preserve the previous
layout; the QCOL extension stores bounded acquisition order, favorite/New bitsets,
134 hunt records (day, elapsed seconds, EXP, coins), backup day and an
accepted hunt's carried progress. The QWIN extension adds120 campaign records,
book counts and bounded active-combat counters. Unknown historical fields remain
Not recorded; finished-book counts are shown only when known and positive. The complete record has an additional checksum.
The encoded file holds one or two slots. Journal rotation follows the latest valid physical slot, including after a truncated file is repaired. RQU1–RQU8 still load from their original
paths, which remain untouched. A corrupt newer file never silently falls back to
an earlier format. Future-version primary or backup data is not overwritten.

Each ordinary semantic save verifies its primary journal slot. There is no
Backup screen, manual backup/restore action or new external backup directory.
Valid older external copies are still read to recover a missing/corrupt primary.
After a primary journal is verified, known supported v10/v9/v8 external copy
files are retired. Directory removal is non-recursive: unrelated files and
unsupported newer copies are preserved. Cleanup is checked once per store
session. The existing internal alternate-slot, temporary-file and previous-file
recovery protects against interrupted saves. It does not protect against losing
the card; retain a computer copy before updates or downgrades.

Shop items remain selectable with an insufficient balance. Buy opens a rounded
notice showing the price and available coins, with one OK action returning to
the shop. No purchase or spending occurs. Affordable purchases keep the normal
confirmation. The bottom Campaign/Hunt label has a 12px bottom inset; English
profile gender labels are Male and Female.

The host Profile measures7192 bytes and StoreWork15392 bytes.
The activity owns two profiles and one StoreWork:29776 bytes, a14632-byte increase
over RQU8, plus existing UI fields. Each primary journal is at most16384 bytes.
Previous v9 and v8 primaries remain available for import, including verified
pending and previous journals. Supported external copies are retired only after
a verified primary is available. New metadata uses bounded arrays, not per-frame allocation.
The existing single framebuffer and streamed artwork are unchanged. Native S3
heap/stack and physical SD interruption tests remain required before publication.

Verify four offers/no pager, campaign ranks, centered pre-acceptance art, Back to
home after acceptance, Favorites/New after restart, and a newly earned dated
victory. Test both orientations and header appearances. On a disposable card,
import an old external copy, verify its retirement after a primary save, and
simulate interrupted writes without duplicate rewards. Confirm insufficient
coins leave both equipment and wallet unchanged and OK returns to the shop.
Physical-device verification remains required; no device is flashed here.

## Shop confirmation and reading return

Settings contains Profile, Reading data and Bonuses. The redundant
My campaign entry is removed; campaign play remains under Campaign. Bonus limits
use three compact percentage cards and a separate short reading-preservation note.

An unowned purchasable item's detail highlights its coin cost and required level
next to the purchase action. Buying opens a confirmation showing the item, price,
current balance and remaining balance. Back/cancel preserves the item preview and
spends nothing. Only explicit confirmation calls the existing purchase/equip
transaction, which rechecks ownership, eligibility and funds. Owned equipment
keeps its immediate equip/remove behavior. The modal uses existing preview state,
logical-input decision focus and read-only guards; no save format or heap is added.

Reading still shows its three combat animation frames and waits for a manual
Next. When both activities progress, campaign feedback and its victory rewards
precede hunt feedback. The completed hunt report lists equipment and numeric
rewards; Collect and continue grants them through the existing draft/save
transaction, without reopening hunt detail. Partial hunt reports return to the
ongoing encounter or home. Failed saves retain the report for retry, with no
duplicate payout. Leaving a report with Back does not collect its hunt reward.

The return attack uses six decorative letters that travel from the hero toward
the monster in three frames, with a white outline over scenery. Campaign and
hunts share the same effect; no weapon is drawn. It borrows the current outfit
geometry and UI glyphs, restores clipping and disappears on the settled result.
The three monster poses, reading totals, manual Next and reward transactions are
unchanged. The existing sword records are retained in the resource format for
compatibility, but are no longer used by this animation.


The current progression ceiling is level 150, with thirty titles. The RQUA QBAL extension revision 2 imports revision 1 without losing EXP, spending or already-earned level coins. See [progression balance](quest-balance.md) for the curve, Hunt rewards and measured campaign duration.

## Reading return and milestone celebrations

Each battle that progressed has one result and a manual continue button. The campaign result includes its rewards, without a second ordinary victory screen. If a Hunt also progressed, its result follows as the second part of the same reading session. Saga completion keeps its four-item reward screen. Finishing the results returns to the hero menu.

Levels 50, 100 and 150 each have an original illustrated celebration, composed with the equipped hero and PET. Continuing marks the milestone seen only after a successful save. Back returns to the hero menu without acknowledging it, so it can be shown again later. Ending Back also remains usable when history or saving is unavailable. Pending milestones survive closing the app; profiles older than QBAL 3 consider previously reached milestones seen to avoid retroactive notifications. The on-card profile remains 8192 bytes; QBAL 3 uses reserved byte 7116 for the three milestone bits. The existing unsupported-revision guard also protects revision 3 saves from older builds.

The required /apps/reading-adventure-v2/celebrations.bin resource has six CNART003 tiles (three scenes), totalling 115320 bytes on SD. tools_local/quest/build_milestone_art.py encodes the selected masters from assets_local/quest/milestones; the complete app resource builder includes it. It reuses SdBitmapPack row rendering, without an additional framebuffer or retained pixel cache. On S3 this pack allocates 96 bytes of index plus 4096 bytes of read-ahead in PSRAM; without that memory it falls back to its fixed read buffer. The activity owns and closes the pack. A missing or incompatible celebration pack shows the resource-installation notice, like the other five packs.

After the campaign ending is acknowledged, Hunts replaces Campaign in the main menu. The lower card opens an active Hunt or recommends an available creature. A Campaign button in Hunts preserves access to Saga reading and the Atlas. After all unique Hunts have been completed, repeat Hunts remain available from the lower card. Available Hunts remains limited to four; Memories remains paginated.

The simulator includes three fixtures one reading minute before these milestones, in both genders and orientations. Hardware checks remain pending: cross a milestone, close before continuing, reopen, confirm it persists, then continue and verify it does not repeat. Also test simultaneous campaign/Hunt progress, a saga ending, a missing celebration pack, and the post-campaign menu in both orientations. No native firmware build or device flash was requested for this change.

## Victory prizes and equipment previews

EXP is awarded when a monster is defeated, once per battle, using its required minutes and the capped equipment bonus. Partial progress and excess reading minutes do not award EXP. Reading history still records the full session. Ordinary path objects are unlocked for purchase in the shop; saga sets and Hunt loot remain direct rewards. Existing EXP and owned items are preserved.

Battle reports display minutes without a progress bar and give unlocked objects a larger card. Equipment detail draws the before/after time arrow and uses a compact owned-item status. Buying and equipping or using an owned item returns to the hero menu. A newly won saga or Hunt outfit has an optional try-out screen; wearing or storing it is saved transactionally. Up to two offers persist across app restarts in QBAL 4. Back defers an offer.

Settings offers Start a new game. Its confirmation resets the game profile only after explicit acceptance; book reading history stays intact. Atlas thumbnails paint their opaque mask before the monster ink. Hero naming reuses the shared rounded firmware keyboard. This work is verified in the simulator; native builds and physical display checks remain pending.

The home status strip shows Level, Coins, Streak and Beasts. Beasts totals completed campaign paths and collected Hunt victories, matching the Atlas and Hunt memories. Each completed campaign path counts once; a Hunt family counts once even if an imported save has duplicate legacy claims. A new game starts at zero. Finished books remain in Reading data. The Hunt entry in Missions reserves enough room for its full availability or active-creature message in both orientations.

Combat and reading-return status cards keep the two minute/status lines together
and use the remaining portrait space for the battle illustration. Re-selecting
the active tab, category or inventory filter, or paging past an endpoint, does
not request another paint. Combat phases still paint on their existing clock;
there is no idle animation timer. The simulator's “Comprobar redibujados” control
reads a browser-only atomic paint counter on demand; it does not request a
refresh, write a save or run a polling timer. This counts Adventure render calls,
not physical E-Ink waveforms. Validate actual refresh behaviour on the device.

### Simulator language testing

The QA sidebar has an **Idioma del lector** selector with all 39 catalogues: 38 human languages and the intentional Ook joke language. The sidebar remains Spanish; the simulated firmware uses the selected language. Changing the language reloads the selected scenario and resets only its disposable virtual card. The language stays in the URL when loading another encounter or panel.

The fixture exporter compiles the real microSD packs, copies the regional interface fonts and matching keyboard resources, and lists them in the catalogue. The browser downloads only the selected locale’s resources before boot. English and Spanish use built-in strings; other languages use the firmware’s normal language-pack loader. A missing resource stops the test with a visible error instead of silently displaying another language. No device setting or real save is changed.

The Aventura Lectora audit corpus contains 1,112 keys in each human language. Previously missing translations are assisted translations with reviewed terminology, names and format arguments; this is not a certification by native speakers. Check both orientations, long equipment names, hunt rewards and the return after reading in the selected language. Install the updated language fonts and packs together on hardware.

Short status pills center text within their full height so accents and Arabic/Thai glyphs are not clipped by a second vertical inset. Pill bounds, hit targets and activity ownership are unchanged. The nine SD interface font files cover the characters required by these translations; only their existing metadata and glyph-cache paths are used.


Unclaimed Hunts show a bordered Rewards row with a trophy and disclosure arrow. Opening it shows EXP/coins and separate illustrated item cards, with Back returning to the originating screen. Inspecting prizes never collects them. Already claimed Hunts, including victory history and reading reports after collection, show a checked Rewards obtained row without an activation target. Encounter backgrounds fill their panel using a centered crop; creature art retains its aspect ratio. The hero backdrop reaches the viewable width below the separate name and rank band, and the lower encounter summary uses a compact row.

PET and Reading Adventure use the shared 72-pixel visible menu header height. Jersey25 headers use the existing Unicode uppercase conversion; natural user names and body text remain unchanged. Other typefaces preserve their normal title case.

The QBAL 5 repeat counters occupy reserved bytes 7124–7659 of the unchanged 8192-byte record. See [progression balance](quest-balance.md) for migration, resource cost, replay rewards and device checks.

On repeat Hunts, unique item rewards show Rewards obtained with a check and no selection target. Outfit, PET, backdrop and other objects are awarded only on the first victory. Each later completed attempt still pays its EXP and coins. Reading and Rewards use the larger existing prose control face and fit long localized labels without changing the row height.

## Completed categories, Saga artwork and final battle

An empty completed Shop category shows a grouped monochrome treasure-chest
illustration, its translated completion message and a My gear button. That
button preserves the selected category. Categories with future level or path
unlocks prompt the player to continue. The supplied imagegen illustration is packed
by `tools_local/quest/build_shop_complete.py` into 2,048 constexpr bytes: no
runtime PNG decode, SD resource installation, allocation or worker is added.
Its source and prompt are in `assets_local/quest/ui/`.

Saga ribbon numerals use the smaller interface cut and the full ribbon text
area, including VIII. Reading a Saga reuses the corresponding Path scenes from
the existing story pack. Successive text pages select scenes across that Saga's
ten paths. Text pagination, stored bookmarks and image quality are unchanged;
existing resource checks and fallback behavior remain in place.

The King of Oblivion, the last campaign battle, requires exactly twice the
penultimate battle's effective reading target with the same hero and equipment.
The base requirements are 159 and 318 minutes. Scaling and rounding happen
before doubling the final target. Victory EXP follows the existing required-
minutes formula. In-progress reading seconds remain saved, while a completed
victory/story keeps its recorded duration; old victories are not reopened and
earned EXP is not awarded again. Save format and Hunt targets are unchanged.

The portrait Hunt reading-return illustration fills the height remaining after
its progress, rewards, optional gains and menu action. It has no fixed 270-pixel
ceiling; landscape keeps its existing full-height side illustration. Existing
scene framing preserves aspect ratio, and no additional image buffer is added.

On hardware, check all four completed Shop categories and their Equipment
button, owned/unowned details in both orientations, outfit power versus its
detail, Level statistics versus Equipment, Saga VIII and several illustrated
Saga pages. Resume a partially read final battle and confirm that elapsed
reading survives. Check the final goal against the penultimate goal with the
same equipment. Test PET's Album and Play screens as described in its guide.
