# PET and Reading Adventure: save generation 2

The redesigned games start a separate save generation. This is a fixed app-data namespace, independent of the firmware release number and binary save format. Future firmware updates keep using these paths until an explicit new generation is chosen.

| Data | Active location |
| --- | --- |
| PET profile, alternating slots | `/.crosspoint/pet-v2/state-a.bin`, `state-b.bin` |
| PET past lives | `/.crosspoint/pet-v2/lives-a.bin`, `lives-b.bin` |
| Reading Adventure profile | `/.crosspoint/reading-adventure-v2/reading-quest-v10.dat` |
| Reading Adventure staging / rollback | Same folder, `.tmp` and `.previous` |
| Reading Adventure recoverable copy | `/Backups/AventuraLectora-v2/partida-v10.rqu` |

Earlier PET files under `/.crosspoint/pet/`, earlier Reading Adventure files at the root of `/.crosspoint/`, and `/Backups/AventuraLectora/` are neither read nor changed. They are not renamed or deleted. Automatic recovery and the explicit Adventure backup-restore action stay inside generation 2; old backup and format names, when supported, are resolved only inside the new namespace. There is no automatic import across generations.

With no generation-2 save, PET starts at companion creation and Adventure at hero creation / the prologue. Old levels, equipment, currencies, Hunt victories and PET lives do not carry over. Books, reading positions, global reading history and firmware settings keep their existing paths. Each new game uses its existing reading baseline so earlier book-reading totals are not replayed as new game progress. Artwork is independently versioned under `/apps/pet-v2/` and `/apps/reading-adventure-v2/`; installing it does not create another save generation.

This adds no heap allocation, framebuffer, task, persistent field or binary format version. The existing HAL stores own directory creation, journal writes, read-back checks and recovery. The old bytes remain on the SD card; space for one new set of ordinary save files is required.

## Verification

Storage regressions place old profiles, histories and Adventure backup files on a simulated card; both apps must report a fresh game without reading those bytes. They then save and reopen generation-2 data, recover a generation-2 copy, and reject damaged new data without importing generation 1. Old files and a sentinel global reading-history file must stay byte-identical.

On the device, keep a copy of the SD card, install the new firmware, and open both apps. Expect hero creation and a new PET. Create both, read a short session, exit and reopen: the same new progress must return. Repeat after reboot and a later firmware update using this namespace. Adventure’s backup message must name `Backups/AventuraLectora-v2`; restore must recover only a generation-2 backup. Old folders must still have their original bytes. Missing new saves after reopening, old inventory reappearing or changes to reading statistics indicate a failure. Physical storage / power interruption testing remains pending until performed on hardware; a simulator check is not hardware verification.

The Adventure developer simulator seeds current scenarios in the generation-2 directory. The query `?save-generation=previous` instead seeds a former-generation profile and backup, leaving generation 2 empty; the app must show new-game setup. These scenarios use a disposable virtual card.
