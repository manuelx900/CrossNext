# Settings UI checks

Run `python3 host-tests/settings-redesign/run.py` or
`./host-tests/settings-redesign/run.sh` from the repository root. `CXX` or
`--compiler` selects a compiler; otherwise the runner uses native `c++`, falling
back to the repository's local Emscripten SDK. Node runs Emscripten output.
The suite uses C++20 and the existing language-pack/storage stubs.

Pass `--build-dir <directory>` to retain the executable. An existing directory
passed as a positional argument receives PBM previews. `--headings-only
<directory>` runs the section-heading checks. `EM_CACHE` is respected.

Checks cover rendered geometry, hit routing, translated help, preview space,
large alignment controls and focused dialog contrast. A partially visible list
may request an entirely clipped icon; pixel checks verify that hidden images
paint nothing and intersecting images paint only inside the clip. Normal
controls still require bitmap bounds to fit their clip.

These are host checks; they do not flash hardware or measure physical refresh
latency.
