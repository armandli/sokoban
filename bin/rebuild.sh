#!/bin/sh
# Clean configure + build. Picks the newest available GCC (project targets C++26).

set -e
cd "$(dirname "$0")/.."

if [ -z "$CXX" ]; then
  for c in g++-15 g++-14 g++; do
    if command -v "$c" >/dev/null 2>&1; then CXX=$c; break; fi
  done
  CC=$(echo "$CXX" | sed 's/g++/gcc/')
  export CXX CC
fi

rm -rf build 2>/dev/null
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(nproc)"
