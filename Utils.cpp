#include "Utils.h"
#include "JellyfinClient.h"

std::vector<Song> loadJellyfinSongs(SDL_Renderer * /*renderer*/,
                                    const std::string &serverUrl,
                                    const std::string &apiKey,
                                    const std::string &userId,
                                    const std::string &libraryId) {
    return Jellyfin::fetchSongs(serverUrl, apiKey, userId, libraryId);
}

SDL_Texture *renderText(SDL_Renderer *r, TTF_Font *font, const std::string &text, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void drawTopBar(SDL_Renderer *r, TTF_Font *font, const std::string &title, int winWidth, int batteryPercent) {
    constexpr int TOP_BAR_HEIGHT = 32;

    // ---------------- Top bar gradient ----------------
    for (int y = 0; y < TOP_BAR_HEIGHT; y++) {
        float t = float(y) / float(TOP_BAR_HEIGHT - 1);

        // Non-linear gradient: bottom darker more
        float factor = t * t; // quadratic to make bottom darker faster
        Uint8 shade = Uint8(245 - factor * 50); // subtract up to 50 at bottom

        SDL_SetRenderDrawColor(r, shade, shade, shade, 255);
        SDL_RenderDrawLine(r, 0, y, winWidth, y);
    }


    // Bottom line
    SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
    SDL_RenderDrawLine(r, 0, TOP_BAR_HEIGHT - 1, winWidth, TOP_BAR_HEIGHT - 1);


    // ---------------- Title ----------------
    SDL_Texture *text = renderText(r, font, title, {40, 40, 40, 255});
    int w, h;
    SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
    SDL_Rect dst{12, (TOP_BAR_HEIGHT - h) / 2, w, h};
    SDL_RenderCopy(r, text, nullptr, &dst);
    SDL_DestroyTexture(text);

    // ---------------- Battery ----------------
    int batteryWidth = 30; // shorter battery
    int batteryHeight = 14;
    int x = winWidth - batteryWidth - 12;
    int yPos = (TOP_BAR_HEIGHT - batteryHeight) / 2;

    // Tip on the right
    SDL_Rect tip{x + batteryWidth, yPos + batteryHeight / 4, 4, batteryHeight / 2};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderFillRect(r, &tip);

    // Outline
    SDL_Rect outline{x, yPos, batteryWidth, batteryHeight};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderDrawRect(r, &outline);

    // Fill gradient based on battery percent
    int fillWidth = (batteryWidth - 2) * batteryPercent / 100;

    // Gradient colors for green
    SDL_Color topColor{0xa9, 0xd3, 0xa4, 255};
    SDL_Color bottomColor{0x44, 0x78, 0x4e, 255};
    SDL_Color darkerTop{0x89, 0xb3, 0x84, 255}; // tiny darker line at very top

    for (int y = 0; y < batteryHeight - 2; y++) {
        float t = float(y) / float(batteryHeight - 2);
        Uint8 rCol, gCol, bCol;

        // Optional darker top line
        if (y == 0) {
            rCol = darkerTop.r;
            gCol = darkerTop.g;
            bCol = darkerTop.b;
        } else {
            rCol = Uint8(topColor.r * (1.0f - t) + bottomColor.r * t);
            gCol = Uint8(topColor.g * (1.0f - t) + bottomColor.g * t);
            bCol = Uint8(topColor.b * (1.0f - t) + bottomColor.b * t);
        }

        SDL_SetRenderDrawColor(r, rCol, gCol, bCol, 255);
        SDL_RenderDrawLine(r, x + 1, yPos + 1 + y, x + 1 + fillWidth, yPos + 1 + y);
    }
}


void drawHighlight(SDL_Renderer *r, const SDL_Rect &rect) {
    for (int i = 0; i < rect.h; i++) {
        float t = i / (float) rect.h;
        Uint8 rCol = (Uint8) (20 + t * 20);
        Uint8 gCol = (Uint8) (120 + t * 60);
        Uint8 bCol = 255;
        SDL_SetRenderDrawColor(r, rCol, gCol, bCol, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y + i, rect.x + rect.w, rect.y + i);
    }
    SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
    SDL_RenderDrawLine(r, rect.x, rect.y, rect.x + rect.w, rect.y);
}
