#include "ui/pages/MediaPages.h"
#include "app/I18n.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr int ROW_HEIGHT = 36;

    void emptyMessage(const UI &ui, const std::string &title, const std::string &hint) {
        int y = ui.h / 2 - 20;
        drawText(ui.r, ui.font, title, TEXT_COLOR, ui.w / 2, y, 1, ui.w - 20);
        drawText(ui.r, ui.small, hint, SUBTEXT_COLOR, ui.w / 2, y + 26, 1, ui.w - 20);
    }

    // Draws `tex` scaled to fit inside `box`, centred.
    void drawFit(SDL_Renderer *r, SDL_Texture *tex, const SDL_Rect &box, Uint8 alpha = 255) {
        int tw, th;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        if (tw <= 0 || th <= 0) return;
        double s = std::min(double(box.w) / tw, double(box.h) / th);
        int w = int(tw * s), h = int(th * s);
        SDL_Rect dst{box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h};
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_SetTextureAlphaMod(tex, 255);
    }

    void drawProgressBar(const UI &ui, int y, double pos, double dur, SDL_Color text) {
        SDL_Renderer *r = ui.r;
        TTF_Font *font = ui.small;
        int textH = TTF_FontHeight(font);
        int ew = 0, rw = 0;
        TTF_SizeUTF8(font, "00:00:00", &ew, nullptr);
        TTF_SizeUTF8(font, "-00:00:00", &rw, nullptr);
        int barX = 8 + ew + 4, barW = ui.w - barX - rw - 12, barH = 6;
        drawText(r, font, formatTime(pos), text, 8, y + barH / 2 - textH / 2);
        drawText(r, font, dur > 0 ? "-" + formatTime(std::max(0.0, dur - pos)) : formatTime(-1), text, ui.w - 8,
                 y + barH / 2 - textH / 2, 2);
        SDL_SetRenderDrawColor(r, 90, 90, 90, 255);
        SDL_Rect track{barX, y, barW, barH};
        SDL_RenderFillRect(r, &track);
        float frac = dur > 0 ? std::clamp(float(pos / dur), 0.0f, 1.0f) : 0.0f;
        if (frac > 0) drawHighlight(r, SDL_Rect{barX, y, std::max(1, int(barW * frac)), barH});
    }
}

// ---------------- Videos ----------------

void drawVideosPage(const UI &ui, AppState &state, const std::vector<VideoItem> &videos) {
    drawTopBar(ui, tr("Videos"));
    if (videos.empty()) {
        emptyMessage(ui, tr("No videos"), tr("Add videos from the web page or connect Jellyfin"));
        return;
    }
    std::vector<ListRow> rows;
    for (const auto &v: videos) rows.push_back({v.title, v.detail});
    drawList(ui, state, rows, 0, ui.w, ROW_HEIGHT);
}

void drawVideoPlayback(const UI &ui, VideoPlayer &player, const std::string &title, bool overlay) {
    SDL_Renderer *r = ui.r;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    if (SDL_Texture *tex = player.update()) drawFit(r, tex, SDL_Rect{0, 0, ui.w, ui.h});
    else if (player.error().empty())
        drawText(r, ui.small, tr("Loading..."), {180, 180, 180, 255}, ui.w / 2, ui.h / 2 - 8, 1);

    if (!player.error().empty()) {
        drawText(r, ui.font, tr("Can't play this video"), {255, 255, 255, 255}, ui.w / 2, ui.h / 2 - 20, 1, ui.w - 20);
        drawText(r, ui.small, player.error(), {170, 170, 170, 255}, ui.w / 2, ui.h / 2 + 6, 1, ui.w - 20);
        return;
    }
    if (!overlay) return;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // Title bar and progress, translucent like the iPod's video overlay.
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect top{0, 0, ui.w, TOP_BAR_HEIGHT};
    SDL_RenderFillRect(r, &top);
    drawText(r, ui.bold ? ui.bold : ui.font, title, {255, 255, 255, 255}, 8,
             (TOP_BAR_HEIGHT - TTF_FontHeight(ui.bold ? ui.bold : ui.font)) / 2, 0, ui.w - 40);
    if (player.paused()) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_Rect a{ui.w - 20, 6, 3, 9}, b{ui.w - 14, 6, 3, 9};
        SDL_RenderFillRect(r, &a);
        SDL_RenderFillRect(r, &b);
    }
    int barArea = 26;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect bottom{0, ui.h - barArea, ui.w, barArea};
    SDL_RenderFillRect(r, &bottom);
    drawProgressBar(ui, ui.h - barArea / 2 - 3, player.position(), player.duration(), {255, 255, 255, 255});
}

// ---------------- Photos ----------------

void drawPhotosGrid(const UI &ui, AppState &state, const std::vector<MediaFile> &photos, PhotoLoader &loader) {
    SDL_Renderer *r = ui.r;
    drawTopBar(ui, tr("Photos"));
    if (photos.empty()) {
        emptyMessage(ui, tr("No photos"), tr("Add photos from the web page (Settings > Add Media)"));
        return;
    }

    int cell = ui.w / PHOTO_COLUMNS;
    int areaH = ui.h - TOP_BAR_HEIGHT;
    int visibleRows = std::max(1, areaH / cell);
    int rows = (static_cast<int>(photos.size()) + PHOTO_COLUMNS - 1) / PHOTO_COLUMNS;
    int selRow = state.selected / PHOTO_COLUMNS;

    // Scroll by rows, keeping the selected row in view.
    if (selRow < state.scrollTop) state.scrollTop = selRow;
    if (selRow >= state.scrollTop + visibleRows) state.scrollTop = selRow - visibleRows + 1;
    state.scrollTop = std::clamp(state.scrollTop, 0, std::max(0, rows - visibleRows));
    state.visualOffset += (state.scrollTop - state.visualOffset) * 0.35f;
    if (std::fabs(state.scrollTop - state.visualOffset) < 0.01f) state.visualOffset = float(state.scrollTop);

    SDL_Rect clip{0, TOP_BAR_HEIGHT, ui.w, areaH};
    SDL_RenderSetClipRect(r, &clip);
    int pad = 3;
    for (int i = 0; i < static_cast<int>(photos.size()); i++) {
        int row = i / PHOTO_COLUMNS, col = i % PHOTO_COLUMNS;
        int y = TOP_BAR_HEIGHT + 2 + static_cast<int>(std::lround((row - state.visualOffset) * cell));
        if (y + cell < TOP_BAR_HEIGHT || y > ui.h) continue;
        SDL_Rect box{col * cell + pad, y + pad, cell - 2 * pad, cell - 2 * pad};
        if (SDL_Texture *t = loader.thumbnail(photos[i])) {
            SDL_RenderCopy(r, t, nullptr, &box);
        } else {
            SDL_SetRenderDrawColor(r, 225, 225, 230, 255);
            SDL_RenderFillRect(r, &box);
        }
        if (i == state.selected) {
            // iPod-style blue selection frame
            SDL_SetRenderDrawColor(r, 30, 130, 255, 255);
            for (int k = 0; k < 3; k++) {
                SDL_Rect f{box.x - k, box.y - k, box.w + 2 * k, box.h + 2 * k};
                SDL_RenderDrawRect(r, &f);
            }
        }
    }
    SDL_RenderSetClipRect(r, nullptr);
}

void drawPhotoViewer(const UI &ui, const std::vector<MediaFile> &photos, int index, PhotoLoader &loader,
                     bool slideshow) {
    SDL_Renderer *r = ui.r;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    if (index < 0 || index >= static_cast<int>(photos.size())) return;

    SDL_Texture *tex = loader.full(photos[index]);
    if (!tex) tex = loader.thumbnail(photos[index]); // blurry placeholder until the full image is ready
    if (tex) drawFit(r, tex, SDL_Rect{0, 0, ui.w, ui.h});

    // Preload neighbours so Left/Right feel instant.
    if (index + 1 < static_cast<int>(photos.size())) loader.full(photos[index + 1]);
    if (index > 0) loader.full(photos[index - 1]);

    if (!slideshow) {
        std::string label = trf("{} of {}", {std::to_string(index + 1), std::to_string(photos.size())});
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        int th = TTF_FontHeight(ui.small);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 140);
        SDL_Rect bg{0, ui.h - th - 6, ui.w, th + 6};
        SDL_RenderFillRect(r, &bg);
        drawText(r, ui.small, label, {255, 255, 255, 255}, ui.w / 2, ui.h - th - 3, 1);
    }
}

// ---------------- Podcasts ----------------

void drawPodcastsPage(const UI &ui, AppState &state, const std::vector<Podcasts::Show> &shows, bool loading,
                      PhotoLoader &loader) {
    drawTopBar(ui, tr("Podcasts"));
    if (shows.empty()) {
        if (loading) emptyMessage(ui, tr("Loading podcasts..."), "");
        else emptyMessage(ui, tr("No podcasts"), tr("Subscribe from the web page (Settings > Add Media)"));
        return;
    }
    std::vector<ListRow> rows;
    for (const auto &s: shows) {
        std::string detail = !s.error.empty() ? s.error
                             : (s.episodes.size() == 1 ? tr("1 episode")
                                                       : trf("{} episodes", {std::to_string(s.episodes.size())}));
        SDL_Texture *art = nullptr;
        if (!s.artworkPath.empty()) art = loader.thumbnail(MediaFile{s.artworkPath, s.title, 0, 0});
        rows.push_back({s.title, detail, true, false, art});
    }
    drawList(ui, state, rows, 0, ui.w, ROW_HEIGHT);
}

void drawEpisodesPage(const UI &ui, AppState &state, const Podcasts::Show &show) {
    drawTopBar(ui, show.title);
    if (show.episodes.empty()) {
        emptyMessage(ui, tr("No episodes"), "");
        return;
    }
    std::vector<ListRow> rows;
    for (const auto &e: show.episodes) {
        std::string detail = e.date;
        if (!e.duration.empty()) detail += (detail.empty() ? "" : "  \xC2\xB7  ") + e.duration;
        rows.push_back({e.title, detail});
    }
    drawList(ui, state, rows, 0, ui.w, ROW_HEIGHT);
}
