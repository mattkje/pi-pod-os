#!/usr/bin/env bash
# Package a built pipod-os into the release layout used by pipod-update and the OS image:
#
#   pi-pod-os-<version>-<arch>/{pipod-os, assets/, VERSION}
#   pi-pod-os-<version>-<arch>.tar.gz (+ .sha256)
#
#   scripts/package.sh <build-dir> <out-dir> [arch]
set -euo pipefail

build_dir="${1:?build dir}"
out_dir="${2:?output dir}"
arch="${3:-$(dpkg --print-architecture 2>/dev/null || uname -m)}"

version="$("$build_dir/pipod-os" --version)"
name="pi-pod-os-$version-$arch"

mkdir -p "$out_dir"
rm -rf "${out_dir:?}/$name"
mkdir -p "$out_dir/$name"
cp "$build_dir/pipod-os" "$out_dir/$name/"
cp -R "$build_dir/assets" "$out_dir/$name/"
echo "$version" > "$out_dir/$name/VERSION"

tar -C "$out_dir" -czf "$out_dir/$name.tar.gz" "$name"
(cd "$out_dir" && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")
echo "$out_dir/$name.tar.gz"
