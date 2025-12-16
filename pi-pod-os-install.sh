#!/bin/bash
set -e  # exit on any error

# -----------------------------
# 1) Update and install dependencies
# -----------------------------
sudo apt update
sudo apt install -y git build-essential cmake libsdl2-dev libsdl2-ttf-dev \
    libsdl2-image-dev libsdl2-mixer-dev xserver-xorg xinit openbox alsa-utils

# -----------------------------
# 2) Clone and build PiPod OS
# -----------------------------
cd "$HOME"
if [ ! -d "pi-pod-os" ]; then
    git clone https://github.com/mattkje/pi-pod-os.git
fi
cd pi-pod-os

mkdir -p cmake-build-release
cd cmake-build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# -----------------------------
# 3) Install assets and binary
# -----------------------------
sudo mkdir -p /opt/pi-pod-os
sudo cp -r ../assets /opt/pi-pod-os/
sudo cp myos /opt/pi-pod-os/
sudo chmod +x /opt/pi-pod-os/myos
sudo chown -R "$USER:$USER" /opt/pi-pod-os

# -----------------------------
# 4) Enable autologin on tty1
# -----------------------------
sudo mkdir -p /etc/systemd/system/getty@tty1.service.d
sudo tee /etc/systemd/system/getty@tty1.service.d/override.conf > /dev/null <<'EOF'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin pi --noclear %I $TERM
EOF
sudo systemctl daemon-reload
sudo systemctl restart getty@tty1.service

# -----------------------------
# 5) Configure X startup for PiPod OS
# -----------------------------
cat > "$HOME/.xinitrc" <<'EOF'
# Set audio driver
export SDL_AUDIODRIVER=alsa

# Change to app folder
cd /opt/pi-pod-os

# Start minimal window manager in background
openbox-session &

# Start the app and log output
exec ./myos > $HOME/pi-pod.log 2>&1
EOF
chown "$USER:$USER" "$HOME/.xinitrc"
chmod +x "$HOME/.xinitrc"

# -----------------------------
# 6) Start X automatically on tty1
# -----------------------------
grep -q 'startx' "$HOME/.profile" || cat >> "$HOME/.profile" <<'EOF'

# start X on tty1 automatically
if [ -z "$DISPLAY" ] && [ "$(tty)" = "/dev/tty1" ]; then
  exec startx
fi
EOF
chown "$USER:$USER" "$HOME/.profile"

# -----------------------------
# 7) Installation complete
# -----------------------------
echo "Installation complete."

read -p "Do you want to reboot now? [Y/n]: " REBOOT_CHOICE
REBOOT_CHOICE=${REBOOT_CHOICE:-Y}  # default to Y if empty

if [[ "$REBOOT_CHOICE" =~ ^[Yy]$ ]]; then
    echo "Rebooting..."
    sudo reboot
else
    echo "Reboot skipped. You can reboot later with 'sudo reboot'."
fi
