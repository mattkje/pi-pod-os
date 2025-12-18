#include "MenuPage.h"
#include "../Utils.h"

const SDL_Color TEXT_COLOR = {40, 40, 40, 255};
const SDL_Color SELECTED_TEXT_COLOR = {255, 255, 255, 255};
const SDL_Color SEPARATOR_COLOR = {200, 200, 200, 255};
constexpr int ITEM_HEIGHT = 23;
constexpr int TOP_BAR_HEIGHT = 20;

void drawMenu(SDL_Renderer *r, TTF_Font *font, AppState &state,
              const std::vector<MenuItem> &items, int winWidth, int winHeight) {
    // ---------------- Right side gradient ----------------
    int rightX = winWidth / 2;
    int rightWidth = winWidth - rightX;
    for (int y = 0; y < winHeight; y++) {  // start from 0 instead of TOP_BAR_HEIGHT
        float t = float(y) / float(winHeight - 1);
        Uint8 rCol = Uint8(0x87 * (1 - t) + 0x45 * t);
        Uint8 gCol = Uint8(0xa8 * (1 - t) + 0x5d * t);
        Uint8 bCol = Uint8(0xec * (1 - t) + 0xa3 * t);
        SDL_SetRenderDrawColor(r, rCol, gCol, bCol, 255);
        SDL_RenderDrawLine(r, rightX, y, winWidth, y);
    }


    // ---------------- Left side menu ----------------
    drawTopBar(r, font, "iPod", winWidth / 2, 100);
    int visibleItems = (winHeight - TOP_BAR_HEIGHT) / ITEM_HEIGHT;
    int startY = TOP_BAR_HEIGHT + (winHeight - TOP_BAR_HEIGHT - visibleItems * ITEM_HEIGHT) / 2;

    for (int i = 0; i < (int)items.size(); i++) {
        int y = startY + i * ITEM_HEIGHT;
        if (y < TOP_BAR_HEIGHT || y > winHeight - ITEM_HEIGHT) continue;

        bool selected = (i == state.selected);
        if (selected) {
            // Highlight only on the left side (menu)
            drawHighlight(r, SDL_Rect{0, y - 4, winWidth / 2, ITEM_HEIGHT});
        }

        SDL_Color color = selected ? SELECTED_TEXT_COLOR : TEXT_COLOR;
        SDL_Texture *text = renderText(r, font, items[i].label, color);
        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
        SDL_Rect dst{24, y, w, h};
        SDL_RenderCopy(r, text, nullptr, &dst);
        SDL_DestroyTexture(text);

        if (!selected) {
            SDL_SetRenderDrawColor(r, SEPARATOR_COLOR.r, SEPARATOR_COLOR.g, SEPARATOR_COLOR.b, 255);
            SDL_RenderDrawLine(r, 12, y + ITEM_HEIGHT - 2, winWidth / 2 - 12, y + ITEM_HEIGHT - 2);
        }
    }

    // Enable alpha blending (do this once, e.g., after creating the renderer)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // ---------------- Left shadow over right side ----------------
    int shadowWidth = 20; // width of the shadow gradient
    SDL_Color shadowColor = {0, 0, 0, 128}; // semi-transparent black

    for (int x = 0; x < shadowWidth; x++) {
        float alphaFactor = 1.0f - float(x) / float(shadowWidth);
        Uint8 a = Uint8(shadowColor.a * alphaFactor);

        SDL_SetRenderDrawColor(r, shadowColor.r, shadowColor.g, shadowColor.b, a);
        SDL_Rect shadowRect = {winWidth/2 + x, 0, 1, winHeight};
        SDL_RenderFillRect(r, &shadowRect);
    }

}

