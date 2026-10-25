#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
"${PYTHON:-python3}" host-tests/simulatorweb/test_simulator_web.py
