# Hearts

The shelf's first trick-taking game. Jaipur and Sea Salt & Paper are set
collection, Solitaire is a patience; nothing here played tricks, which is the
largest family in cards and the reason this got built rather than poker.

Four seats, one human, three opponents. No link play: the radio seats two, and
two humans plus two brains is not a shape the DS would have shipped.

## The rules, exactly

Standard American Hearts. Thirteen cards each, play goes clockwise, and on this
panel clockwise means South to West to North to East, which is also "the player
on your left".

**Every heart costs one point and the queen of spades costs thirteen**, so
exactly 26 leave the table every hand. **Lowest score wins** and the game ends
the moment anybody reaches 100.

**The two of clubs leads the first trick**, always, and nothing else may open.
**Follow the led suit if you hold it**; otherwise play whatever you like. The
**highest card of the led suit** takes the trick and leads the next one -- an
off-suit card cannot win however high it is.

Two rules catch every new player, and they are the two the HOW TO PLAY screen
spends a page on:

- **You may not LEAD a heart** until one has been discarded on another suit
  ("broken"), unless hearts are the whole of your hand.
- **Nothing that costs a point may be played on the first trick** -- no heart
  and no queen -- unless you hold nothing else, which a hand of twelve hearts
  plus the queen genuinely can.

**Before each hand you pass three cards**, and the direction rotates left,
right, across, then a hand with no pass at all. The pass is simultaneous: every
seat's three cards are lifted before any are dealt in, because seat by seat a
card passed left can be picked up again by the next seat's own pass and a hand
ends up with fourteen.

**Shooting the moon**: take all 26 and you score nothing while everyone else
scores 26. There is no "subtract 26 instead" option. One rule, not a choice.

No Jack of Diamonds variant, deliberately: it is one more rule to explain and it
is not standard everywhere.

## The three layers

| Layer    | File                 | Knows about                        |
| -------- | -------------------- | ---------------------------------- |
| Deck     | `cards/Cards.h`      | nothing -- freestanding C++17      |
| Rules    | `HeartsCore.h`       | the deck, and nothing else         |
| Opponent | `HeartsBrain.h`      | the rules, through an Observation  |
| Screens  | `HeartsScreens.h`    | FreeInkUI, Toybox tokens, `cards/` |
| Activity | `HeartsActivity.cpp` | renderer, storage, input, shelf    |

The first four are host-tested with no device: `host-tests/hearts/run.sh` for
the rules and the opponent, `host-tests/ui/run.sh` for the screens.

## The deck is shared, and it is numbered ace-low

`src/apps_local/cards/` holds the encoding, the generated suit art and the card
face. Solitaire was migrated onto it in the same branch, and `solitaire::Suit`
and its card helpers are now aliases of `cards::` -- one Suit in the fork, so a
card drawn by the shared face is the same type as a card compared by Klondike's
rules.

**The deck numbers the ace 0 and the king 12**, because Klondike builds
foundations upward from the ace and never compares two cards for height. Hearts
does nothing else, and under that numbering the ace is the lowest number in the
deck: `a > b` on a raw rank makes the ace lose every trick it enters, silently,
in the one operation the whole game is made of.

**Hearts converts, in `trickRank()`, and the deck stays as it is.** Bending a
shipped game's core encoding to suit a new one is how both end up wrong. Any
future game that ranks cards does the same. A test asserts the ace beats the
king in all four suits; collapsing `trickRank` to `rankOf` turns sixteen checks
red.

## The opponent takes an Observation, never a Game

The same rule `SeaSaltBrain.h` sets out, and it matters more here because
**three of the four seats are brains running in one process**. One brain that
could see all four hands would turn a game of incomplete information into a
solved one while every test still passed. Each seat gets its own Observation,
rebuilt per decision from what that seat is entitled to know: its cards, the
table, the cards already played, and who has shown void in what. All of that is
public at a real table.

What has been played is **recorded** by the rules, not derived from who is not
holding it. The derivation is only correct while all 52 cards are accounted for,
and it read the queen as already fallen in a hand-built test position.

No search. Hearts rewards bookkeeping rather than depth: duck under the trick,
remember who is void, do not hand anyone the queen, notice a moon. Those are
rules over a value function, which is what this is.

**Strength is a head-to-head win rate, not a pooled average.** A Sharp rotated
through all four seats against three Rookies, fresh seed per game: **39.4% of
decided games against 25% by chance**, finishing on 66.3 against 78.5. The suite
prints both numbers on every run.

Three choices are available for the next game. **Easy** uses the Rookie policy: follow suit, play low and discard high when void. **Expert** uses Sharp: track played cards and void suits, stop a moon and attempt one when the public facts justify it. **Normal** chooses between those policies on each decision. None can inspect another hand.

The moon is **hard to enter on purpose**: already holding every point taken, at
least eight of them, and still holding cards nothing outstanding can beat. A
shoot that fails hands over everything collected on the way.

## The table

Scores sit down the left, the current trick occupies the centre and your hand runs along the bottom. During the pass, the selected cards appear above your hand beside Pass. Legal cards receive a double outline when Highlight valid cards is enabled. Other cards retain their normal face. A refused tap names the rule that refused it.

See [Card games](../card-games.md) for settings, 50/100-point limits and the current layouts.

## The save

The unchanged `Game` payload plus active difficulty/limit bytes, written at every trick boundary and on the way out. Version 2 accepts version 1 saves with their previous skill setting and a 100-point limit. A
trick boundary is thirteen writes a hand rather than one per card; Solitaire
wrote ~340 bytes on every tap, 150+ times a session, and this struct is bigger.
Restoring mid-trick is perfectly sound -- the struct carries `turn`, the table
and every hand.

**A version byte and a length prove nothing about a torn write.** Losing power
during the save leaves a file of exactly the right size holding a mix of two
states, which passes both gates and then restores a seat index of 5 or a hand of
twenty cards: an out-of-bounds read on the next repaint and an out-of-bounds
write the first time that hand is tapped. `hearts::isConsistent()` asks the
rules whether the position is possible at all, and the check that catches nearly
everything is that the deck adds up to 52 distinct cards across the hands and
what has been played.

## What is not verified

**Hardware.** Every render of this app so far is the simulator. The pacing in
particular is untested on a panel that takes a third of a second to settle: a
trick holds for 900ms before it sweeps so the fourth card is actually seen, and
a brain waits 420ms before playing, and both numbers were chosen by looking at a
desktop window.
