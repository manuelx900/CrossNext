# CrossNext app documentation

The current app and game list is defined by `src/apps_local/Shelf.cpp`.
This index includes current guides and inherited technical documents.
An inherited document does not add its app to the X4 Pro shelf.

## Current user guides

| Guide | Contents |
| --- | --- |
| [PET](pet.md) | Companion care, home decoration, memories and microSD contents. |
| [Wordle](wordle.md) | Word lengths, saved games, streaks and statistics. |
| [Chess](chess.md) | Screens and navigation. |
| [Checkers](checkers.md) | English draughts and compulsory captures. |
| [Connect Four](connectfour.md) | Board controls and game flow. |
| [Minesweeper](minesweeper.md) | Revealing and flagging cells. |
| [Sudoku](sudoku.md) | Puzzle generation and difficulty grading. |
| [Hearts](hearts.md) | Rules, scoring and shared card handling. |
| [Murdle](murdle.md) | Logic grids, clues and generated cases. |
| [Picross](picross.md) | Puzzle data and generation. |

For installation, the current app list and resource downloads, start with
the [project README](../../README.md). The [PET artwork pipeline](pet-artwork.md)
describes how to rebuild its microSD packs.

## File formats and compatibility

| Reference | Contents |
| --- | --- |
| [Study deck format](study-deck-format.md) | Deck files and card representation. |
| [Anki compatibility](study-anki-compatibility.md) | Supported conversions and limitations. |
| [Trivia pack format](trivia-pack-format.md) | Question banks. |
| [Wikipedia pack format](wikipedia-pack-format.md) | Offline encyclopedia data. |
| [xkcd pack format](xkcd-pack-format.md) | Packed comic images and metadata. |

## Inherited components and design records

These documents describe upstream implementations, earlier workflows or
components that are not registered in this release. Hosted services mentioned
in them belong to upstream; CrossNext does not operate those services.

| Reference | Contents |
| --- | --- |
| [Study](study.md), [sync flow](study-syncflow-ui.md) | Inherited Study app and bridge workflow. |
| [Instapaper](instapaper.md) | Read-later queue and bridge protocol. |
| [Trivia](trivia.md), [sync design](trivia-sync-design.md), [curation](../trivia-curation.md) | Question bank and synchronization. |
| [Hacker News saved articles](hackernews-saved-port.md) | Saved-article implementation. |
| [Wallpapers](wallpapers-phone-flow.md), [shuffle](wallpapers-shuffle.md) | Wallpaper sources and selection. |
| [Yahtzee](yahtzee.md), [Knucklebones](knucklebones.md), [Sea Salt & Paper](seasalt.md) | Rules and implementation choices. |
| [Toy Battle](toybattle.md), [Wavelength](wavelength.md), [Forehead](forehead.md) | Board and group-game designs. |
| [Underhand](underhand.md) | Rules, source provenance and permission records. |
| [Dungeon](dungeon.md), [Guess Who](guesswho.md) | Historical or unimplemented designs. |

The inherited [Study quick reference](study-quick-reference.pdf) is a printable
reference for that workflow, rather than the current Flashcards app.
