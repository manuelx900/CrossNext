# X4 Pro radio startup

This change addresses the 2.0 reports of Bluetooth startup failure, an
unreachable local File Transfer server, and Plugin HUB installation failure.
Some affected users can still use OPDS and CrossPoint Sync. Those reports do
not establish a general failure to join Wi-Fi.

## What changed

- `CrossPointWebServer::begin()` checks the underlying Arduino HTTP listener
  after `begin()`. Socket creation, bind or listen failure leaves the service
  stopped; the File Transfer activity cannot display a working-server QR for
  that failed attempt. HTTP and WebSocket owner allocations are fallible and
  checked. Request handling, ports, uploads and Wi-Fi credentials are unchanged.
- `FontAllocation` places dynamically allocated `SdCardFont` and `TtfEpdFont`
  objects in PSRAM on PSRAM boards. The existing owners and destructors still
  control their lifetime. These objects are smaller than the S3 allocator's
  4096-byte internal-memory preference, so ordinary allocation kept their
  metadata in scarce internal RAM even when glyph buffers used PSRAM. No new
  allocation or cache is added. PSRAM allocation failure returns null and keeps
  the existing font fallback; it never consumes internal RAM as a fallback.
  Boards without PSRAM retain ordinary nothrow allocation.
- Michi's immutable pattern and Zobrist lookup tables live in flash instead of
  writable static RAM. Their combined size is 21,664 bytes on X4 Pro. The
  generator uses the original C builders and compares all entries against the
  compiled constant tables. Game rules, random-number state and saves are
  unchanged. Regenerate the tables when their builders or definitions change.
- Bluetooth exposes its last startup policy refusal. A memory refusal uses the
  existing translated memory-error message instead of the generic startup
  error. A new attempt clears the previous refusal. Startup thresholds and
  Bluetooth/Wi-Fi exclusion remain in place.
- Plugin HUB also downloads `DETAILS.md`, listed by the upstream 0.1.16 package
  manifest. All five files are staged before replacement. Failure leaves the
  existing incomplete-install marker behavior intact; cancellation is not
  recorded as a radio failure.

These changes recover internal memory and fix independently reproducible
startup/installation defects. They do not prove the cause of every physical
radio failure. Successful outgoing requests also cannot prove that an inbound
HTTP listener opened successfully.

## Diagnostics without USB

Explicit Bluetooth startup, hotspot startup, HTTP startup and Plugin HUB
installation failures append a bounded technical log to
`/.crosspoint/radio-diagnostics.txt` through HalStorage. There is no background
writer or new task. The file rotates at 4096 bytes. Each line includes firmware
version, uptime, operation, numeric result and internal/PSRAM heap statistics.
It contains no SSID, address, credential, URL or book name. No file is written
when these operations succeed. A missing/unwritable card cannot produce a log.

For `bluetooth-start`, `code` is `bleturner::Why`: `7` means an exclusive Wi-Fi
or storage operation, `9` insufficient free internal memory, and `10` an
insufficient contiguous block. `0` means no policy refusal was recorded; the
attempt still failed or was cancelled, for example inside the SDK. For
`http-listen`, `code` is socket `errno` and `detail` is the port. For
`pluginhub-install`, they are the download result and last HTTP status; file
errors may retain the last successful response status.

## Checks and device verification

Host checks:

```sh
./host-tests/radio-services/run.sh
python3 tools_local/go/generate_tables.py --check
./host-tests/go/run.sh
```

Run the BlePageTurner CMake suites and reader-font suites as well. On Windows
without a native host C++ compiler, the radio fixture and table generator accept
`--emsdk <emsdk-directory>` for Emscripten/Node. Host tests exercise refusal,
cleanup and retry paths; they cannot test the RF controller or a phone's routing.
The final firmware target is `gh_release_x4pro`.

On X4 Pro, after installing the test firmware:

1. Reboot, enable Bluetooth, scan and pair a supported page-turning remote.
   Open a book and check forward/backward page turns. Repeat after using Wi-Fi.
2. With Bluetooth off, open File Transfer and create the CrossNext hotspot.
   Join it from a phone, stay connected despite the no-Internet warning, open
   the displayed HTTP address, and upload/download a small file. Keep the
   reader on the File Transfer screen.
3. Repeat File Transfer using Join Network on the same router as the phone.
   Verify both the numeric HTTP address and discovery from a compatible app.
4. Install Plugin HUB from the reader, open it, and confirm its details page.
   Verify OPDS and CrossPoint Sync still work separately.
5. Repeat after reading and browsing image-heavy books. Open Go and check that
   a saved game resumes. Check SD and vector fonts, including a missing font's
   normal fallback.

If a step fails, turn the reader off and use a card reader to copy
`/.crosspoint/radio-diagnostics.txt`. Record the failed step and whether the
hotspot appeared. Physical validation is required before publishing a release.
