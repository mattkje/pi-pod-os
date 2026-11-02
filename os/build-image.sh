#!/usr/bin/env bash
# Build a flashable PiPod OS image (Raspberry Pi OS Lite + PiPod OS) with pi-gen in Docker.
#
#   os/build-image.sh            full build -> os/deploy/image_<date>-pipod-os.img.xz
#   os/build-image.sh --continue rebuild only the PiPod stage on top of the previous build
#
# PIPOD_VERSION=1.2.3 os/build-image.sh  builds the app with that version (used by the release
# workflow so the image reports the tag's version); otherwise the version in CMakeLists.txt.
#
# Needs Docker. On Apple Silicon (arm64) the build runs natively; elsewhere pi-gen uses qemu.
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"
PIGEN_REF="${PIGEN_REF:-2026-10-06-raspios-trixie-arm64}"  # pi-gen release tag (arm64 branch)
PIGEN_DIR="$ROOT/os/.pi-gen"
CONTINUE=0
[ "${1:-}" = "--continue" ] && CONTINUE=1

if [ ! -d "$PIGEN_DIR/.git" ]; then
  git clone https://github.com/RPi-Distro/pi-gen.git "$PIGEN_DIR"
fi
git -C "$PIGEN_DIR" fetch --tags --quiet
git -C "$PIGEN_DIR" checkout --quiet --force "$PIGEN_REF"
git -C "$PIGEN_DIR" clean -fdq -e work -e deploy

# Our stage, with the app sources and rootfs overlay as stage files.
rm -rf "$PIGEN_DIR/stage-pipod"
cp -R "$ROOT/os/stage-pipod" "$PIGEN_DIR/stage-pipod"
mkdir -p "$PIGEN_DIR/stage-pipod/01-app/files/src" "$PIGEN_DIR/stage-pipod/02-system/files"
git -C "$ROOT" ls-files -co --exclude-standard -z \
  | rsync -a --from0 --files-from=- "$ROOT/" "$PIGEN_DIR/stage-pipod/01-app/files/src/"
rm -rf "$PIGEN_DIR/stage-pipod/01-app/files/src/os/.pi-gen"
if [ -n "${PIPOD_VERSION:-}" ]; then
  echo "$PIPOD_VERSION" > "$PIGEN_DIR/stage-pipod/01-app/files/src/.pipod-version"
fi
cp -R "$ROOT/os/rootfs" "$PIGEN_DIR/stage-pipod/02-system/files/rootfs"

# Only export the final image.
touch "$PIGEN_DIR/stage2/SKIP_IMAGES"
cp "$ROOT/os/config" "$PIGEN_DIR/config"

cd "$PIGEN_DIR"
if [ "$CONTINUE" = 1 ]; then
  # Reuse the stopped container from the last build; skip the Raspberry Pi OS Lite stages.
  touch stage0/SKIP stage1/SKIP stage2/SKIP
  CONTINUE=1 PRESERVE_CONTAINER=1 ./build-docker.sh
else
  rm -f stage0/SKIP stage1/SKIP stage2/SKIP
  docker rm -v pigen_work >/dev/null 2>&1 || true
  PRESERVE_CONTAINER=1 ./build-docker.sh
fi

mkdir -p "$ROOT/os/deploy"
cp deploy/*.img* "$ROOT/os/deploy/" 2>/dev/null || true
ls -lh "$ROOT/os/deploy"
