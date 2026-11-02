#include "ui/Utils.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#if !defined(_WIN32)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#endif

SDL_Texture *renderText(SDL_Renderer *r, TTF_Font *font, const std::string &text, SDL_Color color) {
    if (text.empty() || !font) return nullptr;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return nullptr;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}

std::string ellipsize(TTF_Font *font, const std::string &text, int maxWidth) {
    int w = 0;
    if (maxWidth <= 0 || !font || TTF_SizeUTF8(font, text.c_str(), &w, nullptr) != 0 || w <= maxWidth) return text;
    static const std::string dots = "\xE2\x80\xA6"; // "…"
    std::string cut = text;
    while (!cut.empty()) {
        // Drop one UTF-8 code point, then any trailing spaces.
        do { cut.pop_back(); } while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80);
        if (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0x80)) cut.pop_back();
        while (!cut.empty() && cut.back() == ' ') cut.pop_back();
        std::string candidate = cut + dots;
        if (TTF_SizeUTF8(font, candidate.c_str(), &w, nullptr) == 0 && w <= maxWidth) return candidate;
    }
    return dots;
}

SDL_Point drawText(SDL_Renderer *r, TTF_Font *font, const std::string &text, SDL_Color color,
                   int x, int y, int align, int maxWidth) {
    SDL_Texture *t = renderText(r, font, ellipsize(font, text, maxWidth), color);
    if (!t) return {0, 0};
    int w, h;
    SDL_QueryTexture(t, nullptr, nullptr, &w, &h);

    int drawW = (maxWidth > 0) ? std::min(w, maxWidth) : w;
    SDL_Rect src{0, 0, drawW, h};
    int dx = align == 1 ? x - drawW / 2 : align == 2 ? x - drawW : x;
    SDL_Rect dst{dx, y, drawW, h};
    SDL_RenderCopy(r, t, &src, &dst);
    SDL_DestroyTexture(t);
    return {drawW, h};
}

static void drawBattery(SDL_Renderer *r, int x, int yPos, int percent) {
    constexpr int batteryWidth = 24;
    constexpr int batteryHeight = 11;

    SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
    SDL_Rect tip{x + batteryWidth, yPos + batteryHeight / 4, 2, batteryHeight / 2};
    SDL_RenderFillRect(r, &tip);
    SDL_Rect outline{x, yPos, batteryWidth, batteryHeight};
    SDL_RenderDrawRect(r, &outline);

    int fillWidth = (batteryWidth - 2) * std::clamp(percent, 0, 100) / 100;
    bool low = percent <= 20;
    SDL_Color top = low ? SDL_Color{0xf0, 0x9a, 0x8a, 255} : SDL_Color{0xa9, 0xd3, 0xa4, 255};
    SDL_Color bottom = low ? SDL_Color{0xb0, 0x30, 0x20, 255} : SDL_Color{0x44, 0x78, 0x4e, 255};

    for (int y = 0; y < batteryHeight - 2; y++) {
        float t = float(y) / float(batteryHeight - 2);
        SDL_SetRenderDrawColor(r,
                               Uint8(top.r * (1 - t) + bottom.r * t),
                               Uint8(top.g * (1 - t) + bottom.g * t),
                               Uint8(top.b * (1 - t) + bottom.b * t), 255);
        if (fillWidth > 0) SDL_RenderDrawLine(r, x + 1, yPos + 1 + y, x + fillWidth, yPos + 1 + y);
    }
}

static void drawPlayIndicator(SDL_Renderer *r, int x, int y, PlayIndicator play) {
    SDL_SetRenderDrawColor(r, 0x2a, 0x6e, 0xd8, 255);
    if (play == PlayIndicator::Playing) {
        // Right-pointing triangle, 8px tall
        for (int i = 0; i < 8; i++) {
            int half = i < 4 ? i : 7 - i;
            SDL_RenderDrawLine(r, x, y + i, x + half * 2, y + i);
        }
    } else if (play == PlayIndicator::Paused) {
        SDL_Rect a{x, y, 3, 8}, b{x + 5, y, 3, 8};
        SDL_RenderFillRect(r, &a);
        SDL_RenderFillRect(r, &b);
    }
}

void drawTopBar(const UI &ui, const std::string &title, int width) {
    SDL_Renderer *r = ui.r;
    if (width < 0) width = ui.w;

    for (int y = 0; y < TOP_BAR_HEIGHT; y++) {
        float t = float(y) / float(TOP_BAR_HEIGHT - 1);
        Uint8 shade = Uint8(245 - t * t * 50);
        SDL_SetRenderDrawColor(r, shade, shade, shade, 255);
        SDL_RenderDrawLine(r, 0, y, width, y);
    }
    SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
    SDL_RenderDrawLine(r, 0, TOP_BAR_HEIGHT - 1, width, TOP_BAR_HEIGHT - 1);

    // iPod classic layout: title on the left, play state and battery on the right.
    TTF_Font *titleFont = ui.titleFont ? ui.titleFont : (ui.bold ? ui.bold : ui.font);
    int textH = TTF_FontHeight(titleFont);
    drawText(r, titleFont, title, TEXT_COLOR, 8, (TOP_BAR_HEIGHT - textH) / 2, 0, width - 70);

    int batteryX = ui.w - 26 - 6;
    drawBattery(r, batteryX, (TOP_BAR_HEIGHT - 11) / 2, ui.battery < 0 ? 100 : ui.battery);
    drawPlayIndicator(r, batteryX - 14, (TOP_BAR_HEIGHT - 8) / 2, ui.play);
}

void drawHighlight(SDL_Renderer *r, const SDL_Rect &rect) {
    for (int i = 0; i < rect.h; i++) {
        float t = i / (float) rect.h;
        SDL_SetRenderDrawColor(r, Uint8(20 + t * 20), Uint8(120 + t * 60), 255, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y + i, rect.x + rect.w - 1, rect.y + i);
    }
    SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
    SDL_RenderDrawLine(r, rect.x, rect.y, rect.x + rect.w - 1, rect.y);
}

void drawList(const UI &ui, AppState &state, const std::vector<ListRow> &rows, int x, int width, int rowHeight) {
    SDL_Renderer *r = ui.r;
    int areaH = ui.h - TOP_BAR_HEIGHT;
    int visible = std::max(1, areaH / rowHeight);
    int count = static_cast<int>(rows.size());

    // Scroll only when the selection leaves the visible window; visualOffset eases towards it.
    if (state.selected < state.scrollTop) state.scrollTop = state.selected;
    if (state.selected >= state.scrollTop + visible) state.scrollTop = state.selected - visible + 1;
    state.scrollTop = std::clamp(state.scrollTop, 0, std::max(0, count - visible));
    state.visualOffset += (state.scrollTop - state.visualOffset) * 0.35f;
    if (std::fabs(state.scrollTop - state.visualOffset) < 0.01f) state.visualOffset = static_cast<float>(state.scrollTop);

    SDL_Rect clip{x, TOP_BAR_HEIGHT, width, areaH};
    SDL_RenderSetClipRect(r, &clip);

    TTF_Font *labelFont = ui.listFont ? ui.listFont : ui.font;
    int lineH = TTF_FontHeight(labelFont);
    int smallH = TTF_FontHeight(ui.small);
    for (int i = 0; i < count; i++) {
        int y = TOP_BAR_HEIGHT + static_cast<int>(std::lround((i - state.visualOffset) * rowHeight));
        if (y + rowHeight < TOP_BAR_HEIGHT || y > ui.h) continue;

        const ListRow &row = rows[i];
        bool selected = i == state.selected;
        if (selected) drawHighlight(r, SDL_Rect{x, y, width, rowHeight});

        bool twoLines = !row.detail.empty() && !row.detailRight;
        // The second line sits just below the title's baseline, not a full line height lower.
        int secondLine = TTF_FontAscent(labelFont) + 2;
        int textY = twoLines ? y + (rowHeight - secondLine - smallH) / 2 : y + (rowHeight - lineH) / 2;
        int rightEdge = x + width - 8;

        if (row.arrow && ui.arrow) {
            SDL_Rect dst{rightEdge - 8, y + (rowHeight - 12) / 2, 8, 12};
            SDL_SetTextureColorMod(ui.arrow, selected ? 255 : 90, selected ? 255 : 90, selected ? 255 : 90);
            SDL_RenderCopy(r, ui.arrow, nullptr, &dst);
            rightEdge -= 14;
        }

        if (row.icon) {
            int size = rowHeight - 6;
            SDL_Rect dst{rightEdge - size, y + 3, size, size};
            SDL_RenderCopy(r, row.icon, nullptr, &dst);
            rightEdge -= size + 6;
        }

        int detailW = 0;
        if (!row.detail.empty() && row.detailRight) {
            // Align the regular-weight value to the bold label's baseline.
            int baseline = TTF_FontAscent(labelFont) - TTF_FontAscent(ui.font);
            detailW = drawText(r, ui.font, row.detail, selected ? SELECTED_TEXT_COLOR : SUBTEXT_COLOR,
                               rightEdge, textY + baseline, 2).x + 8;
        }

        int maxW = rightEdge - (x + 10) - detailW;
        drawText(r, labelFont, row.label, selected ? SELECTED_TEXT_COLOR : TEXT_COLOR, x + 10, textY, 0, maxW);
        if (twoLines) {
            drawText(r, ui.small, row.detail, selected ? SDL_Color{220, 230, 255, 255} : SUBTEXT_COLOR,
                     x + 10, textY + secondLine, 0, maxW);
        }

    }
    SDL_RenderSetClipRect(r, nullptr);

    // Scrollbar when the list doesn't fit.
    if (count > visible) {
        int trackH = areaH - 4;
        int thumbH = std::max(10, trackH * visible / count);
        int thumbY = TOP_BAR_HEIGHT + 2 + static_cast<int>((trackH - thumbH) * state.visualOffset /
                                                           std::max(1, count - visible));
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_Rect track{x + width - 4, TOP_BAR_HEIGHT + 2, 3, trackH};
        SDL_RenderFillRect(r, &track);
        SDL_SetRenderDrawColor(r, 110, 110, 110, 255);
        SDL_Rect thumb{x + width - 4, thumbY, 3, thumbH};
        SDL_RenderFillRect(r, &thumb);
    }
}

SDL_Color paneColor(int y, int h) {
    float t = float(y) / float(std::max(1, h - 1));
    return {Uint8(0x87 * (1 - t) + 0x45 * t), Uint8(0xa8 * (1 - t) + 0x5d * t),
            Uint8(0xec * (1 - t) + 0xa3 * t), 255};
}

void drawPaneBackground(const UI &ui, int x, int width) {
    for (int y = 0; y < ui.h; y++) {
        SDL_Color c = paneColor(y, ui.h);
        SDL_SetRenderDrawColor(ui.r, c.r, c.g, c.b, 255);
        SDL_RenderDrawLine(ui.r, x, y, x + width - 1, y);
    }
}

void drawPaneShadow(const UI &ui, int x) {
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    constexpr int shadowWidth = 12;
    for (int i = 0; i < shadowWidth; i++) {
        SDL_SetRenderDrawColor(ui.r, 0, 0, 0, Uint8(110 * (1.0f - float(i) / shadowWidth)));
        SDL_Rect s{x + i, TOP_BAR_HEIGHT, 1, ui.h - TOP_BAR_HEIGHT};
        SDL_RenderFillRect(ui.r, &s);
    }
}

void drawStatus(const UI &ui, const std::string &message) {
    if (message.empty()) return;
    int textH = TTF_FontHeight(ui.small);
    int boxH = textH + 8;
    SDL_Rect box{8, ui.h - boxH - 6, ui.w - 16, boxH};
    SDL_SetRenderDrawColor(ui.r, 30, 30, 30, 220);
    SDL_RenderFillRect(ui.r, &box);
    drawText(ui.r, ui.small, message, {255, 255, 255, 255}, ui.w / 2, box.y + 4, 1, box.w - 12);
}

int readBatteryPercent() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root = "/sys/class/power_supply";
    if (!fs::is_directory(root, ec)) return -1;
    for (const auto &entry: fs::directory_iterator(root, ec)) {
        std::ifstream cap(entry.path() / "capacity");
        int value;
        if (cap >> value) return std::clamp(value, 0, 100);
    }
    return -1;
}

std::string formatTime(double seconds) {
    if (seconds < 0) return "--:--";
    int s = static_cast<int>(seconds);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return buf;
}

std::string formatBytes(unsigned long long bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024 && u < 4) {
        v /= 1024;
        u++;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), u == 0 || v >= 10 ? "%.0f %s" : "%.1f %s", v, units[u]);
    return buf;
}

std::string localIpAddress() {
#if defined(_WIN32)
    return {};
#else
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) != 0) return {};
    std::string result;
    for (ifaddrs *it = list; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
        auto *addr = reinterpret_cast<sockaddr_in *>(it->ifa_addr);
        char buf[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf))) continue;
        if (std::string(buf).rfind("127.", 0) == 0) continue;
        result = buf;
        break;
    }
    freeifaddrs(list);
    return result;
#endif
}
