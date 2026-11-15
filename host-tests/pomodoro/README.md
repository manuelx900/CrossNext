# Pomodoro host checks

Run `python3 host-tests/pomodoro/run.py` with a C++20 compiler on Unix, or
`.venv/Scripts/python.exe host-tests/pomodoro/run.py` in the prepared Windows
workspace (Emscripten and Node). No firmware upload is performed.

The core suite protects existing POM1/POM2 timing and migration behavior.
History tests exercise actual HAL-backed persistence against fault-injected
storage, checkpoint retry/recovery, midnight and clock discontinuities, and
calendar projections. Screen tests use the real FreeInkUI builders and bundled
fonts across EN/ES, portrait/landscape and both chrome styles. `--suite` selects
one of `core`, `history`, `screens`; `--render <directory>` emits seeded PBMs.

Hardware refresh quality, clock accuracy, SD throughput and heap/stack behavior
remain device checks described in [Pomodoro](../../docs/pomodoro.md).
