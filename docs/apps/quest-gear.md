# Reading Quest equipment

Clothing, Aura and Pet are the three equipment categories. Clothing contains
100 complete outfits, each with a masculine and feminine figure, plus a
separate simplified garment icon for the shop. Headwear, clothing and boots
are one illustration; no runtime head, neck or footwear join exists. One
purchase unlocks both gender variants of an outfit.

The shop displays level and available coins above its categories. Six cards
fit a portrait page; three fit a landscape page. The cards show isolated
garment icons, prices, required levels and locks. Shop contains only unowned
items; My gear contains purchases. A successful saved purchase opens My gear
on the page containing the new item. Selecting a card opens its purchase or
equip controls. The complete character and
companions appear on Hero, not in the shop header. Page numbers and arrows
support the full catalog without scrolling.

Outfits unlock from levels 1 through 100, from apprentice to celestial/divine
armor. The 50 pets progress from real animals through invented and mythical
creatures. Each pet has an individual native sprite sized for the hero's
256-unit world frame. The enlarged cat sets the minimum: every pet, including
the hamster, has at least 96 visible native pixels of height. Large creatures
have larger native canvases; a small common sprite is never enlarged per species
at runtime. Byte-aligned width is bounded at 280 pixels for the existing row
decoder. The whole scene fits proportionally within the available screen area,
so its camera can shrink both companions and hero together. The hero remains
separate and visible beside the largest creature. Aura frames expand from 135% to
150% of hero height and at least 140% of its width. Aura art preserves its
192-by-224 canvas and aspect ratio, with an empty central avatar window.
The scene uses the same camera frame for every outfit.
The aura surrounds the hero behind opaque character and companion masks.

Hero centers a large character below its title and level/coin row. Experience
has a label, numeric value and thick framed bar. Streak/books, the daily reward
have their own cards. The former next-unlock/all-unlocked card is removed,
giving its space to the character panel. Missions has the daily card, a reading
missions band, separate icon/progress/status/reward cards, and numbered pages.
Four mission cards fit portrait; landscape derives a smaller capacity from
available height. Rewards show EXP and their guaranteed whole-coin value under
one coin per ten EXP plus any explicit streak bonus. Fractional EXP carries
into later coins. Level-up gifts are additional currency. Claimed missions are read-only.

All 200 figures use the same 256-unit height and a shared hero frame whose width
is generated from the widest outfit. The scene frame depends on aura/pet choice,
not the selected outfit: buying a wider or more detailed costume cannot zoom
the character out. Two repaired female celestial figures are standalone source
overrides, with their generation prompts beside the PNGs under `overrides/`.
Figures are fitted proportionally into 128-by-208 export canvases with filtered
monochrome reduction to preserve small facial details. The catalog gallery uses
the same C++ avatar renderer and common frame as Hero, rather than sizing each
thumbnail by its individual width.

Sources and catalog metadata live in `assets_local/quest/catalog_v4/`.
The built-in image generator produced the original monochrome chibi atlases
from the user-approved male/female references. Original modern archetypes
avoid franchise names, logos and copied costumes. Literary interpretations
refer to original texts, including [Oz](https://www.gutenberg.org/ebooks/55)
and [Frankenstein](https://www.gutenberg.org/ebooks/84), rather than film designs.
The 50 replacement aura sources are individual transparent PNGs in `auras_v5/`,
with their built-in generation prompts and provenance in
`auras_v5/generation-manifest.json`. They use recognizable cartoon motifs and
consistent outlines; no morphological thinning is applied during export.
The 50 replacement pet sources are individual transparent PNGs in `pets_v5/`,
with exact prompts and provenance in `pets_v5/generation-manifest.json`.
Their stepped pixel contours, expressive eyes and sparse interior detail follow
the approved dog and cat references. Larger pets are drawn with finer source
contours for their final native dimensions. Separate 64-by-80 shop icons are
exported from the original sources rather than enlarging shop art on Hero.

Regenerate with `python tools_local/quest/build_whole_art.py` and
`python tools_local/quest/build_catalog_v4.py`, then run
`python scripts/gen_i18n.py`. Rebuild `CrossNext-Resources.zip` with
`python tools_local/build_resources.py`, then install its contents using the
[SD resource instructions](../sd-resources.md). The older figures/auras/catalog entry points
delegate to these generators. Whole connected silhouettes are extracted from
atlases before fitting, preserving unevenly placed hair, wings, hems and boots.
The alpha mask preserves white interiors and true outside transparency.
Figure and pet canvases are trimmed to the visible silhouette with byte-aligned
widths and end on their ground line. Transparent canvas margins cannot shrink
the artwork or magnify padding beneath a large companion's feet.
The export codec verifies its lossless streams during generation. The resource
packer preserves those pixels in the SD artwork file; rendering streams rows
through reusable buffers and does not allocate a second framebuffer.

Profiles use two checksum-protected RQU5 records in
`/.crosspoint/reading-quest-v5.dat`. RQU1 through RQU4 remain readable.
Migration preserves XP, spent coins, reward bits and gender.
Historical head/body/foot rank `n` purchases unlock complete outfit `2*n`;
old aura rank `n` maps to aura `n/4`. Pets retain their corresponding species;
the retired moon/crystal dragons become the new star-wyrm/ancient-dragon line.
The body determines the equipped outfit. Earlier save files are retained and
are never overwritten by migration. The first successful v5 write begins its
own sequence epoch and is verified before installation.

On a device, check the first and last catalog pages, both genders and both
orientations. Equip the hamster and enlarged cat: faces and silhouettes must
remain visible. Then equip the largest dragons: both hero and pet must remain
complete and share the ground line, with coherent outlines and expected relative
scale. Check shop icons and the Hero illustration after equipping each. Switch
the strongest aura off and on; its old pixels must clear. Restart after a
purchase and after migrating an old profile: progress, gender and equipment
must survive. Power loss during a save must retain the other valid record.


## Progression and rewards

The level cap is 100. A new title appears every five levels: 1–5 Apprentice,
6–10 Curious reader, and so on through 96–100 Deity of stories. Gender changes
only the figure; the title schedule and rewards are shared. The level threshold
is `80*(level-1) + 4*(level-1)^2`: level 2 needs 84 EXP and level 100 needs
47124 total EXP. At the cap the experience bar stays full; further reading and
missions still earn coins.

Reading grants 3 EXP per whole recorded minute, 15 EXP per recorded reading day,
and 120 EXP per completed book. The 15-minute daily mission grants 60 EXP
(6 coins), claimable once per date. The six milestone families retain their
identities, targets and saved claim bits: minutes, pages, sessions, reading days,
books and consecutive-day streaks. Their ordinary EXP rewards start at
10/10/20/20/60/10 respectively and increase by 10 per 100 ranks. Rewards are
one-time claims; daily rewards remain repeatable on later dates.

| Consecutive days | EXP | Extra coins | Coins from EXP + extra |
|---|---:|---:|---:|
| 3 | 40 | 10 | 14 |
| 7 | 80 | 25 | 33 |
| 14 | 120 | 50 | 62 |
| 30 | 200 | 100 | 120 |
| 60 | 300 | 150 | 180 |
| 90 | 400 | 200 | 240 |
| 180 | 600 | 300 | 360 |
| 250 | 800 | 400 | 480 |

A broken streak resets the current streak but does not erase the longest streak,
earned currency, completed milestones or purchases. Streak milestone claims use
that longest record. These are milestones, not a deduction or a requirement to
read every day.

Each level gained awards `20 + 2*newLevel` coins: 24 at level 2, 120 at level 50,
and 220 at level 100. Levels 2–100 together award 12078 coins. Clothing prices
are `15 + floor(requiredLevel*0.9) + floor(index/6)` (starter free); auras and
pets cost `22 + requiredLevel + floor(index/8)`. The entire catalog costs 14283
coins, and the most expensive item is 128. One outfit purchase includes both
figures. The finite catalog can be completed before running out of earned coins;
there is no recurring fee or repeated charge for equipping.

The level-up window appears in Reading Quest after stats refresh or a reward
claim, not during a reader page turn. It shows the new level, current title and
level-gift coins. Several levels gained together produce one summary with the
sum of their gifts. Confirm or Continue acknowledges it. Back can leave without
acknowledging. The notification acknowledgement is saved before dismissal;
a failed save keeps the window with a retryable error. Behind-window controls
are disabled. Coins are derived from total EXP, earned level gifts and claimed
streak bonuses, minus spending; dismissing or replaying a window cannot award
currency twice.

RQU5 records the last acknowledged level in its free bytes and validates spending
against the expanded wallet. Migration keeps old EXP, purchases, spending and
previously granted bonus EXP. Previously earned level gifts and extra streak
coins become available automatically; notifications up to the level represented
by the saved EXP are treated as already seen. Old saves remain as backups. Newly calculated EXP never reduces stored
EXP. The first stats refresh uses the new 3-EXP/minute rate for historical reading.
If that recalculation grants additional levels, it shows one combined level-up
celebration with their coin gifts, even without another reading session.
Acknowledging and restarting does not repeat it.

A deterministic simulation of the real C++ rules, with all available rewards
claimed, one session per reading day, 30 pages/hour and 300-page books, reaches
level 100 in about 17 months at 30 minutes three days/week, 10 months at five
days/week, and 7 months daily. Fifteen minutes five days/week takes about 14
months; 15 minutes three days/week takes longer (about 24 months). Reading
speed, book length and unclaimed rewards change these estimates. The 1500
milestones are an optional long-lived collection, not requirements for level 100.

On hardware, start one minute below a level boundary, read through it and reopen
Reading Quest: the correct title, EXP and coin gift should appear. Continue,
restart and confirm no duplicate gift or popup. Claim a milestone that crosses
several levels and verify the summed gift, then claim a 3/7-day streak milestone
and check both EXP and explicit coins. Test Confirm, touch Continue, Back,
all four rotations, low and high levels, and an SD write failure followed by
retry. Purchases funded by level gifts must survive a restart; old saves must
migrate without changing equipment or removing progress. Hardware verification
is still required before publication.
