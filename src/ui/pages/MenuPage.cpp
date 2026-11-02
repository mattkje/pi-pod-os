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

    // Brushed-silver icon: the white icon is tinted in thin horizontal strips from light at the top
    // to darker at the bottom, like the iPod classic's menu artwork.
    void drawSilverIcon(SDL_Renderer *r, SDL_Texture *tex, const SDL_Rect &dst, Uint8 alpha) {
        int tw, th;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        if (tw <= 0 || th <= 0 || dst.h <= 0) return;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(tex, alpha);
        constexpr int STRIPS = 48;
        for (int i = 0; i < STRIPS; i++) {
            int sy0 = th * i / STRIPS, sy1 = th * (i + 1) / STRIPS;
            int dy0 = dst.h * i / STRIPS, dy1 = dst.h * (i + 1) / STRIPS;
            if (sy1 <= sy0 || dy1 <= dy0) continue;
            float t = (i + 0.5f) / STRIPS;
            // Light silver on top, a soft highlight band, darker towards the bottom.
            float shade = 236 - 70 * t + (t > 0.45f && t < 0.55f ? 6 : 0);
            Uint8 c = Uint8(std::clamp(shade, 0.0f, 255.0f));
            SDL_SetTextureColorMod(tex, c, c, Uint8(std::min(255, c + 4)));
            SDL_Rect src{0, sy0, tw, sy1 - sy0};
            SDL_Rect out{dst.x, dst.y + dy0, dst.w, dy1 - dy0};
            SDL_RenderCopy(r, tex, &src, &out);
        }
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
    }

    // Draws an icon (silver, with an optional caption below) centred in the pane.
    // Album art is drawn edge to edge instead.
    void drawPreviewItem(const UI &ui, SDL_Texture *tex, bool isArt, const std::string &caption, Uint8 alpha,
                         int paneX, int paneW) {
        if (!tex || alpha == 0) return;
        if (isArt) {
            drawFilledArt(ui, tex, alpha, paneX, paneW);
            return;
        }
        int paneH = ui.h - TOP_BAR_HEIGHT;
        int size = std::min(paneW - 64, paneH * 45 / 100);
        TTF_Font *bold = ui.bold ? ui.bold : ui.font;
        int captionH = caption.empty() ? 0 : TTF_FontHeight(bold) + 6;
        int x = paneX + (paneW - size) / 2;
        int y = TOP_BAR_HEIGHT + (paneH - size - captionH) / 2;
        drawSilverIcon(ui.r, tex, SDL_Rect{x, y, size, size}, alpha);
        if (!caption.empty())
            drawText(ui.r, bold, caption, {255, 255, 255, alpha}, paneX + paneW / 2, y + size + 6, 1, paneW - 12);
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
    drawPreviewItem(ui, preview.previous.get(), preview.previousIsArt, "", Uint8(255 - fadeIn), half, ui.w - half);
    drawPreviewItem(ui, preview.current.get(), preview.currentIsArt, preview.caption, fadeIn, half, ui.w - half);

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
    const std::string &name = state.settings.deviceName;
    drawTopBar(ui, name.empty() ? "PiPod" : name, ui.w);

    drawPaneShadow(ui, half);
}
