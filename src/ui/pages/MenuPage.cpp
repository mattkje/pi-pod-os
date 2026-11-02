#include "ui/pages/MenuPage.h"
#include "app/I18n.h"
#include <algorithm>

constexpr int ITEM_HEIGHT = 23;
constexpr Uint32 FADE_MS = 250;

namespace {
    // Album art fills the whole pane (centre-cropped), like the iPod classic.
    void drawFilledArt(const UI &ui, SDL_Texture *tex, Uint8 alpha, int paneX, int paneW) {
        int paneY = TOP_BAR_HEIGHT, paneH = ui.h - TOP_BAR_HEIGHT;
        int tw, th;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        if (tw <= 0 || th <= 0) return;

        SDL_Rect src{0, 0, tw, th};
        if (tw * paneH > th * paneW) { // texture wider than pane: crop the sides
            src.w = th * paneW / paneH;
            src.x = (tw - src.w) / 2;
        } else { // taller: crop top and bottom
            src.h = tw * paneH / paneW;
            src.y = (th - src.h) / 2;
        }
        SDL_Rect dst{paneX, paneY, paneW, paneH};
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_RenderCopy(ui.r, tex, &src, &dst);
        SDL_SetTextureAlphaMod(tex, 255);
    }

    // Draws an icon centred in the pane with a drop shadow and a fading reflection, iPod style.
    // Album art is drawn edge to edge instead.
    void drawPreviewItem(const UI &ui, SDL_Texture *tex, bool isArt, Uint8 alpha, int paneX, int paneW) {
        if (!tex || alpha == 0) return;
        if (isArt) {
            drawFilledArt(ui, tex, alpha, paneX, paneW);
            return;
        }
        SDL_Renderer *r = ui.r;

        int paneH = ui.h - TOP_BAR_HEIGHT;
        int size = std::min(paneW - 56, paneH / 2);
        int x = paneX + (paneW - size) / 2;
        int y = TOP_BAR_HEIGHT + (paneH - size) * 2 / 5;
        SDL_Rect dst{x, y, size, size};

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(tex, 0, 0, 40);
        SDL_SetTextureAlphaMod(tex, Uint8(alpha * 70 / 255));
        SDL_Rect shadow{x + 2, y + 3, size, size};
        SDL_RenderCopy(r, tex, nullptr, &shadow);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_RenderCopy(r, tex, nullptr, &dst);

        // Reflection: flipped copy, faded out with the pane gradient drawn over it.
        int reflH = size * 35 / 100;
        int reflY = y + size + 2;
        SDL_Rect src{0, 0, 0, 0};
        int tw, th;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        src = {0, th - th * 35 / 100, tw, th * 35 / 100};
        SDL_Rect reflDst{x, reflY, size, reflH};
        SDL_SetTextureAlphaMod(tex, Uint8(alpha * 90 / 255));
        SDL_RenderCopyEx(r, tex, &src, &reflDst, 0, nullptr, SDL_FLIP_VERTICAL);
        SDL_SetTextureAlphaMod(tex, 255);
        for (int i = 0; i < reflH; i++) {
            SDL_Color c = paneColor(reflY + i, ui.h);
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, Uint8(110 + 145 * i / std::max(1, reflH)));
            SDL_RenderDrawLine(r, x, reflY + i, x + size - 1, reflY + i);
        }
    }
}

void drawMenu(const UI &ui, AppState &state, const std::vector<MenuItem> &items, const MenuPreview &preview) {
    SDL_Renderer *r = ui.r;
    int half = ui.w / 2;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // ---------------- Right pane ----------------
    drawPaneBackground(ui, half, ui.w - half);

    Uint32 since = SDL_GetTicks() - preview.changedAt;
    Uint8 fadeIn = since >= FADE_MS ? 255 : Uint8(255 * since / FADE_MS);
    drawPreviewItem(ui, preview.previous.get(), preview.previousIsArt, Uint8(255 - fadeIn), half, ui.w - half);
    drawPreviewItem(ui, preview.current.get(), preview.currentIsArt, fadeIn, half, ui.w - half);

    // ---------------- Left side menu ----------------
    SDL_SetRenderDrawColor(r, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
    SDL_Rect left{0, 0, half, ui.h};
    SDL_RenderFillRect(r, &left);

    std::vector<ListRow> rows;
    for (const auto &item: items) {
        bool arrow = item.action == MenuAction::Navigate || item.action == MenuAction::NowPlaying;
        rows.push_back({tr(item.label), "", arrow});
    }
    drawList(ui, state, rows, 0, half, ITEM_HEIGHT);

    // Top bar spans the full width; the title is centred over the menu half.
    drawTopBar(ui, "iPod", ui.w);

    drawPaneShadow(ui, half);
}
