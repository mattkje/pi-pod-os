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
    drawCentered("PiPod Classic");
    drawCentered("PiPod OS 0.0.1");
    drawCentered("Firmware 1A543");
    drawCentered("© 2025 Shelstad Studios");
}
