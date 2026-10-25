# UI coherence checks

Run `host-tests/coherence/run.sh` with a C++17 compiler. An optional output
folder receives PBM renders of the production screen builders with the firmware's
pixel and prose glyphs. This is a host render, not a device or browser screenshot.

Checks exercise both orientations and English/Spanish text, task paging and
original item indices after pending/completed grouping, free-task controls,
calendar taps in every month of representative leap/non-leap/boundary years,
and flashcard content/control bounds. Flashcard card text itself is drawn by
StudyActivity; the builder captures show its frame and controls. Its wrapping,
pane alignment and clipping are covered separately by host-tests/study.
