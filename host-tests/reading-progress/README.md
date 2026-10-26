# Combined reading and position sync

Run from the repository root:

```sh
python host-tests/reading-progress/run.py --emxx /path/to/em++
python host-tests/reading-cloud/run.py --emxx /path/to/em++
python host-tests/reading-cloud/run_http.py --emxx /path/to/em++
python host-tests/reading-cloud/run_server.py --server-root /path/to/crosspoint-sync --emxx /path/to/em++
```

The position suite runs the production card scanner, state machine, JSON handling, progress comparison and progress-file writes. Storage, document hashes and EPUB/XTC pagination use deterministic fixtures. It covers the reported 5% cloud / 7% local case, remote advances, more than ten books, nested directories, EPUB/XTC/XTCH/TXT/Markdown, previously unopened books with missing cache directories, both document identifiers, repeated sync, every request interruption in a two-book run, a lost upload acknowledgement, cancellation, invalid responses, truncated saves, short writes, directory read failures and saves changed during sync. A separate test runs the production document-ID reader with a recording hash primitive to verify exact sampled bytes and safe handling of negative/short reads and failed seeks.

The official-server harness also runs the production HTTP client over real loopback requests with a temporary account/database. It synchronizes fifteen book positions, advances a book offline, restores a later remote position and checks that statistics remain unchanged by position transfers. It does not replace physical X4 Pro testing or establish TLS, power-loss or screen-refresh behavior. See [the verification steps](../../docs/reading-stats.md#sync-verification).
