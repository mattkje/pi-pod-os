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
    for (int y = TOP_BAR_HEIGHT; y < winHeight; y++) {
        float t = float(y - TOP_BAR_HEIGHT) / float(winHeight - TOP_BAR_HEIGHT - 1);
        Uint8 rCol = Uint8(0x87 * (1 - t) + 0x45 * t);
        Uint8 gCol = Uint8(0xa8 * (1 - t) + 0x5d * t);
        Uint8 bCol = Uint8(0xec * (1 - t) + 0xa3 * t);
        SDL_SetRenderDrawColor(r, rCol, gCol, bCol, 255);
        SDL_RenderDrawLine(r, rightX, y, winWidth, y);
    }

    // ---------------- Left side menu ----------------
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
}

