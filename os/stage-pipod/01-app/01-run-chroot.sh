#!/bin/bash -e
# Build PiPod OS inside the image, install it as the first release and remove the toolchain again.
BUILD_DEPS="build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev \
libsdl2-mixer-dev libtag-dev libcurl4-openssl-dev nlohmann-json3-dev libcpp-httplib-dev \
libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libtinyxml2-dev"

apt-get update
# shellcheck disable=SC2086
apt-get install -y --no-install-recommends $BUILD_DEPS

src=/tmp/pi-pod-os-src
# A release build passes its version in .pipod-version (see os/build-image.sh).
version_arg=""
[ -f "$src/.pipod-version" ] && version_arg="-DPIPOD_VERSION=$(cat "$src/.pipod-version")"
# shellcheck disable=SC2086
cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release $version_arg
cmake --build "$src/build" -j "$(nproc)"
"$src/scripts/package.sh" "$src/build" "$src/dist" "$(dpkg --print-architecture)"

version="$("$src/build/pipod-os" --version)"
mkdir -p /opt/pi-pod-os/releases
rm -rf "/opt/pi-pod-os/releases/$version"
tar -xzf "$src"/dist/pi-pod-os-*.tar.gz -C /opt/pi-pod-os/releases
mv "/opt/pi-pod-os/releases/pi-pod-os-$version-$(dpkg --print-architecture)" "/opt/pi-pod-os/releases/$version"
ln -sfn "releases/$version" /opt/pi-pod-os/current

# Keep the shared libraries the binary needs, then drop the build toolchain.
libs=$(ldd /opt/pi-pod-os/current/pipod-os | awk '/=> \//{print $3}')
pkgs=$(for l in $libs; do dpkg -S "$(readlink -f "$l")" 2>/dev/null || dpkg -S "$l" 2>/dev/null; done \
	| cut -d: -f1 | sort -u)
# shellcheck disable=SC2086
apt-mark manual $pkgs
# shellcheck disable=SC2086
apt-get purge -y $BUILD_DEPS
apt-get autoremove -y --purge
apt-get clean
rm -rf "$src"

# Fail the build if anything the app needs went missing.
if ldd /opt/pi-pod-os/current/pipod-os | grep -q "not found"; then
	ldd /opt/pi-pod-os/current/pipod-os
	exit 1
fi
