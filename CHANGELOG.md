# Changelog

## 1.0.2

### Fixes
- Videos showed red and blue swapped (skin looked blue) on the Pi: SDL's OpenGL ES renderer swaps
  them when it draws a YUV texture into the rotated screen. Frames are now converted to RGB with
  FFmpeg, using the video's own colour matrix and range.
- Without a usable sound output (e.g. a Pi Zero with only HDMI audio and no HDMI screen), songs stayed
  at 0:00 and videos never started. Playback now continues silently on SDL's dummy output.

## 1.0.1

### Fixes
- Waveshare 2.8" DPI LCD (and other KMS/DRM screens): the app showed a black or white screen because
  SDL picked its desktop OpenGL renderer; it now uses OpenGL ES, and the image keeps the EGL/GLES
  libraries the display needs (the build checks for them).
- Screen rotation and menu icons use a texture format every renderer supports.
- The boot logo is turned and sized like the app's on portrait panels, so it is no longer sideways.
- The shell service keeps restarting instead of giving up after repeated failures.

### Music
- More audio formats: M4A (AAC and Apple Lossless), AAC, AIFF and WMA, played through FFmpeg when
  SDL_mixer can't decode a file, alongside MP3, FLAC, WAV, Ogg Vorbis and Opus. The upload page accepts them.

## 1.0.0

First release.

### System
- Flashable image based on Raspberry Pi OS Lite (Debian trixie, 64-bit), built with pi-gen (`os/build-image.sh`).
- Boots straight into PiPod OS: no desktop or login prompt; the app draws directly to the display (KMS/DRM).
- Boot splash with the PiPod logo that hands over seamlessly to the app.
- The 320x240 interface scales to any screen (HDMI); optional Adafruit PiTFT 2.8" overlay in `config.txt`.
- First-boot Setup Assistant: language, name (hostname), password, time zone, Wi-Fi, Jellyfin.
- Restart and Shut Down from Settings.
- Over-the-air updates from GitHub releases with download progress and `pipod-update rollback`.

### Interface
- iPod classic (6th/7th gen) look: white lists, split-screen main menu with icons and album art,
  Now Playing with tilted cover, reflection, shuffle/repeat status and progress bar.
- About page with storage bar and device details; Settings with a status pane.
- Translations: English, Norsk, Svenska, Dansk, Deutsch, Français, Español.
- Helvetica-style typography like the iPod classic, using the free TeX Gyre Heros font.

### Videos, photos, podcasts
- Videos from `~/Videos` and Jellyfin (movies and shows, transcoded to H.264 at up to 640x480),
  played with FFmpeg; hardware H.264 decoding on the Pi, pause, ±10 s seek, overlay with progress.
- Photos from `~/Pictures`: thumbnail grid (thumbnails cached), full-screen viewer, slideshow.
- Podcasts: RSS subscriptions managed on the web page, cached for offline browsing; episodes download
  and play in Now Playing.

### Music
- Local library from `~/Music` (mp3, flac, ogg, opus, wav) with cover art.
- Jellyfin login and streaming (songs are cached before playing).
- Web page to upload and delete songs from another device, protected by an access code.
- Shuffle, repeat (off / one / all), volume.

### Development
- IntelliJ run configurations for debug windows, device mode, Setup Assistant, release and image builds.
- `--windowed`, `--size`, `--scale`, `--setup`, `--native` options; F12 saves a screenshot.
