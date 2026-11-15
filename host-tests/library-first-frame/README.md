# Library first-frame checks

Run `python host-tests/library-first-frame/render.py`. Uses native C++ on Unix
or the existing Emscripten SDK and Node on Windows. Output: actual FreeInkUI
component renders in portrait/landscape and English/Spanish, and checks against
the activity's production `buildGrid`, `render`, `runIdleWork` and completion
methods. HAL, parser, panel and image sources use deterministic host adapters.

The checks require a panel presentation before index or cover I/O, require the
cover notice before thumbnail opens, exercise full/partial native prefetch, resident scenes and missing-cover
completion, and compare all 101 charge levels and both charging states against
the original carousel battery pixels in both polarities. They do not measure
hardware latency or simulate decoder/SD/display timing.

For a historical pixel comparison, add `--baseline /path/to/pre-change/LibraryListActivity.cpp`.
This compiles the old grid method alongside the candidate and compares every
final framebuffer byte with identical host image sources. Without that option,
the historical comparison is explicitly skipped; the behavioral checks run.
