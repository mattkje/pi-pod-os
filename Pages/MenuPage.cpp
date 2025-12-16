#include "MenuPage.h"
#include "../Utils.h"

const SDL_Color TEXT_COLOR = {40, 40, 40, 255};
const SDL_Color SELECTED_TEXT_COLOR = {255, 255, 255, 255};
const SDL_Color SEPARATOR_COLOR = {200, 200, 200, 255};
constexpr int ITEM_HEIGHT = 36;
constexpr int TOP_BAR_HEIGHT = 32;

void drawMenu(SDL_Renderer *r, TTF_Font *font, AppState &state,
              const std::vector<MenuItem> &items, int winWidth, int winHeight) {
    int centerY = (winHeight + TOP_BAR_HEIGHT) / 2;
    state.visualOffset += (state.selected - state.visualOffset) * 0.15f;

    for (int i = 0; i < (int)items.size(); i++) {
        float y = centerY + (i - state.visualOffset) * ITEM_HEIGHT;
        if (y < TOP_BAR_HEIGHT - ITEM_HEIGHT || y > winHeight + ITEM_HEIGHT) continue;

        bool selected = (i == state.selected);
        if (selected) drawHighlight(r, SDL_Rect{0, (int)y - 4, winWidth, ITEM_HEIGHT});

        SDL_Color color = selected ? SELECTED_TEXT_COLOR : TEXT_COLOR;
        SDL_Texture *text = renderText(r, font, items[i].label, color);
        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
        SDL_Rect dst{24, (int)y, w, h};
        SDL_RenderCopy(r, text, nullptr, &dst);
        SDL_DestroyTexture(text);

        if (!selected) {
            SDL_SetRenderDrawColor(r, SEPARATOR_COLOR.r, SEPARATOR_COLOR.g, SEPARATOR_COLOR.b, 255);
            SDL_RenderDrawLine(r, 12, (int)y + ITEM_HEIGHT - 2, winWidth - 12, (int)y + ITEM_HEIGHT - 2);
        }
    }
}
