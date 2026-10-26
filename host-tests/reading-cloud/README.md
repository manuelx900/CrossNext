# Reading statistics synchronization tests

Run from the repository root after generating translations and preparing the simulator dependencies:

```sh
python host-tests/reading-cloud/run.py --emxx /path/to/em++
python host-tests/reading-cloud/run_http.py --emxx /path/to/em++
```

These execute the production C++ snapshot store, parser, cumulative uploader, offline projection and screen code under Node. HAL stubs replace storage/PSRAM. Tests cover repeat uploads, a lost acknowledgement, interrupted download, unchanged local journal bytes, account-origin replacement, journal rollback/corruption, PSRAM failure, more than twenty dates, completed/unfinished paging, and English/Spanish touch bounds in portrait and landscape. The sync shortcut is checked both with and without a configured account.

The same Sync action also handles book positions. Run `python host-tests/reading-progress/run.py` for its scanner/state-machine tests. The official-server harness below includes the position flow after the statistics flow; EPUB/XTC pagination uses deterministic fixtures there.

The HTTP harness uses curl over loopback and substitutes the SDK transport boundary; it does not simulate device TLS. For integration against the official server, prepare a separate checkout of `crosspoint-reader/crosspoint-sync`, install its dependencies with `npm ci --ignore-scripts`, then run:

```sh
python host-tests/reading-cloud/run_server.py --server-root /path/to/crosspoint-sync --emxx /path/to/em++
```

It starts that server on a loopback port with a temporary SQLite database and synthetic test credentials. The production client uploads local counters and imports the combined result. Expected progression: 300 seconds already in the account plus 120 local = 420; thirty more offline seconds = 450 before and after repeated sync. The database and server process are removed afterward. Node >=22.13, curl, Emscripten and the simulator ArduinoJson dependency are required. No real account or device data is used.

Protocol reference: [CrossPoint Sync statistics API](https://github.com/crosspoint-reader/crosspoint-sync/blob/main/docs/API.md#reading-stats). Hardware verification is documented in [the user guide](../../docs/reading-stats.md#sync-verification).
