#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
exec "${PYTHON:-python3}" host-tests/reader-next/run.py
