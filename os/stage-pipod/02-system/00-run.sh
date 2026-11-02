#!/bin/bash -e
# System integration: service, updater, power permissions, quiet boot.
cp -a files/rootfs/. "${ROOTFS_DIR}/"
chmod 440 "${ROOTFS_DIR}/etc/sudoers.d/pi-pod-os"
chmod 755 "${ROOTFS_DIR}/usr/local/bin/pipod-update" "${ROOTFS_DIR}/usr/local/bin/pipod-system"
# The overlays README is documentation for the repo, not needed on the boot partition.
rm -f "${ROOTFS_DIR}/boot/firmware/overlays/README-waveshare.md"

BOOT="${ROOTFS_DIR}/boot/firmware"

# Quiet boot: no rainbow splash, no kernel text or cursor; Plymouth shows the PiPod logo.
# The PiPod block is always last in config.txt: drop an old one (rebuilds) and append the current.
sed -i '/^# PiPod OS$/,$d' "${BOOT}/config.txt"
{
	cat <<'CFG'

# PiPod OS
[all]
disable_splash=1

# Waveshare 2.8" DPI LCD (480x640). Overlays are shipped in overlays/ (README-waveshare.md).
# Comment these lines out when using only HDMI or another screen.
dtoverlay=waveshare-28dpi-3b-4b
dtoverlay=waveshare-28dpi-3b
dtoverlay=waveshare-28dpi-4b
dtoverlay=waveshare-touch-28dpi
dtoverlay=vc4-kms-dpi-2inch8

# Adafruit PiTFT 2.8" (resistive), alternative small screen: comment out the Waveshare lines above
# and uncomment this. The drm option makes it a KMS display that the app can draw on.
#dtoverlay=pitft28-resistive,rotate=90,speed=64000000,fps=30,drm
CFG
} >> "${BOOT}/config.txt"
if ! grep -q "logo.nologo" "${BOOT}/cmdline.txt"; then
	sed -i -e 's/ console=tty1//' \
		-e 's/$/ console=tty3 quiet splash plymouth.ignore-serial-consoles loglevel=3 logo.nologo vt.global_cursor_default=0 consoleblank=0/' \
		"${BOOT}/cmdline.txt"
fi
