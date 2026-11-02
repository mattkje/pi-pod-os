#!/usr/bin/env bash
# Build and run PiPod OS from a terminal.
#
#   scripts/run.sh debug   [app args...]   Debug build, 320x240 window (no fullscreen)
#   scripts/run.sh release [app args...]   Release build, fullscreen like on the device
#
# Extra arguments are passed to the app, e.g. `scripts/run.sh debug --scale 2`.
set -euo pipefail

cd "$(dirname "$0")/.."

mode="${1:-debug}"
shift || true

case "$mode" in
  debug)   build_dir=cmake-build-debug;   app_args=(--windowed --size 320x240) ;;
  release) build_dir=cmake-build-release; app_args=() ;;
  *) echo "Unknown mode '$mode' (expected debug or release)" >&2; exit 1 ;;
esac

scripts/build.sh "$mode"
exec "$build_dir/pipod-os" "${app_args[@]}" "$@"
