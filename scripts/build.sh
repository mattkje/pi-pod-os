#!/usr/bin/env bash
# Configure and build PiPod OS.
#
#   scripts/build.sh debug     -> cmake-build-debug/pipod-os
#   scripts/build.sh release   -> cmake-build-release/pipod-os
set -euo pipefail

cd "$(dirname "$0")/.."

# IDEs launched from the Dock don't inherit the shell PATH; make Homebrew tools visible.
for p in /opt/homebrew/bin /usr/local/bin; do
  [[ -d "$p" && ":$PATH:" != *":$p:"* ]] && PATH="$p:$PATH"
done
export PATH

case "${1:-debug}" in
  debug)   build_type=Debug;   build_dir=cmake-build-debug ;;
  release) build_type=Release; build_dir=cmake-build-release ;;
  *) echo "Unknown mode '$1' (expected debug or release)" >&2; exit 1 ;;
esac

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type"
cmake --build "$build_dir" -j "$jobs"
