#include "ui/pages/NowPlayingPage.h"
#include "app/I18n.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    // Background: white fading to a light silver towards the bottom.
    SDL_Color backgroundAt(int y, int h) {
        float t = float(y - TOP_BAR_HEIGHT) / float(std::max(1, h - TOP_BAR_HEIGHT));
        auto c = Uint8(255 - 30 * t);
        return {c, c, Uint8(std::min(255, c + 3)), 255};
    }

    void drawBackground(const UI &ui) {
        for (int y = TOP_BAR_HEIGHT; y < ui.h; y++) {
            SDL_Color c = backgroundAt(y, ui.h);
            SDL_SetRenderDrawColor(ui.r, c.r, c.g, c.b, 255);
            SDL_RenderDrawLine(ui.r, 0, y, ui.w - 1, y);
        }
    }

    // The cover is drawn turned slightly away from the viewer, like the iPod classic: the right edge
    // is shorter and the top slopes down. Rendered as thin vertical strips so the texture follows
    // the trapezoid without visible seams.
    struct Tilt {
        int x, w;            // horizontal extent
        float topL, botL;    // left edge
        float topR, botR;    // right edge

        float top(float t) const { return topL + (topR - topL) * t; }
        float bot(float t) const { return botL + (botR - botL) * t; }
    };

    Tilt tiltFor(const SDL_Rect &rect) {
        return {rect.x, rect.w,
                float(rect.y), float(rect.y + rect.h),
                rect.y + rect.h * 0.09f, rect.y + rect.h * 0.97f};
    }

    // Draws texture rows v0..v1 (0..1) between yTop(t) and yBottom(t) for each column t.
    template<typename TopFn, typename BotFn>
    void drawStrips(SDL_Renderer *r, SDL_Texture *tex, const Tilt &tilt, TopFn yTop, BotFn yBot,
                    float v0, float v1, SDL_Color cTop, SDL_Color cBot) {
        constexpr int N = 32;
        std::vector<SDL_Vertex> verts;
        verts.reserve(N * 6);
        for (int i = 0; i < N; i++) {
            float t0 = float(i) / N, t1 = float(i + 1) / N;
            float x0 = tilt.x + tilt.w * t0, x1 = tilt.x + tilt.w * t1;
            SDL_Vertex a{{x0, yTop(t0)}, cTop, {t0, v0}};
            SDL_Vertex b{{x1, yTop(t1)}, cTop, {t1, v0}};
            SDL_Vertex c{{x1, yBot(t1)}, cBot, {t1, v1}};
            SDL_Vertex d{{x0, yBot(t0)}, cBot, {t0, v1}};
            verts.insert(verts.end(), {a, b, c, a, c, d});
        }
        SDL_RenderGeometry(r, tex, verts.data(), static_cast<int>(verts.size()), nullptr, 0);
    }

    void drawCover(const UI &ui, SDL_Texture *art, SDL_Texture *placeholderIcon, const SDL_Rect &rect, int reflH) {
        SDL_Renderer *r = ui.r;
        Tilt tilt = tiltFor(rect);
        auto top = [&](float t) { return tilt.top(t); };
        auto bot = [&](float t) { return tilt.bot(t); };
        // Reflection height shrinks with the cover towards the right edge.
        auto reflBot = [&](float t) {
            float colH = tilt.bot(t) - tilt.top(t);
            return tilt.bot(t) + reflH * colH / rect.h;
        };
        const SDL_Color opaque{255, 255, 255, 255};
        const SDL_Color reflTop{255, 255, 255, 105};
        const SDL_Color clear{255, 255, 255, 0};

        if (art) {
            SDL_SetTextureBlendMode(art, SDL_BLENDMODE_BLEND);
            drawStrips(r, art, tilt, top, bot, 0.0f, 1.0f, opaque, opaque);
            if (reflH > 0) drawStrips(r, art, tilt, bot, reflBot, 1.0f, 1.0f - float(reflH) / rect.h, reflTop, clear);
            return;
        }

        // No cover: grey card with a music note, tilted the same way.
        drawStrips(r, nullptr, tilt, top, bot, 0, 1, {215, 215, 222, 255}, {170, 170, 180, 255});
        if (reflH > 0) drawStrips(r, nullptr, tilt, bot, reflBot, 0, 1, {175, 175, 185, 105}, {175, 175, 185, 0});
        if (placeholderIcon) {
            Tilt inner = tilt;
            int inset = rect.w / 4;
            inner.x += inset;
            inner.w -= 2 * inset;
            float ta = float(inset) / rect.w, tb = 1.0f - ta;
            float hL = tilt.bot(ta) - tilt.top(ta), hR = tilt.bot(tb) - tilt.top(tb);
            inner.topL = tilt.top(ta) + hL * 0.25f;
            inner.botL = tilt.bot(ta) - hL * 0.25f;
            inner.topR = tilt.top(tb) + hR * 0.25f;
            inner.botR = tilt.bot(tb) - hR * 0.25f;
            SDL_SetTextureColorMod(placeholderIcon, 245, 245, 250);
            drawStrips(r, placeholderIcon, inner, [&](float t) { return inner.top(t); },
                       [&](float t) { return inner.bot(t); }, 0, 1, opaque, opaque);
            SDL_SetTextureColorMod(placeholderIcon, 255, 255, 255);
        }
    }

    void drawProgress(const UI &ui, const Player &player, int barY) {
        SDL_Renderer *r = ui.r;
        double pos = player.position();
        double dur = player.duration();
        float frac = (pos >= 0 && dur > 0) ? std::clamp(float(pos / dur), 0.0f, 1.0f) : 0.0f;

        TTF_Font *font = ui.medium ? ui.medium : ui.small;
        int textH = TTF_FontHeight(font);
        std::string elapsed = formatTime(pos);
        std::string remaining = dur > 0 ? "-" + formatTime(std::max(0.0, dur - pos)) : formatTime(-1);
        int ew = 0, rw = 0;
        TTF_SizeUTF8(font, "-00:00", &rw, nullptr);
        TTF_SizeUTF8(font, "00:00", &ew, nullptr);

        constexpr int barH = 10;
        int barX = 8 + ew + 6;
        int barW = ui.w - barX - rw - 14;
        int textY = barY + barH / 2 - textH / 2;
        drawText(r, font, elapsed, TEXT_COLOR, 8, textY);
        drawText(r, font, remaining, TEXT_COLOR, ui.w - 8, textY, 2);

        // Glossy track
        for (int i = 0; i < barH; i++) {
            auto c = Uint8(i < barH / 2 ? 250 - i * 4 : 225 - (i - barH / 2) * 3);
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            SDL_RenderDrawLine(r, barX, barY + i, barX + barW - 1, barY + i);
        }
        // Blue fill
        int fillW = static_cast<int>(barW * frac);
        if (fillW > 0) {
            for (int i = 0; i < barH; i++) {
                float t = float(i) / barH;
                Uint8 rr = Uint8(t < 0.5f ? 110 - t * 120 : 30 + (t - 0.5f) * 40);
                Uint8 gg = Uint8(t < 0.5f ? 170 - t * 120 : 100 + (t - 0.5f) * 60);
                SDL_SetRenderDrawColor(r, rr, gg, 235, 255);
                SDL_RenderDrawLine(r, barX, barY + i, barX + fillW - 1, barY + i);
            }
        }
        SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
        SDL_Rect border{barX, barY, barW, barH};
        SDL_RenderDrawRect(r, &border);
    }

    void drawStatusIcon(const UI &ui, SDL_Texture *icon, int rightX, int y, int size) {
        if (!icon) return;
        SDL_SetTextureColorMod(icon, 70, 70, 80);
        SDL_SetTextureAlphaMod(icon, 255);
        SDL_Rect dst{rightX - size, y, size, size};
        SDL_RenderCopy(ui.r, icon, nullptr, &dst);
        SDL_SetTextureColorMod(icon, 255, 255, 255);
    }
}

void drawNowPlaying(const UI &ui, const Player &player, const Settings &settings, MenuIcons &icons) {
    SDL_Renderer *r = ui.r;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    drawBackground(ui);
    drawTopBar(ui, tr("Now Playing"));

    const Song *song = player.current();
    if (!song) {
        drawText(r, ui.font, tr("Nothing playing"), SUBTEXT_COLOR, ui.w / 2, ui.h / 2 - 10, 1);
        return;
    }

    // Shuffle / repeat state in the top-right corner
    int iconSize = 14;
    int iconY = TOP_BAR_HEIGHT + 5;
    int iconRight = ui.w - 8;
    if (settings.repeat != RepeatMode::Off) {
        drawStatusIcon(ui, icons.get(MenuIcon::Repeat), iconRight, iconY, iconSize);
        if (settings.repeat == RepeatMode::One)
            drawText(r, ui.small, "1", {70, 70, 80, 255}, iconRight - iconSize / 2, iconY + iconSize - 6, 1);
        iconRight -= iconSize + 6;
    }
    if (settings.shuffle) drawStatusIcon(ui, icons.get(MenuIcon::Shuffle), iconRight, iconY, iconSize);

    // Layout
    constexpr int barH = 10;
    int barY = ui.h - barH - 12;
    int art = std::min(ui.w * 41 / 100, (barY - TOP_BAR_HEIGHT) * 70 / 100);
    SDL_Rect artRect{10, TOP_BAR_HEIGHT + 9, art, art};

    // Tilted cover + reflection
    int reflH = std::min(art * 32 / 100, barY - 6 - (artRect.y + art));
    drawCover(ui, song->artwork.get(), icons.get(MenuIcon::Music), artRect, reflH);

    // Song info, vertically centred next to the cover
    int textX = artRect.x + art + 14;
    int textW = ui.w - textX - 8;
    TTF_Font *titleFont = ui.bold ? ui.bold : ui.font;
    TTF_Font *infoFont = ui.medium ? ui.medium : ui.small;
    int titleH = TTF_FontHeight(titleFont), infoH = TTF_FontHeight(infoFont);
    bool hasAlbum = !song->album.empty();
    int blockH = titleH + infoH * (hasAlbum ? 2 : 1) + 12 + infoH;
    int y = artRect.y + (art - blockH) / 2;

    drawText(r, titleFont, song->title, TEXT_COLOR, textX, y, 0, textW);
    y += titleH;
    drawText(r, infoFont, song->artist, TEXT_COLOR, textX, y, 0, textW);
    y += infoH;
    if (hasAlbum) {
        drawText(r, infoFont, song->album, TEXT_COLOR, textX, y, 0, textW);
        y += infoH;
    }
    y += 12;

    std::string position = trf("{} of {}", {std::to_string(player.queueIndex() + 1), std::to_string(player.queueSize())});
    if (player.isLoading()) position = tr("Downloading...");
    drawText(r, infoFont, position, TEXT_COLOR, textX, y, 0, textW);

    if (!player.error().empty()) {
        drawText(r, ui.small, tr("Can't play:") + " " + player.error(), {180, 40, 30, 255}, ui.w / 2, barY - 2, 1, ui.w - 16);
        return;
    }
    drawProgress(ui, player, barY);
}
