# Checkers

The shelf opens `DraughtsActivity`: four offline rulesets, a computer opponent or two people sharing one device. The retired English two-device game, its UI, rules/search wrappers and network entry are removed. The remaining engine is `DraughtsCore`; no Checkers screen starts a radio or link session.

| Variant | Board / pieces per side | Men capture | Capture choice | Kings | Promotion |
| --- | --- | --- | --- | --- | --- |
| Spanish | 8×8 / 12 | Forward | Most pieces, then most kings | Flying | Reaching the last row ends the turn |
| Brazilian | 8×8 / 12 | Forward and backward | Most pieces | Flying | Must finish on the last row |
| International | 10×10 / 20 | Forward and backward | Most pieces | Flying | Must finish on the last row |
| English | 8×8 / 12 | Forward | Any complete chain | One square | Reaching the last row ends the turn |

All men move one diagonal square forward. Capturing is mandatory. The illustrated help follows these actual engine rules: Spanish, Brazilian, International and English. Portrait shows all four concepts per ruleset on four pages; landscape uses two concepts per page across eight pages. Each lesson uses a complete one-bit illustration extracted from the supplied redesign; it is scaled with Contain within its lesson rather than cropped. Its previous/next controls display the current and total page count. Settings groups variant, opponent, level and your colour on one screen. There is no Apply button: Back to menu and header/physical Back save the selected preferences. A running match retains the rules, opponent, level and colour with which it began until New game. With Black selected against the computer, White opens and Black appears at the bottom. The menu names the previewed match's
ruleset, size, opponent and level. During a running match its buttons are
Continue, New game, Settings and Help. New game asks before replacing the match;
Cancel returns to the menu with that match intact. The board also offers Menu
and New game with the same confirmation.

A finished match opens a separate result. You won / You lost is relative to the selected colour, with a separate sentence naming the winning side. White and Black each have victory and defeat illustrations extracted from the supplied redesigns. Draws are identified as draws. View board preserves the terminal position and rejects moves and machine turns; Back returns to the result. Board squares use the same light-gray one-bit dither as Chess.

`/.crosspoint/draughts.cfg` is still eight bytes. Version 2 adds the colour byte and includes it in the checksum; version 1 remains readable with White as the default. Unknown future versions stay read-only. The rules and search engine are unchanged. See [the refinement profile](../study-suite.md#checkers-variants) for generator, draw and search constraints. Run `host-tests/draughts/run.sh` for engine regressions and `host-tests/go-draughts-pieces-redesign/run.sh` for terminal guards, orientation, help pagination and touch targets. Device timing, input and orientation checks remain separate from host rendering.
