# Reading Adventure: progression balance

The original campaign target is **150–180 hours**. The current host simulations
record campaign-only and combined Hunt strategies separately; results depend
on sessions and purchases. They are model estimates, not device measurements.
Every encounter is individual and surplus reading never banks a future win.

## Levels and titles

The level ceiling is **150**. Every level adds **2 power**. All 30 hero titles cover five levels each, including ten new bilingual titles from level 101 onward. The final title is Eternal legend at levels 146–150.

Let F(L) = 80 × (L−1) + 4 × (L−1)². Cumulative EXP is F(L) through level 100, then F(100) + floor((F(L)−F(100)) × 4/5). Level 100 still requires 47,124 EXP; level 150 requires **90,004 EXP**. The final fifty levels are easier to reach. Experience and reading continue to accumulate at the cap.

Base power is 10 + 2 × (level−1), plus equipment and background affinity. Enemies have fixed reference resistance and do not scale with the hero. Additional power has diminishing returns. Campaign base budgets are rounded up after a 5% adjustment.

Duration is ceiling(base minutes × (100 + reference power) / (100 + hero power)), subject to a minimum of 40% of base time in the campaign, 45% for ordinary Hunts and 75% for Secret Hunts. Hunts also cap the result at their base minutes. The final King of Oblivion is an exception: scale and round the penultimate battle first, then double that result, so the final goal is exactly twice as long with the same level and equipment. The campaign estimate uses this same rule and assumes normal improvements in future encounters.

## Hunt rewards and equipment

Unique outfits, decorations and other creature rewards remain. Hunt victory EXP uses the target saved when the Hunt is accepted. Coin rewards retain baseMinutes/4 + 8, plus fortune at collection. Reading alone grants no EXP. Each selected attempt pays one fixed victory prize: 3 EXP per required minute, plus the equipment EXP bonus capped at 30%. Extra session minutes remain in reading statistics but cannot increase this prize or prepay unselected campaign paths. Daily missions grant 10 EXP and weekly missions grant 40.

Hunt outfits specialize in bonuses without granting overwhelming early power. Basic outfits give 10 + regional tier power, +2% EXP and +4% coins; Elite 14 + tier, +4% EXP and +6% coins; Legendary 20 + tier, +6% EXP and +10% coins; Secret 100 power, +10% EXP and +15% coins. The Ink relic gives 35 power, +8% EXP and +12% coins. Equipment caps remain +30% victory EXP and +25% victory coins. Ordinary campaign paths unlock their objects in the shop; they do not award them for free. Completing a saga still grants its four exclusive objects. Hunt loot remains a direct reward. Previously owned equipment and earned EXP are retained.

Reading advances the active campaign encounter and accepted Hunt simultaneously. Hunt acceptance saves its target; leveling up or changing equipment does not move that target. Imported active and paused Hunts keep their saved minutes. The three letter-attack phases, manual result navigation and unique rewards are retained.

| Rank | Indicative base minutes |
| --- | ---: |
| Basic | 30–105 |
| Elite | 180–290; final guards up to 300 |
| Legendary | 300–480 |
| Secret | 1500–1800; at least 75% of base |

## Repeat Hunts

Unlocked Hunts can be played indefinitely, including after the campaign and all 134 unique creatures. Available Hunts favors unseen eligible creatures, then fills the four offers with rematches. Keepsakes show a cumulative ×N win count and offer Hunt again. Every attempt begins at a fresh reading cursor and retains a fixed target; the Ink dragon can still pause and resume. Opening Rewards only displays the prizes and returns to the same encounter/report. Collecting a completed attempt pays EXP and coins once; the outfit and other unique objects are retained once, without repeated outfit offers. Keepsake dates and reading describe the latest victory.

Imported profiles without counters infer one win from each existing unique claim. There is no retroactive EXP or coin grant. uint32 counters saturate at their maximum while further attempts remain playable. Restart clears the adventure's counters with its other progress. Older firmware's newer-QBAL guard preserves these saves rather than overwriting them.

## Shop through level 150

Purchase gates span levels 1–150. General decorations unlock at 1, 50, 100 and 150;
ordinary level-gated outfits, scenes and pets spread their former 1–100 gates
across this range. Campaign-path purchases keep their path requirement and use
that path's recommended level for pricing. The shop shows unowned items as they
unlock through their required level or path. Saga and Hunt-exclusive prizes
are awarded outside the shop.

For purchase tier L, outfit price is 40 + 8L + floor(L²/8) coins. Scenes cost
85%, pets 90% and general decorations 110%, rounded up. A level-1 outfit costs
48 coins; a level-150 outfit costs 4052. Prices progress by purchase tier within
each category, not by internal asset identifier. Previously owned items, their
equipment bonuses, EXP, spending and victory rewards are unchanged. Shop price
tiers are deliberately separate from combat equipment-stat tiers.

There is **no minute reserve**. Each campaign battle and Hunt attempt begins
with its own reading cursor. For a 30-minute objective in a 60-minute session,
the surplus remains in global reading statistics and does not advance a later
attempt. Reading can still advance an already-active campaign and an accepted
Hunt simultaneously, as before.

## Reproducible checks

Run python host-tests/quest-campaign/run.py. Its progression suite covers 21 campaigns with 15-, 30- and 60-minute daily sessions. The combined routines stop after winning all **134 unique Hunts**, without farming rematches, and check that level 150 remains reachable. The EXP curve is unchanged. The final King now requires twice the penultimate battle's scaled and rounded target; session length still affects how much reading falls beyond the selected enemy's objective. The level cap remains reachable through victories and optional missions.

The balance suite checks all 150 levels, threshold boundaries, 120 resistance curves, fixed Hunt targets, Secret floors and older save import/journal recovery. The UI suite checks title changes and title/name fit in English and Spanish in both orientations. Rebuild browser fixtures with python tools_local/quest/build_simulator_fixture.py; its catalogue and matching controls expose every level from 1 through 150.

## Compatibility and resources

RQUA/v10 keeps the **8192-byte** record and existing profile layout. The QBAL extension revision is **5**. Reserved bytes 7124–7659 hold 134 little-endian uint32 Hunt win counters. Reserved bytes 7118–7121 hold two pending outfit try-out offers; revision 3 milestone flags remain at byte 7116. Revision 1 profiles accept their original acknowledged level through 1000, then clamp the displayed level to 150. EXP and spending are retained. If the lower cap reduces already-earned level coins, the difference is credited to the existing extraCoins field once during import; saving revision 2 prevents repeated credit. A former level 101–150 may rise under the gentler curve. CRC-valid newer QBAL revisions block fallback, saving over that journal, including mixed compatible/newer slots. Damaged-CRC records retain normal journal recovery.

Older formats, owned equipment, favorites, victory records and pending campaign rewards retain their existing import behavior. Active and paused Hunt goals are preserved. Two bounded outfit-offer identifiers use four existing reserved on-card bytes. No task, image resource or framebuffer is added. Host Profile is 7192 bytes and StoreWork is 15392 bytes. Counters add 536 bytes per profile; the activity retains two profiles and one StoreWork candidate, adding 1608 bytes in total. There is no per-attempt allocation or larger on-card record.

Before firmware publication, test both device orientations, reaching levels 100/101/150, unchanged active Hunt goals, reward collection and interrupted-journal recovery on a disposable card. Import an older revision-1 profile above level 150 and confirm its wallet survives repeated saves. This delivery prepares a simulator and does not establish physical-device behavior.
