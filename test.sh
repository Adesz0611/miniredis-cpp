#!/bin/bash
# Usage: ./test.sh [debug|release|asan|tsan|coverage]   (default: debug)
set -euo pipefail
cd "$(dirname "$0")"
PRESET="${1:-debug}"
./build.sh "$PRESET"
ctest --preset "$PRESET"
