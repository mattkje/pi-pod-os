#pragma once
#include <memory>
#include <string>
#include <vector>
#include "app/AppState.h"
#include "ui/Icons.h"
#include "ui/Utils.h"

// What the right pane shows. Crossfades from `previous` to `current` when it changes.
struct MenuPreview {
    std::shared_ptr<SDL_Texture> current;
    std::shared_ptr<SDL_Texture> previous;
    bool currentIsArt = false;  // album art (fills the pane) vs. icon
    std::string caption;        // e.g. "No Music", shown under the icon; empty for none
    bool previousIsArt = false;
    Uint32 changedAt = 0;

    void set(std::shared_ptr<SDL_Texture> tex, bool isArt) {
        if (tex == current) return;
        previous = current;
        previousIsArt = currentIsArt;
        current = std::move(tex);
        currentIsArt = isArt;
        changedAt = SDL_GetTicks();
    }

    void reset() {
        current.reset();
        previous.reset();
    }
};

// Main menu: list on the left half, blue pane with icon / album art on the right half.
// The title bar shows the device name chosen in Setup (falls back to "PiPod").
void drawMenu(const UI &ui, AppState &state, const std::vector<MenuItem> &items, const MenuPreview &preview);
