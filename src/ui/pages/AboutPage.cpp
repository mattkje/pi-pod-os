#include "ui/pages/AboutPage.h"
#include "app/I18n.h"
#include <algorithm>
#include <vector>

#ifndef PIPOD_VERSION
#define PIPOD_VERSION "dev"
#endif

namespace {
    constexpr int ROW_HEIGHT = 20;

    struct Fact {
        std::string label;
        std::string value;
    };

    std::vector<Fact> facts(const DeviceInfo &info) {
        return {
            {tr("Songs"), std::to_string(info.localSongs + info.jellyfinSongs)},
            {"Jellyfin", std::to_string(info.jellyfinSongs)},
            {tr("Capacity"), formatBytes(info.diskTotal)},
            {tr("Available"), formatBytes(info.diskFree)},
            {tr("Version"), PIPOD_VERSION},
            {tr("Model"), info.model},
            {"S/N", info.serial},
            {tr("System"), info.system},
            {"IP", info.ipAddress.empty() ? tr("Offline") : info.ipAddress},
            {"Web", info.webUrl.empty() ? "-" : info.webUrl},
            {tr("Developer"), "Matti Kjellstadli"},
            {tr("Website"), "mattikjellstadli.com"},
        };
    }

    struct Segment {
        unsigned long long bytes;
        SDL_Color top, bottom;
        std::string label;
    };

    // Glossy horizontal bar split into coloured segments, like the iPod's capacity bar.
    void drawStorageBar(const UI &ui, int x, int y, int w, int h, const std::vector<Segment> &segs,
                        unsigned long long total) {
        SDL_Renderer *r = ui.r;
        int cx = x;
        for (size_t i = 0; i < segs.size(); i++) {
            int sw = total ? static_cast<int>(w * (double(segs[i].bytes) / double(total))) : 0;
            if (i + 1 == segs.size()) sw = x + w - cx; // last segment takes the rounding rest
            if (sw <= 0) continue;
            for (int j = 0; j < h; j++) {
                float t = float(j) / float(h - 1);
                // Bright top half, darker bottom half: the classic glossy look.
                float k = t < 0.5f ? t * 0.6f : 0.3f + (t - 0.5f) * 1.4f;
                const SDL_Color &a = segs[i].top, &b = segs[i].bottom;
                SDL_SetRenderDrawColor(r, Uint8(a.r + (b.r - a.r) * k), Uint8(a.g + (b.g - a.g) * k),
                                       Uint8(a.b + (b.b - a.b) * k), 255);
                SDL_RenderDrawLine(r, cx, y + j, cx + sw - 1, y + j);
            }
            cx += sw;
        }
        SDL_SetRenderDrawColor(r, 130, 130, 130, 255);
        SDL_Rect border{x, y, w, h};
        SDL_RenderDrawRect(r, &border);
    }

    int listTop(const UI &ui) {
        int nameH = TTF_FontHeight(ui.bold ? ui.bold : ui.font);
        int legendH = TTF_FontHeight(ui.small);
        return TOP_BAR_HEIGHT + 6 + nameH + 6 + 12 + 4 + legendH + 8;
    }

    int visibleRows(const UI &ui) {
        return std::max(1, (ui.h - listTop(ui)) / ROW_HEIGHT);
    }
}

int aboutRowCount(const AppState &) {
    return static_cast<int>(facts(DeviceInfo{}).size());
}

void drawAboutPage(const UI &ui, AppState &state, const DeviceInfo &info, const std::string &deviceName) {
    SDL_Renderer *r = ui.r;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    drawTopBar(ui, tr("About"));

    // ---- Name ----
    TTF_Font *big = ui.bold ? ui.bold : ui.font;
    int y = TOP_BAR_HEIGHT + 6;
    y += drawText(r, big, deviceName, TEXT_COLOR, ui.w / 2, y, 1, ui.w - 20).y + 6;

    // ---- Storage bar: music, Jellyfin cache, other, free ----
    unsigned long long used = info.diskTotal > info.diskFree ? info.diskTotal - info.diskFree : 0;
    unsigned long long music = std::min(info.musicBytes, used);
    unsigned long long cache = std::min(info.cacheBytes, used - music);
    unsigned long long other = used - music - cache;
    std::vector<Segment> segs{
        {music, {120, 170, 255, 255}, {30, 90, 210, 255}, tr("Music")},
        {cache, {200, 150, 255, 255}, {120, 60, 200, 255}, "Jellyfin"},
        {other, {255, 220, 110, 255}, {220, 160, 20, 255}, tr("Other")},
        {info.diskFree, {250, 250, 250, 255}, {205, 205, 205, 255}, tr("Free")},
    };
    int barX = 12, barW = ui.w - 24;
    drawStorageBar(ui, barX, y, barW, 12, segs, info.diskTotal);
    y += 12 + 4;

    // Legend: coloured dot + label, spread evenly
    int slot = barW / static_cast<int>(segs.size());
    int legendH = TTF_FontHeight(ui.small);
    for (size_t i = 0; i < segs.size(); i++) {
        int lx = barX + slot * static_cast<int>(i);
        SDL_Rect dot{lx, y + legendH / 2 - 3, 7, 7};
        SDL_SetRenderDrawColor(r, segs[i].bottom.r, segs[i].bottom.g, segs[i].bottom.b, 255);
        SDL_RenderFillRect(r, &dot);
        drawText(r, ui.small, segs[i].label, SUBTEXT_COLOR, lx + 10, y, 0, slot - 12);
    }
    y += legendH + 8;

    // ---- Facts list (scrolls with Up/Down) ----
    std::vector<Fact> list = facts(info);
    int top = listTop(ui);
    int visible = visibleRows(ui);
    int maxScroll = std::max(0, static_cast<int>(list.size()) - visible);
    state.selected = std::clamp(state.selected, 0, maxScroll);
    state.visualOffset += (state.selected - state.visualOffset) * 0.35f;
    if (std::abs(state.selected - state.visualOffset) < 0.01f) state.visualOffset = static_cast<float>(state.selected);

    SDL_SetRenderDrawColor(r, SEPARATOR_COLOR.r, SEPARATOR_COLOR.g, SEPARATOR_COLOR.b, 255);
    SDL_RenderDrawLine(r, 0, top - 1, ui.w, top - 1);

    SDL_Rect clip{0, top, ui.w, ui.h - top};
    SDL_RenderSetClipRect(r, &clip);
    int textH = TTF_FontHeight(ui.font);
    for (size_t i = 0; i < list.size(); i++) {
        int ry = top + static_cast<int>((float(i) - state.visualOffset) * ROW_HEIGHT);
        if (ry + ROW_HEIGHT < top || ry > ui.h) continue;
        int ty = ry + (ROW_HEIGHT - textH) / 2;
        int labelW = drawText(r, ui.font, list[i].label, TEXT_COLOR, 12, ty).x;
        drawText(r, ui.font, list[i].value, SUBTEXT_COLOR, ui.w - 12, ty, 2, ui.w - 24 - labelW - 12);
    }
    SDL_RenderSetClipRect(r, nullptr);

    // Scroll hint
    if (maxScroll > 0) {
        int trackH = ui.h - top - 4;
        int thumbH = std::max(10, trackH * visible / static_cast<int>(list.size()));
        int thumbY = top + 2 + static_cast<int>((trackH - thumbH) * state.visualOffset / maxScroll);
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_Rect track{ui.w - 4, top + 2, 3, trackH};
        SDL_RenderFillRect(r, &track);
        SDL_SetRenderDrawColor(r, 110, 110, 110, 255);
        SDL_Rect thumb{ui.w - 4, thumbY, 3, thumbH};
        SDL_RenderFillRect(r, &thumb);
    }
}
