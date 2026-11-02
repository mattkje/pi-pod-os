# Promo assets

Marketing icons for PiPod OS (not used by the OS itself, which uses `assets/icons/logo.svg`).

## Main icon: classic

- `pipod-icon-classic.svg` – glossy iOS 6 style: orange gradient, curved gloss highlight, flat white
  PiPod logo. Hand-written SVG, so it can be edited directly.
- `exports/pipod-icon-classic-1024.png` – 1024×1024 render.

```bash
rsvg-convert -w 1024 -h 1024 promo/pipod-icon-classic.svg -o promo/exports/pipod-icon-classic-1024.png
```

## Variant: Icon Composer

- `PiPod.icon` – the classic icon rebuilt as an Icon Composer document from three layers taken from
  `pipod-icon-classic.svg` (background gradient, gloss, logo). Glass, shadow and specular are off so
  it keeps the classic look; Icon Composer adds the current icon shape and edge highlight.
  Open it with `open promo/PiPod.icon` to tweak. If you change the classic SVG, the layer files in
  `PiPod.icon/Assets/` need the same change.
- `exports/pipod-icon-glass-1024.png` – 1024×1024 render. (No dark version: Icon Composer's automatic
  dark mode recolours the white logo to the background colour, which hides it.)

```bash
ictool=/Applications/Icon\ Composer.app/Contents/Executables/ictool
"$ictool" promo/PiPod.icon --export-image --output-file promo/exports/pipod-icon-glass-1024.png \
  --platform macOS --rendition Default --width 1024 --height 1024 --scale 1
```
