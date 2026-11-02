#include "ui/pages/SplashPage.h"
#include <algorithm>
#include <cmath>

namespace {
    // Filled rounded rectangle (pill when radius = h / 2), drawn line by line.
    void fillRounded(SDL_Renderer *r, float x, float y, float w, float h, SDL_Color c) {
        if (w <= 0 || h <= 0) return;
        float rad = std::min(h / 2, w / 2);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        int rows = static_cast<int>(std::ceil(h));
        for (int i = 0; i < rows; i++) {
            float py = i + 0.5f;
            float inset = 0;
            if (py < rad) inset = rad - std::sqrt(std::max(0.0f, rad * rad - (rad - py) * (rad - py)));
            else if (py > h - rad) inset = rad - std::sqrt(std::max(0.0f, rad * rad - (py - (h - rad)) * (py - (h - rad))));
            SDL_RenderDrawLineF(r, x + inset, y + i, x + w - inset, y + i);
        }
    }
}

void drawSplash(const UI &ui, SDL_Texture *logo, float progress, Uint8 alpha) {
    SDL_Renderer *r = ui.r;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, alpha);
    SDL_Rect all{0, 0, ui.w, ui.h};
    SDL_RenderFillRect(r, &all);

    int size = static_cast<int>(ui.h * SPLASH_LOGO_SCALE);
    SDL_Rect dst{(ui.w - size) / 2, (ui.h - size) / 2, size, size};
    if (logo) {
        SDL_SetTextureAlphaMod(logo, alpha);
        SDL_RenderCopy(r, logo, nullptr, &dst);
        SDL_SetTextureAlphaMod(logo, 255);
    }

    if (progress < 0) return;
    // Thin rounded bar centred under the logo: dark grey track, white fill.
    float barW = ui.w * 0.36f;
    float barH = std::max(3.0f, ui.h / 80.0f);
    float barX = (ui.w - barW) / 2.0f;
    float barY = dst.y + size + ui.h * 0.09f;
    fillRounded(r, barX, barY, barW, barH, {70, 70, 70, alpha});
    float p = std::clamp(progress, 0.0f, 1.0f);
    if (p > 0) fillRounded(r, barX, barY, std::max(barH, barW * p), barH, {255, 255, 255, alpha});
}
