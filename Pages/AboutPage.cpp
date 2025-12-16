#include "AboutPage.h"
#include "../Utils.h"

void drawAboutPage(SDL_Renderer *r, TTF_Font *font, int winWidth) {
    drawTopBar(r, font, "About", winWidth);
    int y = 32 + 24;

    auto drawCentered = [&](const std::string &text) {
        SDL_Texture *t = renderText(r, font, text, {40, 40, 40, 255});
        int w, h;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        SDL_Rect dst{(winWidth - w) / 2, y, w, h};
        SDL_RenderCopy(r, t, nullptr, &dst);
        SDL_DestroyTexture(t);
        y += h + 10;
    };

    auto drawSeparator = [&]() {
        int thickness = 2;
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_Rect line{20, y, winWidth - 40, thickness};
        SDL_RenderFillRect(r, &line);
        y += thickness + 10;
    };

    // App Info
    drawCentered("Raspberry Pi 3B+");
    drawCentered("PiPod OS 0.0.1");
    drawSeparator();

    // Firmware
    drawCentered("Firmware 1A543");
    drawSeparator();

    // Credits
    drawCentered("© 2025 Shelstad Studios");
    drawCentered("Developer: Matti Kjellstadli");
    drawCentered("Website: https://mattikjellstadli.com");
    drawSeparator();

    // License or extra info
    drawCentered("All rights reserved.");
}
