# PiPod OS

PiPod OS is a recreation of the classic iPod interface and functionality, optimized for Raspberry Pi devices.

## Features

* iPod-style menu navigation
* Music playback with album artwork
* Settings page for brightness, theme, volume, shuffle, and repeat
* About page with system information

## Tech Stack

* **Language:** C++
* **Target OS:** Raspberry Pi OS Lite
* **Libraries / Dependencies:**

    * SDL2 (Video, Audio, Input)
    * SDL2_ttf (Fonts)
    * SDL2_image (Artwork / Images)
    * SDL2_mixer (Audio Playback)
    * TagLib (Reading MP3 metadata)

## Installation

1. Install dependencies on Raspberry Pi:

   ```bash
   sudo apt update
   sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libtag1-dev
   ```
2. Clone the repository:

   ```bash
   git clone <repo-url>
   cd PiPodOS
   ```
3. Build the project using your preferred C++ compiler (e.g., `g++` or `make`).

## Usage

Run the compiled binary on your Raspberry Pi. Navigate using the arrow keys and `Enter` to select. `Backspace` returns to the previous menu.

## Notes

* Designed for full-screen on small Raspberry Pi displays.
