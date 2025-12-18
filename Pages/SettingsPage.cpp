#include "SettingsPage.h"
#include "../Utils.h"
#include <vector>
#include <string>

constexpr int ITEM_HEIGHT = 36;

void drawSettingsPage(SDL_Renderer *r, TTF_Font *font, AppState &state, int winWidth, int winHeight) {
    drawTopBar(r, font, "Settings", winWidth);


    std::vector<std::string> settingsItems{
        "Update Software",
        "Brightness: Medium",
        "Theme: Light",
        "Volume: 75%",
        "Shuffle: Off",
        "Repeat: Off",
        "Add Jellyfin Server",
        "Reset PiPod OS"
    };
    int topY = 32 + 24; // below top bar
    int centerY = (winHeight + 32) / 2;
    state.visualOffset += (state.selected - state.visualOffset) * 0.15f;

    for (int i = 0; i < (int) settingsItems.size(); ++i) {
        float y = centerY + (i - state.visualOffset) * ITEM_HEIGHT;
        if (y < 32 - ITEM_HEIGHT || y > winHeight + ITEM_HEIGHT) continue;

        bool selected = (i == state.selected);
        if (selected) drawHighlight(r, SDL_Rect{0, (int) y - 4, winWidth, ITEM_HEIGHT});

        SDL_Color color = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{40, 40, 40, 255};
        SDL_Texture *text = renderText(r, font, settingsItems[i], color);
        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
        SDL_Rect dst{24, (int) y, w, h};
        SDL_RenderCopy(r, text, nullptr, &dst);
        SDL_DestroyTexture(text);
    }

    // ------------------ Jellyfin Input Mode ------------------
    if (state.inputMode != SettingsInputMode::None) {
        int inputY = winHeight / 2 - 60;
        SDL_SetRenderDrawColor(r, 220, 220, 220, 200);
        SDL_Rect box{50, inputY, winWidth - 100, 140};
        SDL_RenderFillRect(r, &box);

        std::string label;
        std::string value;

        switch (state.inputMode) {
            case SettingsInputMode::JellyfinUrl: label = "Jellyfin URL:";
                value = state.jellyfinUrl;
                break;
            case SettingsInputMode::JellyfinUser: label = "Username:";
                value = state.jellyfinUser;
                break;
            case SettingsInputMode::JellyfinPass: label = "Password:";
                value = state.jellyfinPass;
                break;
            default: break;
        }

        SDL_Texture *tLabel = renderText(r, font, label, {0, 0, 0, 255});
        SDL_Texture *tValue = renderText(r, font, value + (SDL_GetTicks() / 500 % 2 ? "|" : ""), {0, 0, 0, 255});

        int lw, lh, vw, vh;
        SDL_QueryTexture(tLabel, nullptr, nullptr, &lw, &lh);
        SDL_QueryTexture(tValue, nullptr, nullptr, &vw, &vh);

        SDL_Rect dstLabel{box.x + 12, box.y + 12, lw, lh};
        SDL_Rect dstValue{box.x + 12, box.y + 12 + lh + 10, vw, vh};

        SDL_RenderCopy(r, tLabel, nullptr, &dstLabel);
        SDL_RenderCopy(r, tValue, nullptr, &dstValue);

        SDL_DestroyTexture(tLabel);
        SDL_DestroyTexture(tValue);
    }
}
