#!/bin/bash
# Usage: ./build.sh [debug|release|asan|tsan|coverage]   (default: debug)
set -euo pipefail
cd "$(dirname "$0")"
PRESET="${1:-debug}"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" -j
