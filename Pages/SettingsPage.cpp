#include "SettingsPage.h"
#include "../Utils.h"
#include <vector>
#include <string>

constexpr int ITEM_HEIGHT = 36; // same height as menu items

void drawSettingsPage(SDL_Renderer *r, TTF_Font *font, AppState &state, int winWidth, int winHeight) {
    drawTopBar(r, font, "Settings", winWidth);

    std::vector<std::string> settingsItems{
        "Brightness: Medium",
        "Theme: Light",
        "Volume: 75%",
        "Shuffle: Off",
        "Repeat: Off",
        "Reset PiPod OS"
    };

    int topY = 32 + 24; // start under top bar

    // Smooth visual offset for scrolling
    int centerY = (winHeight + 32) / 2;
    state.visualOffset += (state.selected - state.visualOffset) * 0.15f;

    for (int i = 0; i < (int)settingsItems.size(); ++i) {
        float y = centerY + (i - state.visualOffset) * ITEM_HEIGHT;
        if (y < 32 - ITEM_HEIGHT || y > winHeight + ITEM_HEIGHT) continue;

        bool selected = (i == state.selected);
        if (selected) drawHighlight(r, SDL_Rect{0, (int)y - 4, winWidth, ITEM_HEIGHT});

        SDL_Color color = selected ? SDL_Color{255,255,255,255} : SDL_Color{40,40,40,255};
        SDL_Texture *text = renderText(r, font, settingsItems[i], color);
        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
        SDL_Rect dst{24, (int)y, w, h};
        SDL_RenderCopy(r, text, nullptr, &dst);
        SDL_DestroyTexture(text);
    }
}
