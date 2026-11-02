#!/bin/bash
# Install PiPod OS on an existing Raspberry Pi OS Lite system.
#
# Prefer the ready-made image (see README); this script is for systems that are already set up.
# It builds the app, installs it into /opt/pi-pod-os the same way the image does and replaces the
# tty1 login prompt with PiPod OS for the current user.
set -e  # exit on any error

# The script configures the shell for the invoking user and calls sudo itself.
if [ "$EUID" -eq 0 ]; then
    echo "Run this script as your normal user (without sudo): scripts/install.sh" >&2
    exit 1
fi

cd "$(dirname "$0")/.."
SRC="$PWD"
ROOT=/opt/pi-pod-os

# -----------------------------
# 1) Dependencies
# -----------------------------
sudo apt update
sudo apt install -y git build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev \
    libsdl2-image-dev libsdl2-mixer-dev libtag-dev libcurl4-openssl-dev nlohmann-json3-dev libcpp-httplib-dev avahi-daemon \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libtinyxml2-dev \
    alsa-utils curl python3 ca-certificates plymouth

# -----------------------------
# 2) Build and package
# -----------------------------
cmake -S "$SRC" -B "$SRC/cmake-build-release" -DCMAKE_BUILD_TYPE=Release
cmake --build "$SRC/cmake-build-release" -j"$(nproc)"
PKG="$("$SRC/scripts/package.sh" "$SRC/cmake-build-release" "$SRC/dist")"
VERSION="$("$SRC/cmake-build-release/pipod-os" --version)"

# -----------------------------
# 3) Install into /opt/pi-pod-os/releases/<version>
# -----------------------------
sudo mkdir -p "$ROOT/releases"
sudo rm -rf "$ROOT/releases/$VERSION"
sudo mkdir -p "$ROOT/releases/$VERSION"
sudo tar -xzf "$PKG" -C "$ROOT/releases/$VERSION" --strip-components=1
sudo ln -sfn "releases/$VERSION" "$ROOT/current"
sudo chown -R "$USER:$USER" "$ROOT"
mkdir -p "$HOME/Music"

# -----------------------------
# 4) System files (service, updater, power permissions) for this user
# -----------------------------
sudo install -m 755 "$SRC/os/rootfs/usr/local/bin/pipod-update" /usr/local/bin/pipod-update
sudo install -m 755 "$SRC/os/rootfs/usr/local/bin/pipod-system" /usr/local/bin/pipod-system
sudo install -m 644 "$SRC/os/rootfs/etc/default/pi-pod-os" /etc/default/pi-pod-os
sed "s/^User=pipod$/User=$USER/" "$SRC/os/rootfs/etc/systemd/system/pi-pod-os.service" \
    | sudo tee /etc/systemd/system/pi-pod-os.service > /dev/null
sed "s/^pipod /$USER /" "$SRC/os/rootfs/etc/sudoers.d/pi-pod-os" \
    | sudo tee /etc/sudoers.d/pi-pod-os > /dev/null
sudo chmod 440 /etc/sudoers.d/pi-pod-os
sudo visudo -cf /etc/sudoers.d/pi-pod-os

# Waveshare 2.8" DPI LCD overlays (enable them in config.txt, see below)
sudo cp "$SRC"/os/rootfs/boot/firmware/overlays/*.dtbo /boot/firmware/overlays/

# Boot splash theme (shown when "splash" is on the kernel command line)
sudo cp -R "$SRC/os/rootfs/usr/share/plymouth/themes/pipod" /usr/share/plymouth/themes/
sudo plymouth-set-default-theme -R pipod || true

sudo systemctl daemon-reload
sudo systemctl disable getty@tty1.service || true
sudo systemctl enable pi-pod-os.service

# -----------------------------
# 5) Installation complete
# -----------------------------
echo "PiPod OS $VERSION installed. It starts on tty1 after a reboot."
echo "For the logo while booting, add 'quiet splash' to /boot/firmware/cmdline.txt."
echo "For the Waveshare 2.8\" DPI LCD, add to /boot/firmware/config.txt:"
echo "  dtoverlay=waveshare-28dpi-3b-4b"
echo "  dtoverlay=waveshare-28dpi-3b"
echo "  dtoverlay=waveshare-28dpi-4b"
echo "  dtoverlay=waveshare-touch-28dpi"
echo "  dtoverlay=vc4-kms-dpi-2inch8"

read -p "Do you want to reboot now? [Y/n]: " REBOOT_CHOICE
REBOOT_CHOICE=${REBOOT_CHOICE:-Y}  # default to Y if empty

if [[ "$REBOOT_CHOICE" =~ ^[Yy]$ ]]; then
    echo "Rebooting..."
    sudo reboot
else
    echo "Reboot skipped. Start it now with 'sudo systemctl start pi-pod-os' or reboot later."
fi
