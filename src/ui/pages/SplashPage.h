#pragma once
#include "ui/Utils.h"

// Black screen with the PiPod logo, used at startup (matching the Plymouth boot splash), while
// updating and when restarting or shutting down.
//
// progress: -1 hides the bar, otherwise 0..1 draws an Apple-style progress bar under the logo.
// alpha fades the whole screen (used to cross-fade into the menu after startup).
void drawSplash(const UI &ui, SDL_Texture *logo, float progress = -1.0f, Uint8 alpha = 255);

// Logo size as a fraction of the screen height. The Plymouth theme uses the same value so the
// hand-over from the boot splash to the app is seamless.
constexpr float SPLASH_LOGO_SCALE = 0.35f;
