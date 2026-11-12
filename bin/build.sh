#!/bin/sh
# Configure (if needed) and build in Release mode.

set -e
cd "$(dirname "$0")/.."

if [ ! -f build/CMakeCache.txt ]; then
  sh bin/rebuild.sh
  exit 0
fi

cmake --build build --config Release -j"$(nproc)"
