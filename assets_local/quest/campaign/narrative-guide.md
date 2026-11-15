# Campaign, archive and hunts

The King of Forgetting sends monsters to destroy the world's books. The hero
discovers that reading weakens them and sets out for the king's castle.

Camino lists the current saga's opponents. Only the highlighted current card
opens a short illustrated preview and its Fight action. An already started
battle resumes directly. Victory shows the guardian, minutes and rewards.
Completed cards keep their checkmark. Next path returns to the list.

The final guardian in each saga opens a celebration with an exclusive outfit,
background, PET and decoration. The next saga opens on its short introduction
before its first path. The opening is also part of the Saga archive.

Saga selects the complete recap for the latest reached milestone of 0, 3, 6,
9 or 10 victories. Saga I includes the four prologue sections. Defeating the
king unlocks the final Saga recap and the separate epilogue. The Atlas shows
completed locations and defeated guardians.

A guardian unlocks one hunt after its first campaign defeat. A reading session
can advance the hunt and the selected campaign battle. Finished hunts retain
their completion when reading statistics change. Collecting the reward adds a
read-only victory to Recuerdos. Legacy hunt records keep their original
rewards. At most eight unclaimed hunts appear at a time, with the active hunt
first. The Shadow and King hunts use two spare claim bits.

The bilingual `paths.json` contains the current prologue, Camino previews,
Saga versions and epilogue. Generate their translations with
`python tools_local/quest/seed_campaign_content.py` and the i18n generator.
`python tools_local/quest/export_story.py` exports the full manuscript.
Existing battle addresses and artwork are shared across the campaign and hunts.

