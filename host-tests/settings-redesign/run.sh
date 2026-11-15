#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
# run.py compiles test_screens.cpp with its current I18n dependencies.
if [ -n "${PYTHON:-}" ]; then
  exec "$PYTHON" host-tests/settings-redesign/run.py "$@"
elif [ -x .venv/Scripts/python.exe ]; then
  exec .venv/Scripts/python.exe host-tests/settings-redesign/run.py "$@"
else
  exec python3 host-tests/settings-redesign/run.py "$@"
fi
