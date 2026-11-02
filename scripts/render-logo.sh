#!/usr/bin/env bash
# Render assets/icons/logo.svg into the PNGs used by the app splash and the Plymouth boot theme.
# Needs rsvg-convert (macOS: brew install librsvg, Debian: apt install librsvg2-bin).
set -euo pipefail
cd "$(dirname "$0")/.."
rsvg-convert -w 512 -h 512 -b none assets/icons/logo.svg -o assets/icons/logo.png
cp assets/icons/logo.png os/rootfs/usr/share/plymouth/themes/pipod/logo.png
echo "Updated assets/icons/logo.png and the boot splash logo."
