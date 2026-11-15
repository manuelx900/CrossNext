# Translation meaning and review

Check an interface string against its call site and neighbouring copy before
translating it. The English YAML now includes context comments for the recurring
ambiguities below. Keep English as the source and preserve format arguments.

| Context | Meaning | Wrong interpretation to avoid |
| --- | --- | --- |
| PET Cold | A fictional companion's illness | Low temperature |
| PET Free | Cleaning costs no resources | Liberty or an empty slot |
| PET hop | A small jump | Brewing hops |
| PET supplies | Food and remedies earned by reading | Reading utensils or legal remedies |
| PET first affection | A completed interaction with the pet, remembered in its photo album | Raising, feeding, a future instruction or a romantic partner |
| Quest Claimed | Reward collected | Assertion, accusation or legal claim |
| Quest sage outfit | A wise person | The herb |
| Keyboard Shift | Change letter case | Work shift or movement |
| Tasks Left | Tasks remaining | Left-hand direction |
| Solitaire draw | Take cards from the draw pile | Drawing a picture or a tied score |
| Solitaire stock / waste | New-card pile / used-card pile | Shares, soup, inventory or rubbish |
| Solitaire foundations | Final ace-to-king card piles | Building construction |
| Solitaire free card | Card available to move | A free-of-charge card |
| Hearts suit / led | Card symbol / first suit played | Clothing, lawsuits or LED lights |
| Hearts trick | Four cards, one from each player | Deception or a helpful tip |
| Toy Battle draw | Take troop pieces from reserve | Playing cards, drawing or a tied score |
| Habits streak | Consecutive daily completions | Victories, hair, weather or a line |
| Words review / passage | Practise a word / book excerpt | A book critique or a corridor |
| Clock DST Auto | Apply daylight-saving time automatically | An automobile |
| Saving / Saved | Writing data / data already written | Saving money, rescuing someone, or a command while a save is running |
| PET retire and save | End a virtual life and keep its record in the album | Rescue the companion instead of ending this life |
| Study Left / lives left / reading left | Remaining count, lives, or reading time | Left-hand direction or left-wing politics |
| Quest Claim +60 XP | Collect earned experience | Price, requirements, search, or download |
| Quest sage ranks | A wise character | The herb, abstract wisdom, or postal letters |
| Study review / reviews safe | Spaced-repetition practice / preserved study results | Published critiques or personal opinions |
| Resume | Continue a reader/game session | CV or a summary |
| Apply (Wordle/settings) | Apply the selected option | Apply for a job |
| Font Regular / Bold | Normal / bold font weight | Frequent, permanent or brave |
| PET mood, energy, food | Three companion resource counters | Money, hair or fabric lining |
| PET stroke | Gently pet the fictional animal | Feeding, romance or sexual activity |
| PET practice | Free practice after rewarded games; does not unlock the game memory | Profession, software application or a reversed unlock rule |
| Study deck | Collection of flashcards | Ship deck, bridge or construction |
| Study sync Back | Leave this screen while background sync continues | Stop or cancel the sync |
| Study picker cancellation | Deck selection unchanged | No previous sync changes or acknowledgements committed |
| Study download cancellation | Download stopped; retry sync | All deck files or reviews unchanged |
| Battleship board | Fleet grid | Governing board, council or committee |
| Study graph | Previous two weeks, today and upcoming due cards | Two days ago or financial debt |
| Solitaire stock | Remaining card pile | Financial capital recovering |

Study's legacy count formats receive an English `S` suffix as their last `%s`
argument. External translations use a count label and `%.0s` when that suffix
does not belong in the language. The precision consumes the existing string
argument without printing it; preserve its position and all numeric arguments.
Do not append the English suffix to a translated noun. English and Spanish
keep their existing formats. A future pluralization change should replace the
caller and every affected format together.

The two meanings of draw in card games and Toy Battle need separate translations.
Do not replace a word globally. A natural card term may itself have another
meaning outside the game: German *Stich* and Russian *взятка* are valid card terms.

Rule explanations must retain actors, conditions, exceptions, numbers and units.
For example, Hearts can be led once **any** player discards a heart, or when the
player holds only hearts. Do not change this to waiting for the same player to
discard. PET's food thresholds and fullness limits must retain every number.
In Hearts, a trick contains four cards in total, one per player. Each heart is
worth one point, the queen of spades thirteen, and all other cards zero. Use
the local playing-card suit names; literal translations can turn spades into
shovels, swords or minerals. Portuguese *espadas* is a valid suit name.

Avoid adding playing-card Unicode symbols to labels without checking font
coverage. These games draw their card symbols as graphics; a working icon does
not establish that the same symbol exists in a text font.

Validate catalogues, generate packs, and check the actual screen with the real
fonts. Structural checks and reverse translation can flag problems; neither
certifies native linguistic accuracy. Machine drafts require editorial review.
Preserve placeholders, proper names and user content. Ook intentionally retains
its English fallback.

Run `python tools_local/i18n/audit_context.py --output <report-folder>` for every
catalogue's key, duplicate, Unicode, format and known-context regression checks.
Add `--packs <languages-folder>` to export only the active external `.cplang`
files. This does not copy the unchanged fonts, keyboards or Wordle dictionaries.
`context-regressions.json` records the original and revised text and the intended
meaning for each correction. Its rejected original values are checked per locale
and key, not replaced globally. New editorial wording is allowed.

The report distinguishes structural failures, corrected active and inactive
entries, and heuristic candidates requiring review. A clean structural report
does not certify all prose. Native editorial review is still needed for nuanced
wording and long game narratives; screen fitting also needs real-font checks.

## Sources

The [KPatience rules](https://docs.kde.org/trunk_kf6/en/kpat/kpat/rules_and_tips.html)
describe the distinct roles of the stock, waste and foundation piles. The
firmware's own Hearts and Toy Battle rules are the authority for their actual
actions; source comments name those contexts. This is a software-localization
task; academic or medical sources do not establish the game's wording.
