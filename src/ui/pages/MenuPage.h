#pragma once
#include <memory>
#include <vector>
#include "app/AppState.h"
#include "ui/Icons.h"
#include "ui/Utils.h"

// What the right pane shows. Crossfades from `previous` to `current` when it changes.
struct MenuPreview {
    std::shared_ptr<SDL_Texture> current;
    std::shared_ptr<SDL_Texture> previous;
    bool currentIsArt = false;  // album art (square, no shadow tint) vs. white icon
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
void drawMenu(const UI &ui, AppState &state, const std::vector<MenuItem> &items, const MenuPreview &preview);
