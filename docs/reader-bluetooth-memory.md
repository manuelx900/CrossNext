# EPUB indexing with Bluetooth

On X4 Pro, the reader keeps a running Bluetooth radio enabled during foreground
chapter indexing, incremental index extension and text-format recomposition.
Physical link continuity and device speed still require device verification. Radio startup and
teardown must still settle before a chapter build. The PSRAM-enabled S3 profile
does not stop a running radio to make room and then risk failing to restart it
in a fragmented internal heap. Devices without the S3 PSRAM profile retain their
existing stop-before-build policy.

Both EPUB engines give Expat a PSRAM-only memory suite. This includes its small
tag/attribute allocations, resize operations and parsing buffers. The layout
word arena's existing 2 KiB text chunks also use PSRAM-only ownership. Neither
path falls back to internal RAM on allocation failure. Expat reports an error;
WordStore preserves its checked failure and retries after requesting reclamation
from the PSRAM pool, even if internal memory is plentiful. No
additional live buffer, task or framebuffer is introduced. Allocation count and
text algorithms are unchanged; capacity moves from internal RAM to PSRAM, and
all blocks are freed at their existing parser/paragraph lifetime boundaries.

The S3 reader may continue its existing bounded background index work beside a
stable radio: two pages or eight milliseconds between parser steps, with the
existing heap checks. A complex callback can exceed the time slice. Starting or
stopping Bluetooth still prevents that work. Wi-Fi, exclusive USB storage,
sleep and the idle timeout retain their existing radio policies.

Layout caches, event replay, fonts, AA, images, CSS, word breaking and generated
page formats are unchanged. This change does not add a lower-quality mode or
replace memory limits with optimistic thresholds. Actual PSRAM exhaustion can
still prevent a page from being prepared; it does not request a Bluetooth
disconnect. Radio range, remote battery, interference and external device
disconnects remain independent of this memory policy.

## Verification

Run `host-tests/reader-bluetooth-memory/run.py` with `CROSSNEXT_GTEST` pointing to
the pinned GoogleTest `googletest` directory. It tests real Expat and WordStore
against the production HAL with PSRAM capability checks, each Expat allocation
failure point, resize failure ownership and 100 repeated word-arena lifecycles.
The fake-radio suite checks 100 index/paint/page-input cycles without another
stack start or stop, including a low-internal-memory condition. It also runs
the legacy radio runtime, profile policies and both chapter-gate configurations.
These tests establish ownership and policy; host timings do not establish X4 Pro speed.

Run the existing chapter-layout, reader-indexing and reader-format tests for
exact text/layout, framebuffer-loan and format/reposition behavior. Build the
X4 Pro release after the final substantive change.

On the device, compare the prior and candidate firmware with the same SD card,
EPUB, engine, font, AA, position and paired device. Test cold chapter caches,
warm caches, long single-paragraph chapters, embedded CSS, images and repeated
font changes. Cross chapter boundaries using both the paired device and local
buttons. Bluetooth must stay connected during indexing; subsequent remote
presses must still turn pages without pairing again. Compare page-turn and
indexing time over repeated trials; a reproducible speed regression is a
failure. Record internal/PSRAM free bytes and largest blocks before opening,
through builds and after closing; repeated cycles must recover memory. Verify
Wi-Fi/USB exclusivity, reconnect after leaving those modes, sleep/wake, Home and
both orientations. Back up progress before using a separate SD copy to test
cold caches; never erase all of `/.crosspoint/` for this comparison.
