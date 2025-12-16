#include "Utils.h"

SDL_Texture *renderText(SDL_Renderer *r, TTF_Font *font, const std::string &text, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void drawTopBar(SDL_Renderer *r, TTF_Font *font, const std::string &title, int winWidth) {
    for (int y = 0; y < 32; y++) {
        Uint8 shade = 245 - y;
        SDL_SetRenderDrawColor(r, shade, shade, shade, 255);
        SDL_RenderDrawLine(r, 0, y, winWidth, y);
    }
    SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
    SDL_RenderDrawLine(r, 0, 31, winWidth, 31);

    SDL_Texture *text = renderText(r, font, title, {40, 40, 40, 255});
    int w, h;
    SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
    SDL_Rect dst{12, (32 - h) / 2, w, h};
    SDL_RenderCopy(r, text, nullptr, &dst);
    SDL_DestroyTexture(text);
}

void drawHighlight(SDL_Renderer *r, const SDL_Rect &rect) {
    for (int i = 0; i < rect.h; i++) {
        float t = i / (float)rect.h;
        Uint8 rCol = (Uint8)(20 + t * 20);
        Uint8 gCol = (Uint8)(120 + t * 60);
        Uint8 bCol = 255;
        SDL_SetRenderDrawColor(r, rCol, gCol, bCol, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y + i, rect.x + rect.w, rect.y + i);
    }
    SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
    SDL_RenderDrawLine(r, rect.x, rect.y, rect.x + rect.w, rect.y);
}
