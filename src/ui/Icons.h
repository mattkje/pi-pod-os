#pragma once
#include <SDL2/SDL.h>
#include <array>
#include <string>
#include "app/AppState.h"

// Large glyphs shown in the main menu's right pane, one per menu entry.
//
// Each icon is loaded from <assets>/icons/menu/<name>.png when that file exists (see fileName()),
// so they can be swapped without recompiling. Missing files fall back to a built-in drawing,
// rendered once at high resolution and scaled down with linear filtering. The built-in Extras
// icon is a live clock; ship an extras.png to replace it with a static image.
class MenuIcons {
public:
    MenuIcons(SDL_Renderer *renderer, const std::string &assetsDir);
    ~MenuIcons();

    MenuIcons(const MenuIcons &) = delete;
    MenuIcons &operator=(const MenuIcons &) = delete;

    // Returns the icon texture (white on transparent). The clock is redrawn when the minute changes.
    SDL_Texture *get(MenuIcon icon);

    static constexpr int SIZE = 256;

    // File name (without directory) used for an icon, e.g. "now-playing.png".
    static const char *fileName(MenuIcon icon);

    // Writes the built-in drawings as PNGs into dir. Returns the number of files written.
    static int exportBuiltIn(SDL_Renderer *renderer, const std::string &dir);

private:
    SDL_Texture *createTarget();
    void render(MenuIcon icon, SDL_Texture *target);

    SDL_Renderer *r;
    std::array<SDL_Texture *, static_cast<size_t>(MenuIcon::Count)> textures{};
    std::array<bool, static_cast<size_t>(MenuIcon::Count)> fromFile{};
    int clockMinute = -1;
};
