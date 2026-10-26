# PET

PET appears in **Apps**, beside Reading Adventure. Choose a Forest, Lunar or Volcano Egg,
set a daily reading goal (5, 10, 20 or 30 minutes), and press Begin. A PET-specific
name screen reuses Wordle’s letter keys, with the selected egg, a name preview,
a 0–12 counter, Space and Confirm name. Names allow uppercase A–Z, Ñ on the
Spanish keyboard, and single spaces, up to 12 characters including spaces.
Blank names are rejected and trailing spaces are trimmed. Back returns to egg
selection without creating a companion; confirmation writes the new life once.
The name appears on its dashboard, room and archived life. Older unnamed saves
remain readable and use their egg label until a new named companion is created.
The interface uses the firmware's pixel fonts, headers and existing icons.
The supplied monochrome artwork is loaded from the microSD packs.

During incubation, vital stats show locks and the Care tab opens an explanation
instead of care controls. Feeding, affection, rest, games, hygiene and treatment
unlock together at hatching (20 minutes of reading). Vital stats do not decay
before hatching; the care clock starts when the new stage is reconciled. Reading,
goals and the album remain available. Home's four slots, next project and project
chooser show locks until hatching. Inherited furniture remains visible; passive
project reading is preserved. The Care tab title stays unadorned so its label fits.
The dashboard prioritizes
minutes remaining until birth before switching to its furnishing project. Difficulty still controls
the absence grace period. A growth window shows the named companion, previous
and new stages, and the care unlock at birth. Continue or Back opens the reading reward after the growth announcement. Only
acknowledging that reward advances the noticed growth/reading counters; a failed
save keeps them pending for retry.
Both stages in the growth window use white cards and the original black artwork,
including the newly reached stage. A thick pixel arrow links them; hatching uses
a double-edged pixel rosette with a white check and surrounding rays above the
care-unlock explanation. These are display-only
changes; rewards and growth use the acknowledgement sequence above.

## Reading and daily visits

PET reads the existing reading journal when opened. Earlier reading is not
imported into a new life. Recorded sessions on the same local day add up. A session crossing midnight is split by its recorded
local time. The three-date replay cache preserves allowances when changing
between the supported UTC-12 and UTC+14 timezones revisits a date. Food
is awarded at 5, 10, 15, 30, 60 and 120 minutes (at most six per day); remedies
at 30 and 120 minutes (at most two per day). The early thresholds sustain short
reading visits while longer sessions earn less additional stock. All reading
still advances stages at 20 minutes, 2 hours, 8 hours and 20 hours, and counts
fully toward furnishings and memories. There is no separate timer inside PET.
Continue reading opens the most recent book, or Home when there is none.

Provisions are food, not currency. Feeding spends one for up to 25 satiety.
Existing food/remedies and bonus remedies survive migration and new generations;
they do not expire. The initial remedy remains available. Stock comes from saved
earning counters, not from dividing lifetime reading. Reading before a new pet's
baseline consumes that day's thresholds without paying the new pet, so starting
a new generation or reopening does not reset the day's allowance. Reading days
follow the journal's recorded local date, not the date PET is opened.
Care shows exact food and remedy counts in a passive status panel. Earning rules
stay in Help, available from the header. The Daily goal is a habit marker, with no
separate completion prize or effect on the absence limit. Supplies continue
beyond the goal until the daily thresholds are exhausted.

Health appears first in Care, with the same border and spacing as the other actions. The Care actions use five white full-width rows in portrait, each with a large
monochrome icon and title on the left, with separate benefit lines to the right.
Whitespace separates labels and benefits. Landscape
keeps the five actions in a compact grid beside the status column. A hygiene
card shares the advice row and the vital cards' height. Petting has its
own row. Rewarded games show the actual capped mood gain and the 20-energy
cost separately; practice and care blocks retain their own explanations. Feed
shows the capped satiety change and ration cost; a confirmation window shows
the actual change and one food spent only after a successful save. Focus reverses the entire row with readable
white text and icons. The shared row geometry and a 128-byte static gamepad
icon add no explicit app heap allocation, task, timer or persisted field.

PET aligns the Home project and Daily goal bars. A pending project shows minutes
since the previous collection threshold over the next decoration's interval;
owned decorations show Ready instead of an invented historical cost. Switching
unowned decorations shares the same collection progress. The current increments
are 10, 15, 20, 30, 45, then 60 minutes each. At 20 hours, 23 earned objects
leave 0/60 minutes toward the twenty-fourth. The first decoration becomes
available when Home opens at hatching (20 minutes); incubation credit is kept.
Existing objects and reading credit survive the shorter curve without a save
format change. If prior reading already covers the next threshold, choosing an
unowned decoration unlocks it immediately; only that chosen item is awarded.
Older or externally seeded collections below their reading threshold show
cumulative collection credit over the full target, keeping remaining time
accurate without revoking objects.

The four tabs have distinct purposes:

- **PET:** companion, mood, four vital stats (satiety, mood, energy and hygiene), next project, today's reading and Continue reading.
- **Care** (Spanish: **Cuidar**): provisions, stats, health, feeding, petting,
  rest and minigames. No repeated companion profile.
- **Home:** a room with the appropriate awake, resting or sick pose; four
  equipped object slots; the project and project chooser.
- **Album:** reading days, best streak, accumulated reading with this pet,
  life stages, illustrated memories and previous generations.

The PET portrait contains the species landscape, companion and its mood indicator. After hatching,
the four aligned vital-stat cards replace the former growth strip. The portrait
grows with its life stage, with the mood indicator beside the fitted companion.
Portrait layout places the stage and status above a centered companion when
there is room; compact layouts retain space for actionable care advice.
Active stages share fixed card heights, with vertically centered artwork that
scales gradually by age.
Vital labels use regular mixed-case text while their values stay emphasized;
static cards have lighter outlines. Album sections use spacing instead of outer
frames, and result windows keep one main outline rather than nested frames.
The stronger Home project and Daily goal outlines remain deliberate emphasis.

Short companion reactions depend on the recorded stage, today's reading and
petting. Care needs always take priority. Unknown dates and pending reading
history never produce a daily-goal reaction. Feeding, cleaning, healing and game
results use warmer descriptions alongside their actual saved numerical changes.
These are static translated messages selected from existing state; there is no
new timer, animation, stored event, reward or save-format change.
The Home project and Daily goal have thicker outlines and aligned progress bars;
a completed collection uses a larger trophy. Album's Life stages
section shows reading minutes remaining beneath every locked stage, rounded up to
the next minute. Furnishing progress remains a separate goal.
Today's reading uses a compact total, daily-goal caption and progress bar. Furnishings
belong to Home. The exclamation icon immediately left of the settings gear opens
**How reading helps**. The same help remains accessible from the Daily goal card. Album's accumulated-time card explains its separate current-life total.
The Home reading-rule band is removed; an urgent deadline remains visible on PET. Relaxed mode explicitly explains
that absence cannot cause death; other modes show the remaining grace period.
Recorded reading before the deadline renews that period, independently of the daily goal.

The Daily goal card caps its progress at the configured target and shows all reading today separately.
The goal is a habit marker. Provisions, remedies, growth and furnishing progress
come from recorded reading. Special encounters, their galleries and collection
bonuses have been removed. Previously earned remedy credit remains valid; no
new encounter bonus can be collected.

The Home catalogue separates Owned from To earn, with a count in each filter.
Tapping an object opens Preview. Confirm Place for an owned object or Choose
project for an unowned object; Back leaves it unchanged. Filters and category/page selection are transient and do not write the
profile. Room category controls open Owned; Decorate home opens To earn while
projects remain, and Owned once all furnishings are collected.
Empty categories explain how to continue using the other filter.

Album presents one reading summary, all five life stages, the supplied pixel-art
book stack and two navigation rows. The summary groups accumulated reading,
reading days and best streak; selecting it opens the accumulated-time explanation.
The black View memories button opens the existing gallery and sits directly above
Previous generations. Individual memories retain their scenes, dates and reading
details in that gallery. The five enlarged life stages appear near the top of
Album under Life stages, linked with shafted arrows;
only the current label is bold. Future stages show a lock and the remaining
reading minutes beneath it. Previous generations opens
the existing archive even when empty; its empty-state explanation remains there.
These presentation views are temporary and do not change the save format.
Accumulated time is current-life reading,
not today's time or inherited reading. Its help explains that previous generations are
separate. The Days/week card is removed.

There are 23 recorded memory types: the original eleven events; reading milestones
at 1, 5, 10, 25, 50 and 100 hours; 5, 25 and 100 completed games; and 1, 5 and 20
remedy treatments. Completed practice games count; abandoned or care-blocked games
do not. Cleaning is separate from treatment. Counters reset for each new life.
Legacy saves start the new counters at zero; earlier wins and treatments cannot
be reconstructed. Reading milestones are captured on a new threshold crossing,
without inventing dates for past milestones.

New moments record the local date, cumulative current-life seconds, life stage,
pose and all four furnishings. A validated journal record captures reading
milestones before later records or care can change their scene. Cards start with
date and generation, then a shorter picture, event title and accumulated minutes
at that moment. Newest recorded memories lead the overview and gallery. The gallery
contains only discovered memories. Old snapshots keep their
original scene; missing date/time values are explicitly unavailable. Only the
arrival date can be recovered from the existing bornDay. Already reached reading
milestones without a snapshot say Not recorded rather than promising a future
unlock; their historical date, time and scene are not fabricated. Ended lives explain
missed memories rather than promising future unlocks.

Decorations are painted behind the full-body companion with its opaque mask.
Changing equipment does not change a recorded memory. New generations start their
own journal; Previous generations still preserves summaries rather than past galleries.

Album, memories, life details and reading help all have explicit return controls.

## Care and health

Care unlocks after the egg hatches. A ration costs one provision and restores
25 satiety. One feeding card shows the shared ration cost and effect; the former three food
choices had identical effects and no longer occupy separate cards. A care hint
uses the existing need priority and links treatment or cleaning advice to Health.
Health, affection, rest and games have separate cards. Health uses a medicine
bottle; the cleaning action within Health uses a soap icon.
These recommendations do not change care thresholds, costs or rewards.
Petting grants up to ten mood once per date, or fifteen when mood is below forty.
The framed affection window shows the matching species and age, a reaction to
the need before petting, and the actual before/after values. Repeated visits
and an already-full mood show zero gain. Affection does not cure illness, fill
satiety or interrupt rest. This result is ephemeral; the existing daily save
field prevents repeat rewards. No extra pack, task, animation clock or heap
allocation is added for this window. Satiety falls two points per elapsed
hour and mood one; in Relaxed mode neither falls below twenty. Energy recovers
five points per hour (up to twenty hours per reconciliation). A two-hour rest
adds thirty energy and wakes the companion automatically.

Hygiene falls one point per elapsed hour after hatching. It stops at twenty
in Relaxed mode. At twenty or less, a healthy companion develops a cold, or
malaise if satiety is below twenty-five. Cleaning is free, restores hygiene to
100 and adds five mood; it does not cure an existing illness. Healing consumes
one remedy, removes the illness and restores hygiene to at least sixty.
Every profile begins with one remedy. Further remedies come from reading. Sick companions can still be fed, cleaned and rested;
minigames remain unavailable until they recover. Cleaning and healing now open
a framed illustrated result using the companion species and current age, the
actual message, hygiene value and a Continue button. These results add no
background timer or persistent result state.

Pairs, Sequence and Sliding puzzle use turns, suitable for E-Ink. Pairs displays six
double-framed cards, matched-pair and error counters, and keeps a mismatch
visible until Continue. Sequence has three rounds of five, six and seven symbols;
Ready to repeat hides the series, and a mistake returns to memorizing the same
round. Sliding puzzle has eight numbered tiles and one empty cell in a three-by-three grid.
Only a tile next to the gap can move. Order 1–8 with the gap at the bottom right to finish.
Every initial board has even inversion parity and a Manhattan distance of at least twelve,
so it is solvable and does not begin nearly finished. Generation is bounded to sixteen attempts,
with a known solvable fallback. Restart restores the same initial board. There is no countdown
or automatic replay timer. The board and restart copy are fixed nine-byte arrays in GameState;
no heap allocation or persistent format change is introduced.

The header Back arrow and the explicit Menu button return from every game to
the minigame menu. Menu Back returns to Care. Exiting an unfinished game discards
its transient board without spending energy or granting a reward. Settings can
be opened from games and returns to the same board. Game state is bounded inline
activity data; no game board is saved to the microSD or added to the profile.
The menu shows three side-by-side difficulty cards, with the selected game
reversed to white on black. The same card, sequence and grid motifs identify
each game in its play header. Mood and energy stay separate from the daily prize
counter. The sliding puzzle has a framed board, raised numbered tiles and a
dashed empty square; Restart restores the initial board even before the first move.

The first three
eligible completed games each day grant ten mood for Pairs (easy), fifteen for Sequence
(medium), or twenty for Sliding puzzle (hard). Mood below forty adds five to each base reward;
all gains stop at one hundred mood. Each rewarded game costs twenty energy. The menu shows
each game's difficulty, actual capped gain, and cost. Care shows the available gain range.
A hatched, active, awake, healthy companion with at
least thirty satiety and twenty energy can play. The game screen explains
which need to attend to when play is unavailable. Further
games cost nothing and grant nothing. Abandoning a game awards nothing.
Reading remains the source of provisions, remedies, growth and furnishings.

## Home and lives

Choose an unowned object as the next reading project, or equip an owned one.
The supplied catalog contains 240 objects, sixty in each of four slots: rest,
decor, toy and environment. The chooser has category buttons and discrete pages;
the Home slot shows the actual equipped artwork. The chooser uses a compact title
and an explicit **Back to Home** button beside its paginator. Reading counts
for growth and projects together; choosing another project does not spend or
lose progress. Objects unlock at increasing lifetime reading thresholds.

After all 240 objects are owned, the next-objective card in PET and Home becomes
a trophy button marked **Home complete!**, with a 240 / 240 count and **View trophy**.
It opens a congratulatory window explaining that the collection is saved and
inherited by future generations. Continue and header Back return to the view that
opened it. Completion is derived from the existing ownership bitmap, with no new
save flag, automatic popup, extra reward or generation reset. Inherited eggs keep
Home controls locked until hatching.
The window follows the supplied celebration reference: a large star trophy with
laurels and sparkles, a ribbon title, prominent count and corner decorations.
The approved artwork and prompt live in `assets_local/pet/collection/`.
`tools_local/pet/build_collection.py` converts it to the required
`/apps/pet/collection.bin` SD pack (one 280 x 210 BW1 record; 7,390 bytes).
The activity validates it on entry, then opens it on the complete-collection window and on completed PET/Home
panels after hatching. It reuses the cache between these views and closes it on
other views or activity exit. The existing fixed pack scratch is 1,059 bytes plus handle/metadata;
the bounded pack cache may use 4,112 PSRAM bytes;
allocation failure uses its small-read fallback. A missing or invalid pack
blocks entry with the resource-installation notice. The illustration is not compiled into firmware. Labels,
ribbon, count and Continue remain native controls/text and adapt to landscape.

Relaxed mode has no absence death. Normal allows seven days since the latest
recorded reading, Hard three days, Extreme twenty-four hours. Elapsed time is
reconciled on entry and once per minute while PET is open; PET has no background
task. Migration from the older save starts the absence clock on first entry,
so installing this version cannot retroactively end an old companion's life.
Changing difficulty takes effect immediately using the same last-reading date.

An adult can complete its life from Previous generations, with an explicit confirmation.
An absence death also closes that life. The album keeps the last twelve completed
lives, including species, reading and reading-day totals. Starting another egg
preserves objects, provisions, remedies and project reading; the new
companion has fresh growth, care and reading-day totals. If history cannot be
saved, PET blocks replacement until the microSD is available.

The dashboard, Home and archived lives display the saved generation as **Gen 1**,
**Gen 2**, **Gen 3**, and so on. Starting a new companion increments that same
counter; existing names, difficulty rules and inheritance are unchanged.

A newly recorded absence death opens a Farewell window naming the companion
and generation. **Continue** acknowledges it and returns to PET; **Generations**
acknowledges it and opens Previous generations. Acknowledgement is saved before leaving
the window, so a failed write keeps it pending for retry. Back leaves PET without
acknowledging; the window returns on the next successful entry. Previously lost
companions also receive the window once. Retirement does not trigger it.
Dead companions use a species-specific ghost illustration in PET and Home,
without sleep animation or Z marks. Only their companion card (PET) or room card
(Home) uses inverted colors; tabs, header, vital stats and controls retain their
normal palette. The ghost's opaque silhouette and ink retain their normal colors.
A scoped draw-target adapter restores the palette after each card; it requires
no second framebuffer. Missing or damaged farewell art displays a
simple grave marker instead. Starting the next generation remains an explicit
choice; the farewell window never replaces the companion automatically.

## Storage and resources

Two alternating checksummed 640-byte PET7 saves live under `/.crosspoint/pet/`.
The loader accepts the previous 256-byte PET1–PET5 and 640-byte PET6 records and converts them on
the next successful write, preserving progress, names, equipment, resources and
available snapshots. A failed conversion write retains the other valid slot.
Older firmware treats PET7 as unsupported/read-only. Unknown future versions remain
read-only instead of overwriting them.

The shared core through byte 204 retains its offsets. Byte 205 is memory version
2; bytes 206–209 hold completed games and 210–213 hold current-life treatments.
Bytes 214–217 are the 23-bit availability mask. From byte 218, each of 23 records
uses 16 bytes: four-byte scene, four-byte local day, eight-byte accumulated seconds.
Bytes 586–617 hold four eight-byte unsigned counters: total earned food, total
earned remedies (excluding the initial/bonus remedies), acknowledged food and
acknowledged remedies. Old versions initialize them from their previous lifetime
formula, preserving exact stock. New gains are computed once from validated
journal checkpoints and the day's cumulative recorded seconds; a failed scan or
save never publishes draft earnings. Retrying assigns the same counters instead
of adding them again. Acknowledging a reading reward snapshots the earning
counters, so its result shows actual daily gains rather than the old flat rate.
The profile adds 32 fixed bytes in RAM; each timeline adds 56 fixed bytes. Both
remain activity members, with no new allocation, task, timer, or framebuffer.
The final checksum covers all preceding bytes. Legacy PET5's eleven snapshots
are decoded from their original 205–251 extension; unavailable dates and times
remain unknown. Scene bits retain four six-bit furnishing indices, three stage
bits and three pose bits; empty furnishing slots use 63.

Two alternating 640-byte PTL2
files store life history, preserving names; older 512-byte PTL1 history migrates
without discarding its records. Writes are read back before becoming active. An older
valid slot recovers an interrupted write; unsupported newer formats are preserved
read-only. Archive writes are idempotent per life number, including retries after
an interrupted profile save. A damaged or reset reading journal preserves the
companion and asks the user to restore the journal before awarding progress.

Copy `resources/apps/pet/` to `/apps/pet/` on the microSD. The original
`graphics.bin` has 64 stable slots: 63 creature/object illustrations and the supplied hand. Required `home.bin` has 39
state-pose slots followed by six landscape halves; it also accepts the earlier
39-record pack, which uses the original backgrounds. A missing or invalid pack
shows the resource-installation notice.
The supplied pixel collection fills all 102 slots (87 distinct illustrations plus
15 exact aliases). Source PNGs remain in `assets_local/pet/png/`; rebuilding
with `tools_local/pet/build_resources.py` uses those files by default. The X4 Pro
browser simulator can use the same packs in its virtual SD card.
See [resource instructions](../../assets_local/pet/README.md).
Artwork is not embedded in firmware flash.

`habitat.bin` adds the supplied 2:1 room, all 240 accessories and opaque silhouettes
for the 39 Home poses. Its 560 CNART002 records contain two background tiles and
279 mask/ink pairs. Exterior white is transparent; interior white covers the
floor and background. Explicit cutouts preserve open hoops and hammock frames.
Wall, ceiling and floor objects use separate anchors. Pet ink bounds determine
its baseline and size by age; illness takes priority over rest. A healthy sleeping
pet rests at a support point measured within its furniture, with the front rim
painted over it. The scene preserves its 2:1 aspect ratio in either screen shape.
The generated layout metadata and PNG derivatives come from
`tools_local/pet/build_habitat.py`; the supplied originals remain unchanged.

The activity owns its configured pack handles and releases them on exit. The Home pack
adds a bounded 4,816-byte optional PSRAM index/read cache (4,720 for the legacy
39-record pack); the pack's fixed scratch
buffers and bounded life records are activity members. The habitat pack adds
13,056 bytes of optional PSRAM cache and one set of fixed pack scratch buffers.
The required `/apps/pet/breathing.bin` holds 216 registered frames covering all
three species, four hatched ages and six poses: normal, happy, hungry, tired,
sick and resting. Twelve static head crops and nine egg wobble frames complete the pack (474
CNART002 records). Its bounded index/read-ahead cache uses 11,680 bytes of PSRAM, with
the existing fixed small-read fallback on allocation failure. Original atlases,
prompts and provenance are in `assets_local/pet/animation/`; regenerate with
`tools_local/pet/build_breathing.py`. The activity owns and closes the pack.

Breathing uses phases 0/1/2/1, with 0.75 seconds between steps, while an active,
hatched pet is visible on PET, Home, Reward or Evolution. PET and Home share
one pose selection for happy, hungry, tired, sick and sleeping companions.
PET always shows the complete body; Home alone may crop to a shelter head.
Both sick conditions use the subdued sick pose in both views.
Eggs rock between left, upright and right orientations about their lower support.
Ended lives, menus and historical pictures remain static. No animation runs
while reading. A missing or invalid required pack blocks entry with the installation notice. Procedural
rising Z marks use the same clock for a healthy sleeping pet in PET, Home or Reward; they
are native UI elements, independent of the packed animation frames. Their black strokes have a white outline
so background details do not hide them. Live mood badges are drawn as monochrome
faces (happy, sad, hungry, tired or sick) above the pet on PET, Home and Reward.
They alternate two centered sizes on the same clock and are native UI elements
independent of the packed animation frames. Sleeping healthy pets use Z marks instead. Shelter heads remain still because the
breathing torso is hidden inside the furniture.

Beds and hammocks fit the entire sleeping body without limiting it to mattress
thickness. Twenty-three enclosed shelters use a head-only view sized to their
entrance, described in `assets_local/pet/hogar/rest-poses.json`. The torso stays
inside; the visible head is still while Z marks indicate sleep. A clipped crop
of the original pose remains available when the optional head artwork is absent.
One pending tap is retained through blocking refreshes, tagged with the last
painted view revision and discarded on screen transitions, so animation does
not repeatedly lose touch releases or replay a tap on a different screen.
There is no extra display
framebuffer, timer task or per-frame dynamic collection. State saves occur only
on changes; elapsed care is checked once a minute and saved at hourly/state
boundaries, plus explicit care/settings/reward actions.

The required `/apps/pet/farewell.bin` contains three ghost illustrations, each
with an opaque silhouette and ink layer (six CNART002 records). Source PNGs and
provenance live in `assets_local/pet/farewell/`; Silva uses the supplied final
halo-and-wisp illustration. Rebuild using `tools_local/pet/build_farewell.py`. After validation on entry, the activity
reopens this pack only for a lost companion and closes it when a new life starts
or PET exits. It adds one fixed pack member (1,059 scratch bytes plus handle and
metadata) and at most 4,192 bytes of optional PSRAM index/read-ahead cache. Cache
allocation failure uses the pack's fixed small-read fallback. There is no extra
framebuffer or per-frame allocation. The companion's identity, halo and wisp are
scaled together to fit their destination, preserving aspect ratio.

## Controls and verification

On X4 Pro, tap controls to activate them. Both short and held side-button presses
change the main tab; they never move focus between controls. Back returns to a subpage's parent.
In the sliding puzzle, tap an adjacent tile or swipe it toward the empty square.
One gesture moves one tile; nonadjacent and stale gestures do nothing. Swipes
follow the direction with the larger movement, as elsewhere in the interface.
The puzzle owns edge swipes while open; use Menu or Back to leave it.
Landscape rearranges the interface without scrolling. Health lives under Care,
and games do not replace Home in the tab bar.

Host checks cover growth/stock/project rules, hygiene, illness, healing, daily
reward idempotence, absence boundaries, legacy migration, interrupted writes,
archive retries/capacity, future formats, resource checksums and English/Spanish
rendering at 480 x 800 and 800 x 480 using the actual firmware fonts.
This update is verified in the X4 Pro browser simulator. No device firmware is
built or flashed for this simulator-only delivery.
Host results do not establish physical panel timing, touch behavior or SD durability.
The local PET test page offers a genuine fresh start and independent day 1, 7 and
14 scenarios at 15, 30 or 60 minutes of daily reading. They use production reading,
care, furnishing and serialization rules, with no inherited credit or stock.
The scenario assumes one care/catalogue visit per reading day: feed when satiety
is at most 75, clean below 80, heal illness and pet once. Games are optional and
not included in this resource comparison. At 15 minutes/day, day 7 has 11 food,
1 remedy and 4 decorations; day 14 has 18 food, 1 remedy and 6 decorations.
At 30 minutes/day, day 14 has 30 food and 15 remedies; at 60 it has 44 and 15.
Free cleaning can prevent illness, so remedies may remain unused. These are
simulated days, not evidence of several days of real use or proof of long-term
motivation. Run `tools_local/pet/build_progress_fixture.py` to regenerate the
independent presets and recorded CSV. Boundary checks also cover the two-hour
cap, whole sessions spanning midnight, checkpoint cuts, timezone date revisits,
partial visits, repeat scans, generations and PET1–7 migration.
On hardware, check all four orientations; open/leave PET repeatedly; feed, clean,
heal and rest; reopen after reading; verify rewards are awarded once. Remove the
art packs to confirm the installation notice; restore the complete folder,
re-enter PET and check equipped objects.
Watch for clipped controls, resets, repeated rewards or SD errors in the log.
Using a disposable test card/save, let an absence deadline expire: verify the
named Farewell window, both buttons, reopening after acknowledgement, and Back
before acknowledgement. Interrupt a save to check that the window remains
pending. Start a new companion and verify Gen 2 (then Gen 3) with inherited
objects and stock, fresh growth and no ghost or sleep marks on the active egg.
Repeat the farewell checks with its art pack absent and inspect all orientations.
Use a test save rather than changing the clock on a card with real reading data.

The setup screen separates the selected companion's traits from a settings card
with a gear and difficulty/goal values. Settings presents difficulty, daily goal
and an independent guidance panel. The active PET overview uses the freed shortcut
area for paired next-objective and daily-reading cards, keeping the daily benefit
visible alongside the minutes still needed.
The [artwork pipeline](pet-artwork.md) and [original filename list](pet-artwork-list.csv)
describe the supported assets, reuse and final conversion requirements.

Names use fixed 25-byte UTF-8 arrays in the profile, history and input draft. The
shared Wordle keyboard uses the existing interaction table and coordinate focus
grids, including mapped-button confirmation. No generic keyboard activity, extra
framebuffer, task or dynamic text collection is added.

The games menu distinguishes unavailable play, rewarded play and free practice.
It shows rewards already used today out of three when the date and save are
usable, or an unknown-reward marker otherwise. A health/energy block is displayed
as unavailable rather than as zero-cost practice; its cause stays visible. The
same existing game guards and reward rules apply.

### Communication and accessory previews (browser simulator)

The reading reward separates time since the last acknowledged visit, actual food/remedy gains, Growth and the Daily goal. Gains use persisted earning and acknowledgement counters, excluding the initial remedy and previously earned bonus credit. Acknowledging the reward advances both reading and earning counters. Adults show Growth complete; all reading still counts for Home projects, while supplies follow the daily diminishing thresholds.

PET links a current need to Health or Care. Feeding previews the capped result (90 to 100 still costs one food) and shows actual before/after values in a confirmation window only after a successful save. This receipt is temporary activity state and clears when leaving its notice.

Home catalogue cards open a static preview: an enlarged object and the room with its proposed placement. Owned items use Place; unowned items use Choose project. Opening or leaving does not save. Only confirmation uses the existing guarded save. Back retains category, filter and page. Read-only profiles may preview, but cannot apply. A reusable fixed-size Profile copy lives in the activity Work workspace; no framebuffer, heap buffer, task, timer, persistent field or asset pack is added. Room preview does not animate.

Preview arrows browse the current category and Owned/Unowned filter without saving. The counter identifies the current item; arrows stop at the first and last item. Back opens the catalogue page containing the item just previewed. Read-only browsing remains available.

Confirming Place or Choose project keeps Preview open after the guarded save. An equipped object shows its saved status and offers Remove; a pending project's confirmation is disabled. An immediately unlocked project is already placed by the existing project rule; its preview switches to Owned before locating its page. A pending project stays in To earn. Back returns to the catalogue page containing that item. In Owned, the equipped object appears first; all remaining objects keep their canonical order, so replacing it restores the previous object to its normal position. This order is derived and is not saved.

At full satiety, Feed stays disabled and explains that the companion is already full, even with no provisions left. Wake warns that ending early forfeits the cycle bonus; current energy remains in its separate metric. Rest shows the two-hour cycle and an upper bound on its energy bonus before starting. Successful cleaning shows actual hygiene and capped mood changes and is free. Successful healing shows the hygiene change and the one remedy spent. Results are temporary activity state, created only after a successful save and cleared on leaving their notice; save errors never show a successful result. Remedy rewards and healing use a medicine bottle icon, while hearts continue to mean mood and affection. These refinements add six bytes of result data to each UI model/activity instance and a fixed 128-byte monochrome icon, with no new save fields, heap buffers, timers or SD packs.

This delivery updates the browser simulator and gallery only; native firmware and device refresh/consumption remain unverified.

### Care guidance and readable progress

Health actions explain their current availability: already clean, no treatment needed, no remedies, saving unavailable, or their normal free/one-remedy cost. Existing care and save guards remain in force. Care shows the rest cycle's remaining time when its clock is usable, an unknown-time message otherwise, and full energy at 100. The existing once-per-minute care check refreshes Care while resting; it creates no new timer or task. The two-hour rest cycle and its energy rules are unchanged, and waking still ends it early.

The Daily goal card separates capped goal progress (for example 10/10 minutes) from the full reading time today (for example 25 minutes). Accessory previews show remaining reading as hours/minutes, rounding incomplete minutes up; zero remaining time is Ready to collect, with confirmation still required. The Games tile in Care uses the same derived offer as the game menu: actual capped mood gain and 20-energy cost for rewarded play, free practice, or a short reason play is blocked. These are presentation changes, with no additional saved fields, asset packs or heap allocations.

### Contextual health and consistent daily labels

The reward now applies the Daily goal card's date/history/save availability guard. It caps pending goal progress and replaces reached fractions with Daily goal reached; Read today shows the full nonnegative daily duration separately. Unavailable data never shows an invented daily result. Album distinguishes Reading days from Accumulated reading time for the current pet.

Health shows the existing full-body pet illustration, using the current species, age and pose, beside a short current-state recommendation: no care needed, free cleaning, urgent cleaning, remedy use, reading to earn a missing remedy, or unavailable saving. The illustration remains static on Health. Health help opens three rule cards for cleaning, healing and the two-hour rest cycle. Header Back and Back to Health both return to Health; its normal Back returns to Care. The help page is a transient View with no saved field, new allocation, activity, timer or SD pack. Existing care/save guards still govern actions.
Clean and Cure have black title bands. Available actions show their cost and
an Available label. An unavailable action explains full hygiene, no treatment
needed, missing remedies, blocked care or unavailable saving, without an active
chevron. Full hygiene is shown as already clean; there is no daily cleaning limit.
Memory illustrations size the companion using its recorded age and the room's
existing stage proportions, so a newborn remains smaller than an adult.
Puzzle gestures add bounded inline input state and share the rendered board's
tile geometry. They add no heap allocation, save field, background timer or asset pack.

Before starting rest, Care shows Cycle: 2 h, including when energy is full. While resting it retains the remaining-time explanation, current/full energy and early-wake behavior. No reward amount, care threshold or rest rule changes. This delivery updates the browser simulator and gallery; native device verification remains pending.


### Consistent absence, rewards and navigation

PET checks new journal intervals in append order against the last recorded reading.
Reading begun at or after an expired absence deadline cannot revive that life.
Reading begun before the deadline extends it through the end of the session, even
while PET is closed. The archive records the actual expired deadline. Invalid,
future or reversed new intervals protect the existing save and display the journal
warning instead of inventing a death. Legacy profiles without a usable reading
anchor retain their migration protection. These checks use a fixed timeline member
and the existing incremental journal scan, without another journal buffer.

Settings previews the deadline under the selected difficulty. If a stricter mode
would already be overdue, Save opens an explicit Apply and end life / Cancel
confirmation. Cancel keeps the draft selection for editing; leaving Settings
cancels it. The existing settings gear is also reachable through mapped-button
focus and confirmation. An urgent deadline appears directly on PET as well as Home.
Relaxed mode continues to prevent death from absence.

Reading grants up to ten mood per completed five-minute interval, measured by
current-life reading, rather than once per visit or reconciliation. Splitting the
same reading into smaller visits cannot multiply this reward. Hourly care saturates
at the same floors for one long absence or successive hourly checks.

Album labels its counter Reading days: at least five minutes recorded since this
life began, independently of the daily goal. Streak uses the same threshold; the statistic cards explain it. The expanded
Accumulated reading time card retains the full current-life reading. Memory and life details return to the overview or list that opened them.
Archived details show the ending reason and locally dated ending when available.
Retirement explicitly ends the current life; it preserves the summary and inherited
resources, not a separate historical memory gallery. The farewell action uses the
compact Generations label. Ended lives replace live vital bars with their reading summary
and Care explains that this life's care is closed.

Finished games identify Collect reward before leaving, or View result for practice.
Only collection applies the existing reward/cost after a successful save. Its
illustrated result shows actual mood and energy before/after, or explains practice
and a newly unavailable care state. Leaving through Menu still discards the round
without reward or cost. The receipt and its return view are temporary activity data.
No new persistent fields, framebuffer, asset pack, timer or heap buffer is added.

Egg progress is labelled Hatching progress. Catalogue focus uses the normal selected
fill, while a solid outer border and Placed / Your project label identify the
saved choice; other cards say View object. Once the adult has all furnishings,
reading guidance points to supplies and remedies instead of unfinished home growth.

Numeric before/after results consistently use a right arrow (70 → 90), including
feeding previews, food receipts, cleaning, healing and games. The shared pixel and
prose cuts include an explicit U+2192 glyph with a visible shaft. Run
`python tools_local/toybox/extend_status_arrows.py` after regenerating those cuts;
it preserves existing glyphs, metrics and kerning and is idempotent. The five
added monochrome glyphs are flash constants, with no runtime buffer or allocation.


Mood faces use a proportional slot beside the upper-right of the fitted companion,
bounded to 36–64 pixels when space permits. The PET anchor follows the current
species, stage and pose dimensions and stays inside the illustration area.
Home uses a 36–56 pixel face in screen coordinates, choosing
among four nearby anchors to reduce overlap with the companion, window and placed
furnishings. The two pulse sizes share a fixed centre. Missing artwork follows the
same single-badge path; it does not add a second face. These are bounded layout
calculations with fixed rectangles, no extra animation frames or refreshes.


### Reference-based panels and memory journal (simulator)

PET adds nearby grass and a cross before Go to Health. The name has no separator beneath it.
Growth, Daily goal and Home project use illustrated bordered cards. Care keeps its
actual capped outcomes, while treatment guidance links to Health and the four
action tiles have clear navigation marks. Home removes the repeated reading-rule
strip, enlarges the Home project panel and labels its main action Decorate home.
Header reading help remains available. Three temporary arrangements were rendered;
the first follows the supplied references and no variant macro remains.

The supplied petting-hand image is preserved in assets_local/pet/ui/ and packed
as graphics record 63. The existing 0–62 IDs retain their artwork and order. The
builder thresholds/crops/scales only this new UI source; original creature PNGs
remain unchanged. The 64-record graphics pack lives on the microSD. Older
63-record packs remain loadable and use the code-native hand fallback.

On the host renderer, Profile is 624 bytes. The four resident Profile copies
add 128 fixed bytes for the provision ledger; the timeline adds 56 fixed bytes,
for 184 retained bytes in this refinement. Relative to the original 256-byte
Profile, the four copies have grown by 1,472 bytes across all PET updates. Its
Store byte workspace adds 384 bytes. No additional allocation, framebuffer, task,
timer or per-frame container is introduced. The optional graphics index grows by
16 PSRAM bytes; the existing cache/read fallback owns its lifetime. Real hardware
memory, display and power measurements remain pending. Only the browser simulator
and gallery are built and delivered in this iteration.


All newly redesigned cards use straight rectangular corners. The compact Home project and Daily goal cards omit heading dividers; the name separator is also removed and progress bars remain.


Completed Home panels reuse the existing illustrated trophy/laurels art. The
same required collection pack is now retained while a completed PET/Home panel
or the collection window is visible, and closed on other views or activity exit.
Its existing bounded cache remains optional with the same small-read fallback.
The Decorate home action opens Owned after collection completion, preserving the
ability to place furnishings without first passing through an empty To earn list.

Success notices reserve the check mark in their heading, separate from the mood
face above the pet. Cleaning/healing symbols use a 44–64-pixel illustration slot;
their receipt symbols use 40 pixels. These changes add no animation or refresh.

Album section headings use the smaller display cut with a top inset. View all
controls use a compact rectangular button and a three-pixel chevron. Only the
current life-stage label uses the bold display face; other stages and memory
captions keep the regular prose face, without a bold fitting fallback. Memory
gallery illustrations have a bounded 2:1 scene frame instead of filling a tall card.
Memory scenes crop only the room background, while the pet and saved decorations
fit within the frame. Clipping is restored before subsequent controls are drawn.
Targets without clipping retain the contained background.
Best streak is the longest run of days with at least five minutes of reading
with the current pet. It survives interruptions, using the existing streak field
and save format. Existing active saves retain their last stored streak and are
reconciled against the available reading-history window; older unrecorded runs
outside that window cannot be reconstructed. Ended lives keep their saved value.
The daily summary covers 730 days, which also bounds recomputed runs; a larger
previously saved maximum is retained. No profile field, explicit app allocation,
art asset, timer or display refresh is added. Multiline text still uses the
existing renderer and its internal allocation behavior.

Care describes Rest as Restores energy at every current energy value, with Cycle: 2 h beneath it; current energy remains in its separate metric. While resting, Wake still explains the lost cycle bonus and the status band keeps its remaining time. The passive recovery and capped completion bonus are unchanged. Health's action row shows only Healthy, Cold or Malaise; its hygiene reading is already in the separate metric above. The eleven Care icons are extracted directly from the supplied reference and packed by `tools_local/pet/build_care_icons.py` into the existing one-bit Icon format. Their 5,632 bitmap bytes plus icon metadata are compile-time flash data; no new heap allocation, SD pack, framebuffer, timer or task is needed. The source screenshot and crop manifest remain in `assets_local/pet/ui/`.

The Album archive is called Previous generations throughout its heading, summary, empty state and reading help. Compact buttons use Generations. Spanish loss messages use fallecer with a Despedida heading; English uses passed away. The absence rule and reconciliation behavior are unchanged.

The active PET dashboard keeps the same card dimensions from Egg through Adult: a 136-pixel vital row and 132-pixel Birth/Home-project and Daily-goal row in portrait. Landscape uses 142 and 106 pixels. Egg metrics show their usual icons and labels with a lock in place of the value; Birth uses the same compact card layout. Ended-life controls retain their separate layout.

These layout, wording and icon refinements retain the existing care and minigame rules. The provision ledger is the only new persistent gameplay state in this update.

The PET dashboard uses the supplied species landscapes: Silva/Bosque gets the
forest, Nox/Lunar the moon and Ember/Volcán the volcanic plain, from Egg through
Adult. Original PNGs and SHA-256 provenance live in `assets_local/pet/backgrounds/`.
The resource builder scales proportionally to 448 pixels wide with nearest-neighbor
sampling, bottom-aligns a 144-pixel canvas, thresholds at 160, and appends two
224 × 144 halves per species to `home.bin` at IDs 39–44. No ink-bounding crop is
applied, so the halves retain a shared horizon and meet without a gap. Existing
pose IDs 0–38 and the 560-record habitat pack stay unchanged.

The activity first opens the 45-record home pack and falls back to the older
39-record pack. One availability flag gates background access; no saved state,
extra pack/handle or framebuffer is added. The existing optional PSRAM index
grows by 96 bytes (six 16-byte records), and the same read-ahead buffer and small-read
fallback remain in use. A failed background draw clears that panorama to white
before the companion is rendered. If both opaque creature sources are unavailable
or fail, a white rectangle behind the fitted fallback pose keeps scenery out of
its body. Only the PET hero uses the landscapes; Home,
memories, care and farewell retain their own compositions. Install the matching
`home.bin` with the new firmware to see the landscapes. Hardware display, SD
latency and memory measurements remain pending.

The album contains 50 memory types: 23 saved snapshots and 27 discoveries.
The last reading milestone is 100 hours in the current life (about 200 days at
30 minutes a day), rather than 1000 hours. Discoveries add 13 reading milestones
from 2 to 90 hours, 4 game milestones from 2 to 50 wins, 6 shared-day milestones
from 2 to 60 days, and 4 best-streak milestones from 3 to 30 days. The saved
100-hour and 100-game snapshots remain the final milestones in those categories.
A shared day still requires five minutes of reading; streak uses the best run,
so a missed day does not remove a memory. All discoveries use the current life's
reading, game, day or streak counters. Inherited objects, provisions, remedies
and reading heritage do not award memories to a new companion.
These are memory milestones, not a reduction of the 240-object home collection.
Reaching 100 hours alone does not unlock the other memory categories.

Existing monotonic counters determine discoveries, including when reopening an
older save or a completed life that is still the current profile. Starting a new
egg resets the life counters; the generations list retains its existing compact
summaries, not a separate memory gallery for each life. The expanded curve is
recalculated from those counters. No new profile fields, save version or writes
are introduced.
The 23 snapshot IDs and their PET7 binary records are unchanged. Discoveries use
a reference illustration, clearly labelled without an invented historical date,
reading total or furnishings. Only discovered items appear in the gallery and
its pagination; there is no grid of 50 locked illustrations. Stage locks remain
in the framed life-stage timeline.

PET places care links beside its status text. During an urgent absence warning,
that warning opens reading and replaces the bottom Continue reading action; the
vital and objective cards retain their dimensions. Care and Health use separate
landscape arrangements. Result windows have a heading, illustration, actual-effect
receipt and Continue control. Generic information has no fabricated receipt, and
cleaning an ill pet retains its sick illustration. Previous-generation cards show
their local end date, or the existing unknown-date label when unavailable.

`tools_local/pet/build_reference_icons.py` converts the supplied UI reference crops
to `PetReferenceIcons.h`: twenty images, 45,088 bitmap bytes in compile-time flash
constants. The medal, lock, empty/completed pedestals, body soap/remedy artwork,
book and target are direct crops. The supplied Silva celebration, feeding, petting and Ember cleanup
illustrations apply only to healthy, awake adults of those species; other ages,
species and conditions retain their matching existing artwork. Crop metadata,
source hashes and original PNGs remain in `assets_local/pet/ui/`. No explicit render
allocation, SD pack, framebuffer, task or timer is added; the existing SDK drawing
and text layout contracts remain in use.

Remove is available in an equipped object's catalogue preview. It clears only that
category's equipped ID, preserving ownership, reading credit, project selection,
rest state, resources and captured memories. A successful guarded save returns to
Home so the missing object is visible immediately; failed saves leave the saved
room unchanged. The object remains in Owned at its canonical position and can be
placed again. The existing empty-slot value 255 is retained in PET7, so no save
format or migration changes are needed.

Empty unlocked categories use the supplied dashed bed, plant, ball and curtain
icons with plus signs. A finished project has no pending decoration to earn, so
Home's project panel uses the supplied dashed plus and category-browsing prompt.
A selected unfinished project keeps its reading progress. Home retains its full
collection trophy while decorated; with all categories empty it shows the empty
project panel. PET's collection trophy remains available. These five new crops
add 2,560 bitmap bytes to the existing reference-art header, without a new SD pack,
explicit app allocation, timer or task.

Album uses the supplied book stack and photo icon directly, adding 5,280 flash
bitmap bytes. Its overview no longer displays a featured memory. In landscape,
the life stages occupy the leading column and the books and navigation rows share
the other column; both buttons retain the same order and actions.
Both navigation labels use uppercase display text with a shared leading edge,
photo/history icons and trailing chevrons. The books are centered at 88% of their
allocated illustration area.

### Required microSD contents

PET requires the complete `/apps/pet/` folder from `CrossNext-Resources.zip`:
`graphics.bin`, `home.bin`, `habitat.bin`, `breathing.bin`, `farewell.bin` and
`collection.bin`. Extract the ZIP to the card root, merge folders, and restart
with the card inserted. If a file is missing, incompatible or unreadable, PET
shows **Resources unavailable** with the installation path and a Back button.
It does not load/reconcile the companion or apply reading/care changes while
blocked. A sprite checksum/read failure during rendering replaces that frame
with the same notice. Re-enter the app after installing a complete compatible
folder. Saves remain under `/.crosspoint/`; never replace it to install artwork.

The four core packs stay open during the activity. Farewell and collection are
validated on entry and closed, then opened only for their existing relevant
views. The existing SD-art loader owns its bounded PSRAM caches and releases
them on exit; it can use fixed small reads when PSRAM allocation fails.

The life-stage timeline fits complete stage names on one line in English and
Spanish, including Newborn and Recién nacido. The current stage keeps its pixel
face emphasis. Care supplies are a read-only status panel without a chevron or
focus/touch action; reading guidance remains available in the header. PET fits
the battery percentage reserve to its label so the help and settings controls
sit farther right, with their existing 44-pixel hit areas kept separate.

Placed, writable accessory previews use a compact information card with a
centered name, followed by a black check-marked Placed / Colocado status band and the
separate Remove action. The status band has no action. Read-only previews keep
their saving-unavailable explanation. Catalogue current-item borders use the
same solid 8-pixel stroke as the selected egg, without an inner outline.

Previous-generation cards show an illustration of the archived species at its
last life stage, alongside its name, generation, end date and reading time.
Portrait cards center the illustration; landscape rows place it beside the name.
These are illustrations from the installed PET artwork, not saved screenshots.

**Settings > System > Clock > Date format** selects Day-Month-Year (the default),
Month-Day-Year or Year-Month-Day. PET uses this preference for memory dates,
previous-generation cards and life details, independently of interface language.
The preference is kept in the firmware settings; existing PET saves retain their
format. Short card dates use a two-digit year; life details use a four-digit year.

PET animation advances every 750 ms while an animated PET screen is visible.
Menus and historical pictures remain static. The shorter interval requests more
frequent display updates; the panel's refresh time determines the visible cadence.
