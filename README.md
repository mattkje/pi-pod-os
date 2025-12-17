# PiPod OS

PiPod OS is a recreation of the classic iPod interface and functionality for Raspberry Pi devices.

## Status
**Pre-MVP**
Only basic functionality is implemented. The system is not yet usable.

## Features

* iPod-style menu navigation
* Music playback with album artwork
* Optional support for jellyfin music library
* More TBD...

## Tech Stack

* **Language:** C++
* **Target OS:** Raspberry Pi OS Lite
* * Should work on anything, but goal is for this app to serve as the OS frontend, hence Pi OS Lite being the main target
* **Libraries / Dependencies:**

    * SDL2 (Video, Audio, Input)
    * SDL2_ttf (Fonts)
    * SDL2_image (Artwork / Images)
    * SDL2_mixer (Audio Playback)
    * TagLib (Reading MP3 metadata)

## Installation

1. Install **PiPod OS** on a fresh Raspberry Pi OS Lite system by running the installer script:

   ```bash
   git clone https://github.com/mattkje/pi-pod-os.git
   cd pi-pod-os
   sudo ./pi-pod-os-install.sh

## Usage

Navigate using the arrow keys and `Enter` to select. `Backspace` returns to the previous menu.
