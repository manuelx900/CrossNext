# PET

PET and Reading Adventure now use [save generation 2](save-generation-v2.md). Older game files remain intact and are not imported automatically.

Room floor seams use at least dark-gray ink when the existing room raster is
drawn. A one-pixel light-gray line can miss every dot in the device's B/W dither
pattern; raising only the floor's light-gray runs keeps the seams visible in
either orientation. This is a rendering correction: existing resource packs,
wall/window ink, the pet, accessories and saved profiles remain compatible.
The header's clock and two trailing buttons share one content inset from the
safe right edge, retaining their spacing and touch targets.

PET uses smooth grayscale illustrations, preserving Silva, Nox and Ember and
their existing life stages. Masters and generation provenance live in
`REDISEÑOS/PET/assets/v2/`; the resource builders select them by default. The
six CNART003 packs retain stable IDs and placement dimensions. The shared SD
loader retains a legacy decoder for other callers, but PET requires CNART003
and the current image counts. Home has 52 records; 45/39-record packs are not
accepted by this app.
The two corrected Silva illustrations omit the stray dot above the mouth; the
adult tired pose also has a repaired, coherent sleepy expression.
The source illustrations use a fine black outside contour while retaining clean gray
fills, with no random white flecks or dithered interior shading.
The room has clear white surfaces, visible plank seams and a thin baseboard;
seven native-resolution tiles preserve its lines without enlarging a thumbnail.

Decoration ink promotes dark-gray packed runs to black in the room, catalogue,
previews and memory furnishings. Light fills and white cutouts retain their tone;
backgrounds and companion artwork use their existing palette. Inverted object
icons retain their white-on-black palette. The local draw-target adapter reuses
the same packed resources, cache, single composition and fast refresh policy;
it adds no allocation or display update.

Ceiling decorations sit below the companion's name and age, using only their
original illustration. Floor furniture keeps its existing
baseline; wall pictures retain their wall anchors. Short landscape walls fit
ceiling art proportionally above the tallest resting furniture, without changing
companion or floor-object sizes. Memory crops keep their captured room placement.

The catalogue contains 223 available accessories. Retired ornament IDs remain
reserved in existing save and pack formats but cannot be selected, awarded or
rendered. Loading an older save removes retired equipped items and replaces a
retired reading project with an available one, preserving its reading credit.
Captured memories omit retired objects. Collection totals use available items.
Memory pictures use the same portrait-room camera in both screen orientations,
cropping it without stretching so the window and floor keep their position.
Environment artwork is enlarged proportionally in the room and memory crops;
ground scenery draws in front of resting furniture. The empty project card uses
a framed add symbol and a centered text group.
Wall and floating environment ornaments use dedicated smooth grayscale masters
without ground shadows or bases. Floor ornaments retain their contact shading.
The catalogue's optional `grayscale_source` selects replacement artwork while
preserving existing sprite IDs, dimensions and save data.

For active companions, Care, Health, Play and Decorate open a panel over the same full-screen room: half-screen in portrait, taller in landscape to keep its controls legible.
A solid black shadow marks its upper edge; the top-right circular X closes it.
Back returns through catalogue/preview levels, then closes the initial panel. Outside taps leave it open. Care uses three tiles in one row: Feed, Pet and Rest.
Categories, filters, object previews and placement stay inside Decorate's panel.
The rounded category and ownership selectors retain their selected black fill.
Active companions browse three larger, named object cards per portrait page,
or two horizontal cards in landscape. Cards have the main actions' rounded
outline and lower shadow; equipped objects carry a check without darkening the art.
Retired/lost companions retain their existing catalogue layouts.
Previewing never saves. Owned items can be placed or removed; unfinished items
retain their reading project and confirmation flow. Game boards retain their
existing play flow after choosing a game.

PET binds smooth Ubuntu/Noto fonts locally and uses vector symbols for its main
controls. It restores the shared label-case preference after each render. X4 Pro
uses one B/W composition and the ordinary fast panel update for every PET view.
Gray source ink uses ordered dithering, keeping the artwork and black contours
without entering and leaving the physical four-level grayscale waveform during
navigation. The browser preview retains absolute grayscale and continuous idle
motion. Both paths reuse the single framebuffer. CNART003 adds 35 fixed row-buffer
bytes per open pack. The room pack retains up to 96 KiB of packed source pixels;
the other main packs retain up to 64 KiB each. After changing a real SD card, replace all six packs while
PET is closed. Existing saves remain unchanged.


Info starts with raising a pet through reading, then explains food, mood,
energy, cleaning and decoration in short everyday language. Info shows three
cards per page in either orientation; difficulty is explained only in Rules.
Rules lists the daily food/remedy rewards, games, difficulty and projects. Paragraphs retain
regular text instead of falling back to a bold pixel font. The guide is
paginated for the current orientation and uses the care screen's real rules.
The reading-return summary enlarges its
elapsed-reading and growth labels without changing recorded totals
or rewards. The reading-help screen explains the daily totals directly: up to
six food portions (one at each of 5, 10, 15, 30, 60 and 120 minutes) and two
remedies (30 and 120 minutes). It explains that reading can be accumulated in
several sessions. Every recorded minute contributes without a daily target.

PET Settings keeps only the difficulty choices and Save.
Removing the repeated deadline preview leaves more room around the difficulty
rows; the confirmation required when a difficulty change would end a life is
retained. Reading rewards use a food bowl for provisions and an upward growth
arrow. The remaining-minutes caption is centred between its heading and bar.

PET appears in **Apps**, beside Reading Adventure. Choose a Forest, Lunar or Volcano Egg,
choose a difficulty, and press Begin. A PET-specific
name screen reuses Wordle’s letter keys, with the selected egg, a name preview,
a 0–12 counter, Space and Confirm name. Names allow uppercase A–Z, Ñ on the
Spanish keyboard, and single spaces, up to 12 characters including spaces.
Blank names are rejected and trailing spaces are trimmed. Back returns to egg
selection without creating a companion; confirmation writes the new life once.
The name appears on its dashboard, room and archived life. Older unnamed saves
remain readable and use their egg label until a new named companion is created.
PET keeps one rounded Carrusel 2 body style in every interface theme, including
selection and Settings. Common panels and controls share the same corner radius;
primary action cards also use the lower black shadow. Its header follows the
selected theme: black in Framed/Carousel, light in Carousel 2/Bookshelf.
The header has a larger title and 48-pixel controls within the same 72-pixel visible height as the other menus.
PET restores the theme's chrome flag after painting and does not change saved settings or other apps.
The stage/generation row also shows calendar days since creation, starting at
zero; missing or future birth dates show an unknown age. The existing minute
check refreshes this label when the local date changes, without a new timer.
The supplied monochrome artwork is loaded from the microSD packs.

During incubation, vital stats show locks and the Care tab opens an explanation
instead of care controls. Feeding, affection, rest, games, hygiene and treatment
unlock together at hatching (20 minutes of reading). Vital stats do not decay
before hatching; the care clock starts when the new stage is reconciled. Reading and the album remain available. Home's four slots, next project and project
chooser show locks until hatching. Inherited furniture remains visible; passive
project reading is preserved. The Care tab title stays unadorned so its label fits.
The dashboard prioritizes
minutes remaining until birth before switching to its furnishing project. Difficulty still controls
the absence grace period. Growth uses the same room, initially showing the previous
stage and a small confirmation card. OK starts a finite sequence alternating the
previous and reached stages with black silhouettes, then reveals the new form
with confetti. Only the companion changes; the room and entire screen do not
flash black. The reading receipt follows in the room. Back can skip the visual
sequence and open that receipt. Only acknowledging the reading receipt advances
the noticed growth/reading and supply counters; a failed save keeps them pending
for retry. The animation itself grants no reward and writes no save.

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
The main screen's remaining-life button opens the most recent book, or Home when there is none.

Provisions are food, not currency. Feeding spends one for up to 25 satiety.
Existing food/remedies and bonus remedies survive migration and new generations;
they do not expire. The initial remedy remains available. Stock comes from saved
earning counters, not from dividing lifetime reading. Reading before a new pet's
baseline consumes that day's thresholds without paying the new pet, so starting
a new generation or reopening does not reset the day's allowance. Reading days
follow the journal's recorded local date, not the date PET is opened.
Care shows the exact food count in a separate card above its actions; remedy stock lives in Health. Earning rules
stay in Help, available from the header. Food and remedies follow their daily
reading thresholds; PET has no daily reading goal.

Care uses three tiles in one row: Feed, Pet and Rest (or Wake while resting).
The food count appears in a rounded status card above them. Health is a main-room
action containing Clean and Cure. Reading stays on the room footer. An incubating
egg gets after-hatching guidance, never an ended-life message.
Rewarded games retain their capped mood gain and 20-energy cost; practice and
care blocks retain their explanations. Feeding shows its saved result only after
a successful write. Feed opens nine illustrated choices with the current pack, three per page:
berries, cake, night fruit, burger, pizza, sandwich, kibble, pet pate and a dental
treat. Legacy packs retain the original six choices. They all spend one food for up to 25
satiety. Tapping an available food saves the meal and closes the drawer, then shows
two eating poses with the selected
plate in the room. The two poses repeat for three bites; each bite removes more
food until only the empty plate remains, followed by a short satiety receipt. Each pose's dwell starts
after its display refresh, so a slow panel cannot skip a pose. Navigation cancels
the visual sequence; it does not repeat or undo the saved meal. Selected controls
reverse text and icons for contrast.

Selecting a different unowned
decoration starts its reading goal at zero. Requirements depend on how many
objects the collection already contains: 10, 15, 20, 30 and 45 minutes for the
first five, then 60 minutes per object. The requirement never exceeds 60 minutes,
including inherited or imported collections. For example, selecting an object
with 10,440 minutes of previous reading shows 0/60, then 30/60 after another
30 minutes. Prior reading does not instantly buy a newly selected object.
Reopening the same project preserves its progress; choosing a different one
starts that goal over. Growth, reading statistics and owned objects are preserved.
The first default project can use incubation reading and becomes available when
Home opens at hatching (20 minutes). Old saves retain valid progress up to the
new requirement, without revoking objects or demanding a cumulative deficit.
The optional versioned project-start trailer uses reserved bytes in the existing
640-byte PET7 record; the date preference and memory records are unchanged.

PET opens on one main screen with the companion, room, mood indicator and four
needs above the scene: Hunger, Mood, Energy and Hygiene. The room now spans the
whole display, behind navigation, needs and action controls. Its walls and floor
follow the viewport; the window and all companion/furnishing sprites retain their
aspect ratios. There are no white letterbox bands around a separate room picture.
The mood badge sits closer to the companion while still avoiding furnishings.
The black reading bar defaults to the selected unfinished furnishing's elapsed
and required reading minutes. Its icon is the furnishing's detailed illustration, inverted to white and gray for contrast on black.
The right-hand switch alternates this with remaining life; the main area still
opens reading. With no available unfinished furnishing, the bar shows life and
the switch is disabled. Before hatching, furnishings remain unavailable. This
display choice lasts only while PET is open and does not save the profile; a
new project restores the project display. The existing reading costs and
life rules are unchanged.
The taller needs panel emphasizes its numerical values, with distinct bowl,
heart, lightning and sparkle symbols. Larger action tiles use a paw, gamepad,
sleeping crescent and paint roller from the supplied design. Action tiles use
rounded corners and a lower black shadow; the needs panel stays rounded in every
theme. Back, clock, Album and Settings remain available in the larger header.
Care, Play, Decorate and their catalogue/preview sheets extend across the full
screen width and down to its bottom edge, with rounded upper corners and a matching black upper shadow. Controls
stay inside the viewable area. The Care sheet covers this same room without moving the companion or furnishings. Satiety rises toward 100 when fed and falls as food is needed. All four needs improve toward 100; the saved satiety value and care rules retain their existing meaning.

- **Care:** opens the care panel with food stock, feeding, petting and rest.
  The panel covers the room without moving it.
- **Play:** opens the minigame selector as a panel over the room.
- **Health:** opens the health panel with condition, remedy stock, Clean and Cure.
- **Decorate:** opens a panel with furnishing categories and the next reading project.

The header provides Settings and Album. Album contains accumulated reading,
reading days, best streak, life stages, illustrated memories and previous
generations. Needs stay locked until the egg hatches. The bottom button shows
the remaining absence grace period and returns to reading. Relaxed mode instead
states that absence cannot cause death; an unknown clock never invents a countdown.
With the current motion pack, a healthy awake companion greets entry
with two newly drawn greeting poses, then returns to normal. Feeding and care
reactions take priority. In the browser preview, sitting follows calm inactivity;
waking queues a stretch on both targets.
The original curiosity head tilt and blink remain available when there is no
urgent need. Those curiosity poses
wait for completed refreshes before dwelling (500 ms looking and 300 ms blinking
on X4 Pro; 1.8 seconds and 1 second in the browser). Navigation cancels either animation. The current pack has dedicated care poses. These
visual reactions add no reward or persisted animation field.
Mood badges keep a fixed size and position on both targets; their expression
updates with the companion's state or interaction. The browser preview retains
the separate egg animation. Physical idle indicators remain still between interactions.
Petting hands use solid black dark contours while retaining their light fills
and existing illustration packs. The enlarged room hand stays clear of the mood badge.

Short companion reactions depend on the recorded stage, today's reading and
petting. Outside explicit care/play reactions, care needs take priority. Unknown dates and pending reading
history never produce a reading reaction. Cleaning, healing, affection and game
results appear as animated room reactions with their actual saved numerical changes. Feeding
uses the room sequence and short receipt described above. Descriptive reactions
add no stored event, reward or save-format change.
The Home project has an outlined progress bar; a completed collection uses a larger trophy. Album's Life stages
section shows reading minutes remaining beneath every locked stage, rounded up to
the next minute. Furnishing progress remains a separate goal.
The main screen groups the four needs in one outlined panel, with the same
rounded Carrusel 2 styling under every interface theme.
Furnishings belong to Home. **How reading helps** explains the reading rewards;
Album shows its separate current-life accumulated reading total.
The reading bar shows an unfinished furnishing's progress by default. Its right
switch reveals remaining life, including urgent deadlines; without a pending
furnishing, life is shown directly.
Recorded reading before the deadline renews that period.

PET settings only select difficulty. The reading-return summary shows recorded
reading, provisions, remedies and growth, with no daily target or completion state.
Existing saves remain compatible: byte 62 is reserved and written as zero; old
preferences are ignored. Provisions, remedies, growth and furnishing progress
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
up to 25 satiety. The feeding card opens nine illustrated choices with the current pack (six with
legacy packs), three per page, with
identical costs and effects. The picker previews the capped result; tapping a food saves and serves that
plate directly in the existing room. A care hint
uses the existing need priority and links treatment or cleaning advice to Health.
Health, affection, rest and games have separate cards. Health uses a medicine
bottle; the cleaning action within Health uses a soap icon.
These recommendations do not change care thresholds, costs or rewards.
Petting grants up to ten mood once per date, or fifteen when mood is below forty.
Petting returns to the room with a hand and rising hearts, retaining the matching
species and age. The usual care pose returns for the result. Its small receipt shows actual before/after mood values. Repeated visits
and an already-full mood show zero gain. Affection does not cure illness, fill
satiety or interrupt rest. This result is ephemeral; the existing daily save
field prevents repeat rewards. The shared bounded room-reaction clock adds no
pack, task, save field or per-frame heap allocation. Satiety falls two points per elapsed
hour and mood one; in Relaxed mode neither falls below twenty. Energy recovers
five points per hour (up to twenty hours per reconciliation). A two-hour rest
restores energy to 100 and wakes the companion automatically.

Hygiene falls one point per elapsed hour after hatching. It stops at twenty
in Relaxed mode. At twenty or less, a healthy companion develops a cold, or
malaise if satiety is below twenty-five. Cleaning is free, restores hygiene to
100 and adds five mood; it does not cure an existing illness. Healing consumes
one remedy, removes the illness and restores hygiene to at least sixty.
Every profile begins with one remedy. Further remedies come from reading. Sick companions can still be fed, cleaned and rested;
minigames remain unavailable until they recover. Successful cleaning returns to
the room with bubbles and sparkles; healing uses rising hearts and sparkles.
Their receipts show actual hygiene/mood changes and remedy cost. They are
created only after a successful guarded save, finish automatically and can be
closed with X. They add no background task or persistent result state.

Pairs, Sequence and Sliding puzzle use turns, suitable for E-Ink. Pairs displays six
rounded cards with lower shadows, matched-pair and error counters, and keeps a mismatch
visible until Continue. Sequence has three rounds of five, six and seven symbols;
Ready to repeat hides the series. Each choice fills the next slot with the
player's selected symbol, without checking or revealing the answer mid-attempt.
The game checks only after the complete row is filled. A failed row remains on
screen with Retry; only pressing Retry returns to memorizing the same round.
Failed attempts increment the error count once and never advance the round or
grant a reward. The seven chosen symbols use a fixed activity-owned byte array,
with no heap allocation or save-format change. Sliding puzzle has eight numbered tiles and one empty cell in a three-by-three grid.
Only a tile next to the gap can move. Order 1–8 with the gap at the bottom right to finish.
Every initial board has even inversion parity and a Manhattan distance of at least twelve,
so it is solvable and does not begin nearly finished. Generation is bounded to sixteen attempts,
with a known solvable fallback. Restart restores the same initial board. There is no countdown
or automatic replay timer. The board and restart copy are fixed nine-byte arrays in GameState;
no heap allocation or persistent format change is introduced.

The header Back arrow and the explicit Menu button return from every game to
the minigame selector. Back from the selector returns to the main room. Exiting an unfinished game discards
its transient board without spending energy or granting a reward. Settings can
be opened from games and returns to the same board. Game state is bounded inline
activity data; no game board is saved to the microSD or added to the profile.
Finishing a game returns to the room for a brief hop and confetti when care
permits, with exact mood and energy changes in a small receipt. A care-blocked
practice result keeps the care pose and does not invent a prize. Replaying or
dismissing the visual sequence never grants a second win or reward.
The selector shows three difficulty tabs, with the selected game
reversed to white on black, and a board preview. Play headers show the game name and instructions, with the shared PET
gamepad icon in portrait. Mood and energy stay separate from the daily prize
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
Wake the companion in Care before feeding, petting, cleaning or healing.
Attempts during rest return to Care with a wake-first message and its Wake
button. Core care functions also reject these actions while resting, leaving
resources and needs unchanged. Successful cleaning after waking uses its normal
room animation; save failures never show a successful action.

The available catalog contains 223 objects: 60 rest items, 43 decorations,
60 toys and 60 environment items. The chooser has category buttons and discrete pages;
the Home slot shows the actual equipped artwork. The chooser uses a compact title
and a page counter between previous/next arrows. The top-right X closes the
panel; Back returns through preview and catalogue levels. Reading counts
for growth and the selected project together. A different unowned selection
starts a new goal; earned objects and overall reading statistics remain intact.

After all 223 available objects are owned, the next-objective card in PET and Home becomes
a trophy button marked **Home complete!**, with a 223 / 223 count and **View trophy**.
It opens a congratulatory window explaining that the collection is saved and
inherited by future generations. Continue and header Back return to the view that
opened it. Completion is derived from the existing ownership bitmap, with no new
save flag, automatic popup, extra reward or generation reset. Inherited eggs keep
Home controls locked until hatching.
The window follows the supplied celebration reference: a large star trophy with
laurels and sparkles, a ribbon title, prominent count and corner decorations.
The current artwork and generation provenance live in `REDISEÑOS/PET/assets/v2/`;
`assets_local/pet/collection/` retains the legacy illustration.
`tools_local/pet/build_collection.py` converts it to the required
`/apps/pet-v2/collection.bin` SD pack (one 280 x 210 CNART003 two-bit record; 14,740 bytes).
The activity validates it on entry, then opens it on the complete-collection window and on completed PET/Home
panels after hatching. Once opened, the pack and cache remain available until
activity exit. The existing fixed pack scratch is 1,094 bytes plus handle/metadata;
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

In the browser preview, ghost illustrations have a gentle vertical float on the
farewell screen and PET dashboard. The existing ambient clock moves the illustration by six pixels every 750 ms in a
three-second cycle; menus and saved memories remain still. Missing ghost art
keeps the static fallback. The black portrait card has one rounded fill without
an inset white border.

On the physical e-ink devices, PET keeps ambient poses, sleep marks, emotion
badges and ghosts still instead of scheduling repeated full grayscale waveforms.
The browser preview retains continuous motion. Feeding, care, growth, greeting
and sleep/wake transitions remain finite sequences on the device, using shorter
dwells and the same fast monochrome update as menus. Settling does not switch
back to native grayscale. A cleanup inherited from the previous app or a shared
system refresh policy can still be visible on entry; PET does not request a
grayscale cleanup on every action or idle clock tick.

## Storage and resources

Two alternating checksummed 640-byte PET7 saves live under `/.crosspoint/pet-v2/`.
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
files store life history, preserving names and recorded age; older 512-byte PTL1 history migrates
without discarding its records. Writes are read back before becoming active. An older
valid slot recovers an interrupted write; unsupported newer formats are preserved
read-only. Archive writes are idempotent per life number, including retries after
an interrupted profile save. A damaged or reset reading journal preserves the
companion and asks the user to restore the journal before awarding progress.

Previous generations use three horizontal cards per portrait page, with shared rounded
borders, lower shadows and separate paging buttons. Landscape uses three columns.
Each card shows the companion, generation, growth stage, calendar age, ending
date and shared reading time. Details enlarge the companion and separate reading
time, reading days and the ending reason into cards. Calendar age is captured
when the life is archived, independently of days with reading. Earlier records
without an age show “Age not recorded”; no age is inferred from their reading days.
Retries preserve an already recorded age for the same ending timestamp, including
after a timezone change.

Copy `resources/apps/pet-v2/` to `/apps/pet-v2/` on the microSD. The original
`graphics.bin` has 64 stable slots: 63 creature/object illustrations and the supplied hand. Required `home.bin` has 52
records: 39 state-pose slots, six landscape halves and seven native-resolution
room tiles. Earlier pack sizes are incompatible. A missing or invalid pack
shows the resource-installation notice.
The legacy pixel collection fills all 102 slots (87 distinct illustrations plus
15 exact aliases). Source PNGs remain in `assets_local/pet/png/`; explicit legacy encoder calls can still rebuild those files; the default build now uses the grayscale masters. The X4 Pro
browser simulator can use the same packs in its virtual SD card.
See [resource instructions](../../assets_local/pet/README.md).
Artwork is not embedded in firmware flash.

`habitat.bin` adds the supplied 2:1 room, all 240 accessories and opaque silhouettes
for the 39 Home poses. Its 560 records contain two background tiles and
279 mask/ink pairs. Exterior white is transparent; interior white covers the
floor and background. Explicit cutouts preserve open hoops and hammock frames.
Wall, ceiling and floor objects use separate anchors. Pet ink bounds determine
its baseline and size by age; illness takes priority over rest. A healthy sleeping
pet rests at a support point measured within its furniture, with the front rim
painted over it. The scene preserves its 2:1 aspect ratio in either screen shape.
The generated layout metadata and PNG derivatives come from
`tools_local/pet/build_habitat.py`; the supplied originals remain unchanged.

The activity owns its configured pack handles and releases them on exit. The Home pack
adds a bounded 4,928-byte optional PSRAM index/read cache; the pack's fixed scratch
buffers and bounded life records are activity members. The habitat pack adds
13,056 bytes of optional PSRAM cache and one set of fixed pack scratch buffers.
The required `/apps/pet-v2/breathing.bin` contains 1,278 CNART003 records.
The smaller packs below describe historical slot additions and are not accepted
by the current app. The initial atlas held 216 registered frames covering all
three species, four hatched ages and six poses: normal, happy, hungry, tired,
sick and resting. Twelve static head crops and nine egg wobble frames complete the pack (474
legacy records). The current pack appends 60 mask/ink records for three foods
(whole and bitten) and two eating poses per species and age, for 534 legacy records. The current pack appends 48 more records for all six foods
in four registered states: whole, two-thirds, one-third and empty plate, for 582
legacy records. The current pack adds 48 mask/ink records for two curiosity poses
per species and hatched age, for 630 legacy records. Another 192 mask/ink records
add two newly drawn poses for petting, cleaning, healing and play for every
species and hatched age, for 822 legacy records. The legacy 1,014-record pack
appends 192 registered replacements. Their character scale is measured against
the matching normal stage, excluding detached droplets/motion marks; both frames
share one scale and feet anchor. Extra canvas space preserves effects without
shrinking the character. Rendering matches the registered character to the current idle body's
visible area and feet, so care does not resize an ill or hungry companion.
When a resting companion peeks from a shelter, placement derives its full-body
scale from the actual head crop instead of fitting a body into the head's box.
Legacy 822-record packs retain the original action
illustrations and placement. Legacy 630-record packs retain curiosity
and use the existing poses/effects for care reactions. Legacy 582-record packs retain all
six foods without the curiosity reaction. Legacy 534-record packs retain two eating poses and their three original
food illustrations; new foods use a generic icon. Legacy 474-record packs show
the static companion and generic food icons. Both hide food after the last bite.
The 1,182-record pack appends 24 mask/ink records for kibble, pet
pate and a dental treat in four consumption states, then 144 records for seated,
waking-stretch and greeting poses (two frames for three species and four ages).
Original slots 0-1013 remain byte-identical. Legacy 1,014-record packs retain
prior actions and six foods; new poses require the new pack. For the current 1,278-record pack, the optional index grows by 4,224 bytes
over 1,014 records; the existing 64 KiB pixel cache limit
is unchanged. Its bounded index/read-ahead cache uses 24,544 bytes of PSRAM, with
the existing fixed small-read fallback on allocation failure. Current atlases, prompts and provenance are in `REDISEÑOS/PET/assets/v2/`;
legacy atlases remain in `assets_local/pet/animation/`. Regenerate with
`tools_local/pet/build_breathing.py`. The activity owns and closes the pack.

After changing action masters, run `python tools_local/pet/gray_assets.py --update-registration`
before compiling firmware, then regenerate the pack. This
updates the checked-in `PetReactionData.h` and `registration.json`. The fixed Q8
body metrics avoid square roots at render time. Normal pack
builds validate the generated header instead of silently changing it after C++
compilation. Registration is fixed flash data; drawing adds no allocation.

Dedicated hungry artwork in REDISEÑOS/PET/assets/v5/ shows each species and age
looking into an empty bowl with natural paws. It replaces the previous sad
expression for hunger; other expressions and need priorities remain unchanged.

On physical devices, hatched companions stay still outside the brief curiosity,
care, play and feeding sequences. Their pose still reflects
happiness, hunger, tiredness, illness and rest. The existing body frames and
animation source artwork remain in the resource pack; physical still poses
select the first body frame. PET shows the full body, while Home may
show a shelter head. Sleeping Z marks stay still too.

In the browser preview, egg wobble changes every 0.75 seconds and sleep marks
advance every 0.6 seconds through the existing activity clock. Mood badges use
static native monochrome faces, with a smaller badge in Home. They do not
schedule periodic refreshes or follow animated pose registration. Menus, previous lives and reading
remain static. Required resource installation checks and save formats are
unchanged.

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

The required `/apps/pet-v2/farewell.bin` contains three ghost illustrations, each
with an opaque silhouette and ink layer (six records). Source PNGs and
provenance live in `REDISEÑOS/PET/assets/v2/`; the legacy masters remain in
`assets_local/pet/farewell/`. Silva uses the corrected halo-and-wisp illustration. Rebuild using `tools_local/pet/build_farewell.py`. After validation on entry, the activity
reopens this pack only for a lost companion and closes it when a new life starts
or PET exits. It adds one fixed pack member (1,094 scratch bytes plus handle and
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
Landscape rearranges the interface without scrolling. Health opens directly from
the main room; games do not replace Home in the tab bar.

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
with a gear and difficulty. Settings presents the four difficulty modes and Save.
The active PET overview shows the companion's four needs above its room.
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

The compact reading receipt shows time since the last acknowledged visit and actual food/remedy gains in the room. Gains use persisted earning and acknowledgement counters, excluding the initial remedy and previously earned bonus credit. Acknowledging the receipt advances both reading and earning counters. Detailed reading totals and stages remain in the Album. All reading still counts for Home projects, while supplies follow the daily diminishing thresholds.

PET links a current need to Health or Care. The meal receipt shows the one food spent. Tapping a food card serves it directly without a second Feed button and shows the selected food and two eating poses repeated for three bites in the existing room after a successful save. Each bite consumes another visible portion, ending with an empty plate. A brief room receipt shows actual satiety before/after and the food spent, then disappears. There is no feeding confirmation window. The sequence and receipt are temporary activity state and clear on navigation; only the already-existing food choice field is saved with the successful meal. Food IDs 0-5 remain PET7; IDs 6-8 write PET8 with the same layout and checksum. Older firmware rejects PET8 instead of falling back to an older meal balance. The clock uses the existing activity loop, with no new task, framebuffer or per-frame allocation. Eating-pose masters and prompts are in `REDISEÑOS/PET/assets/v3/feeding/`; the six-food consumption atlas and prompt are in `REDISEÑOS/PET/assets/v4/`.

Home catalogue cards open a static preview: an enlarged object and the room with its proposed placement. Owned items use Place; unowned items use Choose project. Opening or leaving does not save. Only confirmation uses the existing guarded save. Back retains category, filter and page. Read-only profiles may preview, but cannot apply. A reusable fixed-size Profile copy lives in the activity Work workspace; no framebuffer, heap buffer, task, timer, persistent field or asset pack is added. Room preview does not animate.

Preview arrows browse the current category and Owned/Unowned filter without saving. In an active companion's sheet, the counter shows the object's position; catalogue arrows show page numbers. Preview arrows move between objects and stop at the first and last item. Back opens the catalogue page containing the item just previewed. Read-only browsing remains available.

Confirming Place returns to Home after a successful guarded save. Choose project keeps Preview open. An equipped object shows its saved status and offers Remove; a pending project's confirmation is disabled. A different unowned selection starts its reading goal at zero and stays in To earn until completed through reading. Back returns to the catalogue page containing that item. In Owned, the equipped object appears first; all remaining objects keep their canonical order, so replacing it restores the previous object to its normal position. This order is derived and is not saved.

At full satiety, the food picker remains available but its food cards stay disabled, even with no provisions left. Full, empty-stock and read-only states explain why serving is unavailable. Wake warns that ending early forfeits the cycle bonus; current energy remains in its separate metric. Successful cleaning shows actual hygiene and capped mood changes and is free. Successful healing shows the hygiene change and the one remedy spent. Cleaning and healing results are temporary activity state, created only after a successful save and cleared when the room receipt closes or finishes; save errors never show a successful result. Remedy counts stay separate from mood values. Care receipts use fixed-size activity/model fields and reuse existing artwork with code-native bubbles, hearts, sparkles and confetti. Feeding adds its bounded clock and picker state described above, with no new save fields, heap buffers, timer task or SD pack.

Firmware and host-rendered UI checks cover these behaviors; device refresh and consumption require hardware verification.

### Care guidance and readable progress

Health shows the one-remedy treatment cost when needed and gives a short reason when a care action is unavailable. Existing care and save guards remain in force. Care shows the current energy and the gain from completing the two-hour rest cycle. Waking early ends the cycle before the full recovery is applied.

Accessory previews show remaining reading as hours/minutes, rounding incomplete minutes up; zero remaining time is Ready to collect, with confirmation still required. The Games tile in Care uses the same derived offer as the game menu: actual capped mood gain and 20-energy cost for rewarded play, free practice, or a short reason play is blocked. These are presentation changes, with no additional saved fields, asset packs or heap allocations.

### Contextual health and consistent daily labels

The reward only shows today's recorded reading when date, history and save data are available. Unavailable data never shows an invented daily result. Album distinguishes Reading days from Accumulated reading time for the current pet.

The reading receipt is a compact rounded card above the lower edge, leaving the
room and companion visible. It emphasizes time read since the last visit,
followed by separate provision/remedy cards and today's reading. Its circular X
and Continue acknowledge the existing saved rewards without changing their
amounts. Drawer option cards share the main buttons' 12-pixel corners, two-pixel
outlines and three-pixel lower shadow, including food, game and furnishing
choices. Selected and disabled states keep their existing meanings.

Health is a drawer over the existing room: half-screen in portrait and taller
in landscape. It shows condition,
remedy stock, treatment cost when ill, and saving availability. Clean and Heal
are disabled when unavailable. Its Info button opens the existing health guide;
Back returns from the guide to Health, then to PET. The drawer X closes to PET.
There is no daily cleaning limit. Existing care/save guards still govern actions.
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
life began. Streak uses the same threshold; the statistic cards explain it. The expanded
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

Current room receipts use localized words for before/after values, such as
70 to 90 (70 a 90 in Spanish). Legacy layouts can still use the right arrow;
the shared pixel and prose cuts include an explicit U+2192 glyph. Run
`python tools_local/toybox/extend_status_arrows.py` after regenerating those cuts;
it preserves existing glyphs, metrics and kerning and is idempotent. The five
added monochrome glyphs are flash constants, with no runtime buffer or allocation.


Mood faces use a proportional slot beside the upper-right of the fitted companion,
bounded to 36–64 pixels when space permits. The PET anchor follows the current
species, stage and pose dimensions and stays inside the illustration area.
Home uses a 28–42 pixel face in screen coordinates, choosing
among four nearby anchors to reduce overlap with the companion, window and placed
furnishings. The two pulse sizes share a fixed centre. Missing artwork follows the
same single-badge path; it does not add a second face. These are bounded layout
calculations with fixed rectangles, no extra animation frames or refreshes.


### Reference-based panels and memory journal (simulator)

PET adds nearby grass and a cross before Go to Health. The name has no separator beneath it.
Growth and Home project use illustrated bordered cards. Care keeps its
actual capped outcomes, while treatment guidance links to Health and the four
action tiles have clear navigation marks. Home removes the repeated reading-rule
strip, enlarges the Home project panel and labels its main action Decorate home.
Header reading help remains available. Three temporary arrangements were rendered;
the first follows the supplied references and no variant macro remains.

The supplied petting-hand image is preserved in assets_local/pet/ui/ and packed
as graphics record 63. The existing 0–62 IDs retain their artwork and order. The
builder thresholds/crops/scales only this new UI source; original creature PNGs
remain unchanged. The 64-record graphics pack lives on the microSD. Older
63-record packs used the code-native hand fallback in older firmware; they
are rejected by the current app.

On the host renderer, Profile is 624 bytes. The four resident Profile copies
add 128 fixed bytes for the provision ledger; the timeline adds 56 fixed bytes,
for 184 retained bytes in this refinement. Relative to the original 256-byte
Profile, the four copies have grown by 1,472 bytes across all PET updates. Its
Store byte workspace adds 384 bytes. No additional allocation, framebuffer, task,
timer or per-frame container is introduced. The optional graphics index grows by
16 PSRAM bytes; the existing cache/read fallback owns its lifetime. Real hardware
memory, display and power measurements remain pending. Only the browser simulator
and gallery are built and delivered in this iteration.


PET panel corners keep the fixed rounded Carrusel 2 style. The compact Home project card omits heading dividers; the name separator is also removed and progress bars remain.


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
Best streak is the longest run of days with at least five minutes of reading
with the current pet. It survives interruptions, using the existing streak field
and save format. Existing active saves retain their last stored streak and are
reconciled against the available reading-history window; older unrecorded runs
outside that window cannot be reconstructed. Ended lives keep their saved value.
The daily summary covers 730 days, which also bounds recomputed runs; a larger
previously saved maximum is retained. No profile field, explicit app allocation,
art asset, timer or display refresh is added. Multiline text still uses the
existing renderer and its internal allocation behavior.

Care describes Rest as Restores energy at every current energy value, with Cycle: 2 h beneath it; current energy remains in its separate metric. While resting, Wake explains that ending the cycle early forfeits full recovery. Passive recovery continues and a completed cycle restores energy to 100. Health's action row shows only Healthy, Cold or Malaise; its hygiene reading is already in the separate metric above. The eleven Care icons are extracted directly from the supplied reference and packed by `tools_local/pet/build_care_icons.py` into the existing one-bit Icon format. Their 5,632 bitmap bytes plus icon metadata are compile-time flash data; no new heap allocation, SD pack, framebuffer, timer or task is needed. The source screenshot and crop manifest remain in `assets_local/pet/ui/`.

The Album archive is called Previous generations throughout its heading, summary, empty state and reading help. Compact buttons use Generations. Spanish loss messages use fallecer with a Despedida heading; English uses passed away. The absence rule and reconciliation behavior are unchanged.

The active PET dashboard keeps the same outlined needs panel from Egg through Adult. Egg needs show their usual icons and labels with a lock in place of the value. The name and growth stage sit above the room; the four actions and reading button stay below it. Ended-life controls retain their separate layout.

These layout, wording and icon refinements retain the existing care and minigame rules. The provision ledger is the only new persistent gameplay state in this update.

The PET dashboard uses the supplied species landscapes: Silva/Bosque gets the
forest, Nox/Lunar the moon and Ember/Volcán the volcanic plain, from Egg through
Adult in legacy packs. Their original PNGs and provenance remain in
`assets_local/pet/backgrounds/`. Current masters use one clear grayscale room,
exported at native resolution as four portrait tiles (IDs 45–48) and three
landscape tiles (IDs 49–51). The compatible panorama halves remain at IDs
39–44. Pose IDs 0–38 and all placement dimensions remain stable.

The activity requires the 52-record grayscale Home pack. It does not retry
45/39-record packs. Existing flags gate full-room tiles and panoramas; no saved state,
extra pack/handle or framebuffer is added. The existing optional PSRAM index
grows by 112 bytes from the 45-record pack (208 from the 39-record pack), and the same read-ahead buffer and small-read
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
These are memory milestones, separate from the 223-object home collection.
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

PET places care links beside its status text. The bottom remaining-life button
opens reading, whether the absence deadline is urgent or not. Care and Health use separate
landscape arrangements. Care/play results animate in the room and show an
actual-effect receipt that closes automatically or with X. Temporary action
poses preserve species and age; the usual care pose returns for the result.
Generic information has no fabricated receipt. Previous-generation cards show
their local end date, or the existing unknown-date label when unavailable.

`tools_local/pet/build_reference_icons.py` converts the supplied UI reference crops
to `PetReferenceIcons.h`: twenty images, 45,088 bitmap bytes in compile-time flash
constants. The medal, lock, empty/completed pedestals, body soap/remedy artwork,
book and target are direct crops. The legacy Silva celebration, petting and Ember cleanup illustrations apply
only to healthy, awake adults of those species; other ages, species and conditions
retain their matching existing artwork. The older feeding crop remains a legacy
resource; meals now use the room animation for every hatched species and age. Crop metadata,
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
A selected unfinished project keeps its reading progress. Home shows its full collection trophy whether or not any furnishings are
placed. PET's collection trophy remains available. These five new crops
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

PET requires the complete `/apps/pet-v2/` folder from `CrossNext-Resources.zip`:
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

The life-stage timeline uses five equally spaced columns with centered images
and labels. Stage names can use two lines; Spanish Recién nacido wraps at the
word boundary. The current stage uses the same size with emphasis; compact captions use a thin
underline when a bold face would not fit. Care supplies are a read-only status panel without a chevron or
focus/touch action; reading guidance remains available in the header. PET's
shared header uses separate 48-pixel help/album and settings controls and
reserves space for the clock when enabled; battery information is omitted.

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

In the browser preview, eggs advance every 750 ms and sleep marks every 600 ms
while visible; physical devices keep those idle frames still. Mood badges remain
static on both targets. Hatched bodies stay still outside the short curiosity,
care, play and three-bite feeding sequences. Menus and historical pictures stay
still. Each eating pose waits for its completed
display refresh before its dwell begins. The two poses repeat three times,
consuming one provision for the entire meal. On X4 Pro each pose waits 300 ms
and the result waits 900 ms: 2.7 seconds of dwell plus rendering/panel time.
The browser preview retains 1.2-second poses and a 2.4-second result. Navigation
can cancel either sequence sooner.

PET's four main artwork packs can retain up to sixteen verified source sprites
per pack: 96 KiB for Home and 64 KiB each for the other three (288 KiB total
additional pixels). The room allowance fits all four portrait tiles or all three
landscape tiles together, so repeated views do not evict their own background. Cached
draws reuse those bytes without microSD reads or seeks. Allocation happens when
opening the app and is released on exit; allocation failure retains the existing
read-ahead/small-read path. Other apps keep the default uncached pixel path.
Retained images are checksum-validated in that same allocation before drawing,
avoiding a second SD read on their first use. Uncached images retain streaming
checksum validation before their pixels are drawn.
Drawers clip the room to the area above their opaque surface, preserving the
rounded top corners. Packed artwork rejects wholly clipped images before pixel
reads and decodes only visible rows and column runs; the streamed fallback still
reads complete rows. First-visible-image checksum validation is unchanged.
The trophy pack stays open after its first use until PET closes.
Trophy pixels have a 16 KiB optional cache on first use, while farewell pixels
use 64 KiB when that life is lost. These allocations reuse the pack owner and
its existing index/read-ahead allocation; they add no framebuffer and release
on the existing close/exit paths. Their allocation-failure fallback remains valid.
Selecting the
already-visible main section does not request another repaint. These changes
reduce avoidable work; actual transition time still includes the e-paper refresh.
Black and white artwork is drawn as horizontal ink runs rather than individual
pixels, with the existing nearest-neighbour sampling and clipping preserved.
After a section becomes visible, its first animation update waits one frame
interval; entering the section does not cause a second immediate animation repaint.


The overview's hatch/project card shows the goal name without a duplicate birth
heading, with larger text and separate artwork. Goal names can wrap within the card; the
reading fraction and progress bar remain separate. Home project names use the
same readable heading treatment. Progress targets and earned objects are
unchanged.

Long names wrap within the available card area; exceptionally long names use an
ellipsis and remain available in full in the object catalogue.

The PET progress card shows a larger egg or furnishing on the left, with its
regular-weight name and reading fraction on the right. The species background
fills the companion card width and is clipped below status text. Selecting an
owned furnishing and successfully saving its placement returns directly to Home;
choosing an unfinished project keeps the preview. A failed save stays visible
with the existing error feedback.

The Album separates accumulated reading, reading days and best streak into
rounded metric cards. The illustrated book uses the same reference artwork as
the earlier album. Memories show the room background cropped proportionally to
their frame, with the captured furnishings and companion drawn over it.
The detail shows age on the recorded date and larger accumulated minutes;
older memories without a date show an unknown age. Arrival can use the saved
birth date to show age zero without inventing an unrecorded reading time. Home
places the smaller mood badge near the companion while avoiding the window and furnishings.

In Decorate home, swipe left for the next catalogue page or right for the previous one. Category and Owned/To earn filter stay selected. The first and last pages do not wrap. A swipe does not open, place or select an object. Use the header Back arrow or the physical Back action to return; the catalogue reserves horizontal gestures for paging.

Decorate pauses the covered companion animation in both orientations. Closing
its panel rebases the animation clock before the first visible update.


### Reactions in the room

Petting, cleaning, healing and completed games return to the main room instead
of opening a full-page success window. Two action frames precede a short result
receipt; the sequence ends automatically and X dismisses it sooner. While the
receipt is present it replaces the main action area, so a hidden button cannot
receive a tap. Dismissal and automatic completion rebase the interaction revision.
All normal navigation cancels transient reactions. No animation grants a reward
or saves state; existing guarded care/game transactions finish first.

The shared inline clock waits for each completed display refresh before starting
its dwell. On X4 Pro ordinary action frames wait 300 ms and results 900 ms.
Growth first requires OK, then alternates black stage silhouettes at 200–400 ms,
reveals the new illustration for 600 ms and shows its confetti result for 900 ms.
The browser retains its 1.2-second action frames, 2.4-second results, 0.8–1-second
growth silhouettes and 1.6-second reveal. The compact reading receipt follows. Rewards and noticed stage
counters are acknowledged only through that reading receipt, including when
Back skips the visual sequence. These timings exclude the physical panel refresh.

The care/play illustrations are new two-frame atlases in
`REDISEÑOS/PET/assets/v8/acciones/`, with saved prompts and provenance. They share
one scale and feet baseline per pair; the play hop lifts its second frame six
logical pixels. Explicit care reactions temporarily show their action pose even
when another need remains; normal need artwork returns for the result. A game
that could not reward because care is needed keeps the need pose and omits
confetti. Growth uses the existing old/new stage illustrations.

Effects use fixed tables. The clock and phase snapshot are bounded activity/model
fields; no new task, framebuffer, per-frame allocation, dependency, SD pack or
persistent field is introduced. The appended images reuse the existing bounded
sprite cache; only the optional pack index grows as described above.

Celebration confetti uses 24 strips, diamonds, outlined paper pieces and curled
ribbons projected from one origin at the companion's feet. Depth controls size
and shade; distant pieces are painted behind the furnishings and companion,
near pieces in front. Soft shadows stay on the floor and falling pieces flatten
when they land. It runs on the same bounded phases and does not flash the whole
screen.


### Shared PET controls

PET keeps rounded cards, two-pixel outlines and a three-pixel lower shadow
throughout its drawers, action results and close buttons. Its header follows
the selected theme's light or dark background, uses the shared application
title face and omits battery information. A larger clock is shown only when
the interface clock setting is enabled.

In portrait, Decorate places its category controls above a compact reading-project
card with the chosen furnishing, earned minutes and progress bar. Landscape
places those controls beside the card. A completed
collection shows its trophy instead. Category and ownership filters are
separated from the object cards. Play previews the selected matching, sequence
or sliding-tile game before starting it. Icon cards show mood gained and energy spent, while
the daily reward counter is omitted from Play. Reward and energy rules are unchanged.

Care and game results show one before-to-after transition per metric card,
with any food or medicine cost beside them.
They stay inside the room and use the existing transient animation lifecycle.
Growth alternates only black silhouettes from its first animated frame through
the final pre-reveal frame. The old illustration remains visible while awaiting
confirmation, and the new illustration appears with confetti only at Reveal.
This also covers hatching for all three species.


While the companion is an egg, Care, Play, Health and Decorate show lock badges
and cannot be selected. Side-button section navigation skips these locked
sections and cycles between the room and Album. The black reading bar shows the remaining reading
minutes until hatching, rounded up to the next minute. It still opens reading;
the project/lifetime switch returns once the companion has hatched.

Care's three action icons use the editable SVG set in
`REDISEÑOS/PET/assets/v9/ui/care/`: a paw-marked food bowl, plain paw and moon.
Health reuses the same set's soap and outline heart. Four-shade antialiasing
preserves smooth edges; the open-book asset remains available for reading surfaces.


### Touch paging and game surfaces

Swipe horizontally through the screen's centre to page through Comidas, decoration catalogues and previews,
memory galleries and previous generations. A left swipe advances and a right
swipe goes back; paged lists stop at their first and last pages. In the room,
Care, Decorate and Album, the same gesture moves between those four sections.
Eggs cycle only between their room and Album, including coalesced physical
navigation steps. The Jugar chooser also supports swiping between its three games.
Game boards keep their own input: a puzzle swipe moves an adjacent tile into the
empty space, and never leaves the game. Settings and action confirmations do not
change pages on a swipe. The existing rightward gesture from the left edge remains
Back outside the catalogue and sliding puzzle.

Comidas reserves additional space below the sheet header, separating its X from
the stock, page arrows and food cards. Pair, sequence and puzzle boards use PET's
rounded two-pixel outlines and three-pixel lower shadows, with instruction and
progress cards above the board in portrait, beside it in landscape, and a separate bottom control row. Game rules,
reward costs and saved progress formats are unchanged.


### Clear care values and health status

The Hunger / Hambre bar displays the existing saved satiety value directly; 100 means no hunger:
feeding adds up to 25 points, capped at 100, and time lowers it. New semantic label
and meal-preview keys make other languages fall back to the correct meaning.
Numeric care receipts show one prominent transition such as `80 -> 90 / 100`,
with the final progress bar below it. The current value has no separate repeated
headline. This applies to feeding, petting,
cleaning, healing and game-result metrics.

Health shows a prominent Healthy / Ill status, the specific illness when relevant,
and the actual remedies available. Cleaning and healing keep their contextual
status, cost and existing enable/save rules inside their respective cards. The
shared rounded borders and lower shadows are retained. Its help control sits
beside X in the sheet header. Landscape keeps status beside the two action cards.

The reading-project icon uses the actual furnishing artwork, not its opaque mask,
in white and gray at 48 pixels to preserve the details against the black bar.


### Main health access and focused care

The main actions are Care, Play, Health and Decorate. Care retains Feed, Pet
and Rest/Wake in its existing three-column card style. Clean and Cure live in
Health, while reading remains available through the main room footer. Opening
Health and closing it with Back or X returns to the main room; its guide returns
to Health first. Egg locks still disable all four main care/game/home actions.
Existing rest, cleaning, healing and persistence rules are unchanged.


The Care panel places a food-stock card above Feed, Pet and Rest/Wake, following
Health's status-card hierarchy. Landscape places this stock card beside the actions to preserve
readable effect captions and usable action heights. Action receipts show food/remedy cost or free status in
the regular large bold UI text; numeric result cards use one before-to-after
transition. No care or resource rules change.


### Care effects and compact food choices

Care and Health action cards show an illustration, an action label and a small
text panel for the expected gain or the reason an action is unavailable.
A full two-hour rest restores energy to 100. The Rest card shows how much
energy the companion will recover. Waking the companion early ends the cycle
without applying the full recovery.
The food-stock card uses the existing kibble illustration; Feed retains its
sandwich illustration.
The bone-shaped food is labelled Biscuit / Galleta; its saved choice, artwork,
cost and effect are unchanged.
Matched pair cards carry a large black badge with a thick white check so their
completed state remains distinct from a temporarily revealed card.
All three egg designs use the black-contoured v12 atlas, including their wobble
frames, setup choices, home poses and album illustrations. Pack IDs and target
dimensions are retained.

The visible food meter is labelled Hunger (Hambre), retaining its positive
meaning: 100 means the companion has no hunger. Feeding raises it; time lowers
it. The food picker uses illustrated cards, pagination and a short explanation
when a meal cannot be served.

The food-stock card uses the existing kibble illustration from the extended
food pack, while Feed uses the sandwich. Pet uses
the exact paw symbol of the main Care button. No new artwork pack is introduced.

### Sleep feedback, header actions and simulator history

In the browser preview, three staggered z marks rise beside a sleeping companion
at 600 ms intervals on the existing animation clock. Physical sleep marks stay
still. Opening menus suspends room animation as before.
Care shows a centred wake-first reminder while resting and keeps Wake available.
The main header actions have white backgrounds with black icons on Marco and
Carrusel, and black backgrounds with white icons on Carrusel 2 and Estantería.
The settings icon reuses the Apps menu gear; the information icon has an unfilled
circle and a solid i. Album reading time, reading days and streak cards are passive
information.
The disposable simulator presets include three prior generations, one per
species, encoded and checked with the production history codec; the active
companion is generation four. Device saves and history formats are unchanged.
The needs panel uses fine black dividers in both monochrome and grayscale passes,
so room redraws do not change their shade.


### Contextual companion reactions

The companion greets on entry and stretches after a successful wake. In the
browser preview it also sits after 30 seconds of calm inactivity. Tapping alternates sitting and greeting when
calm; hunger, tiredness, dirt and illness show a short in-room hint instead.
Tapping never spends supplies or grants affection. While resting it only asks
to wake first and keeps sleeping. Eggs, ended lives and drawers have no pet hit.

A fixed activity-owned clock holds each frame until it has actually painted,
then waits 300 ms on X4 Pro (700 ms for sitting). The browser retains one-second
dwells, or 1.8 seconds for sitting. Navigation cancels the reaction.
A successful meal adds satisfaction; a saved decoration unlock queues surprise
until the room is free. Wake/unlock cues queue only after successful persistence
and clear across generations. Expressions extend the existing PET face renderer;
the rejected generated emotion atlas is excluded. No new task, framebuffer or
per-frame allocation is added. Masters and exact prompts are in
`REDISEÑOS/PET/assets/v10/personality/`.

Three new foods cost one provision and restore the same capped +25 hunger.
Whole, two-thirds, one-third and empty-container illustrations accompany the
three bites. PET8 only expands the accepted food-choice range at byte 72; the
save size and other fields stay fixed. Six older food IDs keep writing PET7.


### Rest and wake journeys

The current 1,278-record breathing pack adds two walking poses per species and
hatched stage, registered in both directions. Walking occupies slots 1182-1277;
all existing slot IDs and dimensions remain fixed, with black-contoured eggs in
slots 456-473. Walking frames
are bounded to 128 x 160; the whole pack remains below the loader's 8 MiB limit.
The index adds 1,536 bytes of PSRAM over the 1,182-record pack, reusing the same
handle, read-ahead and 64 KiB sprite cache. No task or framebuffer is added.
Masters, built-in generation prompts and registration data live in
`REDISEÑOS/PET/assets/v11/rest/`. Regenerate the registration headers with
`gray_assets.py --update-registration`, then run `build_breathing.py`.

After a successful Rest save, the companion takes three short steps toward the
bed, sits and settles behind its foreground. After a successful wake, it rises,
stretches beside the bed and walks back to its normal spot, facing the return
direction. Shelters retain the existing head crop at the doorway while entering
or emerging; the full stretch happens outside. Without a bed it settles and
stretches in place. All three species and four hatched stages share this flow.

Each visual phase waits for its actual completed display refresh before its
200 ms walking dwell or 300 ms settling/stretching dwell on X4 Pro (650/900 ms
in the browser preview). The profile's rest
state is committed first, so care stays blocked during the approach to sleep.
Navigation cancels the visual journey and displays the already-saved state;
re-entering does not replay an old walk. Direct pet touches are suppressed while
a journey runs. Save failures never start a journey. Timed two-hour waking and
all food, mood, health and rest rewards keep their existing rules. No animation
field or format change is persisted. The current app requires the complete
1,278-record grayscale pack; earlier packs show the installation notice.


### Shared PET surface spacing

PET uses one surface contract for action buttons, information cards and history
cards: a 12 px radius, 2 px outline and 3 px lower shadow. Full-page content and
drawer content share a 16 px safe-area inset; footer geometry reserves the shadow
inside that inset. Structural frames keep the same outline without filling or
erasing their content. Major footer controls are 48 px high, with 36 px compact
footers in landscape drawers. Related controls use the smaller existing spacing
step; separate groups use 16 px. Photo borders inside memory tiles use a 6 px
radius to follow their 6 px inset, without a second elevation shadow.

Portrait drawers continue to occupy the lower half. Landscape drawers start
below the statistics rather than squeezing the catalogue into half the short
screen. Pager controls share the same shape and state handling across decoration,
memories and generations. Explicit PET rectangles use the shared FreeInkUI frame
button primitive with a 24 px minimum hit size, so the row wrapper cannot expand
compact controls into adjacent targets. Primary actions retain their larger
visible rectangles. This changes layout and hit bounds only, not save formats,
reading rewards, game rules or navigation actions.

### Collecting a completed decoration

Completing the selected decoration's reading time leaves it ready to collect.
The return receipt offers **Get now**; only that action earns and places the
decoration. Closing the receipt leaves the existing room unchanged, and the
completed project remains available on the next visit. Its catalog preview also
offers Get now. Previously owned decorations and save files remain compatible.


Album statistic cards place a larger icon beside the title above the value. Titles can wrap to two lines within their card. Jersey25 uses uppercase header titles while other text retains its normal case; the header shares the menu's 72-pixel visible height. Check long translated labels, both orientations and the light/dark header themes on the device.

The Album accumulated-reading card uses the original 64-by-48 book artwork in
a 64-pixel slot, preserving its aspect ratio. Reading-day and streak icons keep
their sizes. Play omits the Rewards today counter in both current and legacy
presentations; daily reward/practice rules and saved game state are unchanged.
Check the longer translated reading title, its value and the enlarged icon in
portrait/landscape, and open each game after both a fresh day and three rewards.
