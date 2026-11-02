#!/bin/bash -e
id pipod >/dev/null
chown -R pipod:pipod /opt/pi-pod-os
install -d -o pipod -g pipod /home/pipod/Music
systemctl enable pi-pod-os.service
# Boot splash (rebuilds the initramfs so the logo shows as early as possible).
plymouth-set-default-theme -R pipod
# The shell owns tty1.
systemctl disable getty@tty1.service || true
