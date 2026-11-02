# Waveshare 2.8" DPI LCD overlays

Device-tree overlays for the Waveshare 2.8" DPI LCD (480x640, Goodix GT911 touch). They are not part
of Raspberry Pi OS, so the image ships them here and enables them in `config.txt` (see
`os/stage-pipod/02-system/00-run.sh`).

| File | Purpose |
| --- | --- |
| `vc4-kms-dpi-2inch8.dtbo` | KMS/DRM display driver for the panel (options: `rotate`, `rgb565`, ...) |
| `waveshare-touch-28dpi.dtbo` | GT911 touch controller, all Pi models |
| `waveshare-28dpi-3b-4b.dtbo` | DPI pins, backlight GPIO, I2C-GPIO bus |
| `waveshare-28dpi-3b.dtbo`, `waveshare-28dpi-4b.dtbo` | Touch for Pi 3B / 4B (older overlays) |

Source: Waveshare's 28DPI-DTBO package, as linked from
https://mattikjellstadli.com/article/waveshare-lcd-installation-guide
(`28DPI-DTBO.zip`, sha256 `abcf510709b33eb97eadf03c8df42211880106c7309ecfac47000992038c810b`).
