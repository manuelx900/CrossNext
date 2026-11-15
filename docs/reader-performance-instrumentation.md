# Reader latency and exact image cache writes

The X4 Pro development profile enables `CROSSPOINT_PERFORMANCE_TRACE`. Release profiles keep the instrumentation as
inline no-ops. The collector uses one fixed aggregate, no heap buffer or task.
It records operation ownership so an old render or an unrelated screen cannot
complete a newer measurement.

`TRACE` operations cover navigation from its request through activity exit,
entry and final render; EPUB page turns; and text adjustments from the latest
input. Held adjustments coalesce until input release, without a fixed 450 ms delay. Intermediate text-panel updates and recoverable page-load retries do not
complete the format operation. Asynchronous overlay completion is recorded
when the existing input-tick or settle path waits for the waveform. Rejected page turns do
not create operations; successful turns retain their pre-lock input timestamp. Stages distinguish lifecycle,
fonts, book loading, layout, page loading, extraction, decoding and drawing.
Storage totals count actual returned bytes and calls through `HalFile`, with
separate mutex-wait and read/write time. Durations are wall time; nested stages
and concurrent storage totals must not be summed as CPU time. The operation ends
after render returns, including progress persistence; the draw-stage end gives
an earlier software boundary for the completed page presentation.

Capture a single boot's serial log, then run:

```bash
python tools_local/performance_trace.py capture.log trace.json
```

Open `trace.json` in Perfetto. The command also prints sample count, median,
nearest-rank P95 and maximum per operation kind. Use separate files for each
workload/card/book/settings combination. Superseded operations are incomplete
and excluded. Missing log lines can make a trace incomplete; keep the serial
consumer running and inspect the existing dropped-log counter. Instrumentation
adds timing/formatting overhead: compare with the release configuration on the
same device before claiming a speedup.

The image pixel cache accumulates decoder output in its existing band and writes
contiguous final rows together. JPEG MCU bands, PNG scanlines and GIF rows keep
the same coordinate, clipping, zero-fill and 2-bit packing rules. The `.pxc`
format is unchanged. A synthetic 480 by 800 output takes 52 HAL writes instead
of 802 with one-row or sixteen-row input steps. This is an operation count,
not an assertion about physical SD transactions or device latency.

EPUB reflow can retain one raw event-cache stream of up to 256 KiB in PSRAM
during a reader session. A parser owns the buffer while replaying; a lease keeps
nested parsers on the ordinary file path. The buffer fills only as ordinary replay reads progress, avoiding an eager full
chapter read before the first page. Its completed prefix transfers to one memo
on release, never to a second live snapshot. Uncached suffixes still stream from SD. Admission leaves 1 MiB PSRAM headroom.
The path, file size, modification time and validated source header must match;
missing timestamps, pressure, oversized chapters and allocation failure stream
normally. CRC and record validation still execute on memory-backed blocks.
Reader exit releases retention. Rewrites invalidate the matching memo. This
retains XML events, not a framebuffer or a font-dependent pagination checkpoint.

Changing font or size still composes exact pages up to the reading anchor when
no compatible layout exists. Offset-targeted construction now stops after a
one-page parse increment instead of an eight-page increment. The parser may
emit several pages in one XML step. No styles, line-breaking, font metrics,
image resolution, antialiasing, waveform or cache format change is made.

Run `python host-tests/reader-performance/run.py` and
`python host-tests/reader-speed/run.py`. Compare baseline/candidate image cache
bytes and BW/gray planes; exercise gaps, clipping, upscaling, cancellation,
short writes and close failure. Replay checks cover a header-only warm read,
nested parsers, memory pressure and the existing malformed-stream cases.

On X4 Pro, compare cold/warm opening, isolated and rapid font changes at early,
middle and late chapter positions, image-first visits/returns, Home and Library
round trips. Use the same card, resources and all visual settings. Record 30
warm samples initially and expand the tail-latency comparison as needed. Check
all orientations, inversion, progress, sleep/resume, repeated entry/exit and
internal/PSRAM heap. Expect unchanged text, page breaks and gray planes; panel
quality and actual latency require device measurements.


See [Home and EPUB work reuse](home-epub-speed.md) for the subsequent Shelf composition, summary publication, native cover and parser input/prefix work reuse. It keeps the same image and text quality contracts.
