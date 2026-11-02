#include "ui/pages/AddMediaPage.h"
#include "app/I18n.h"

namespace {
    // Draws text scaled up (for the big access code); the font only ships in one size.
    void drawBig(const UI &ui, const std::string &text, SDL_Color color, int cx, int y, int scale) {
        SDL_Texture *t = renderText(ui.r, ui.bold ? ui.bold : ui.font, text, color);
        if (!t) return;
        int w, h;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        SDL_Rect dst{cx - w * scale / 2, y, w * scale, h * scale};
        SDL_RenderCopy(ui.r, t, nullptr, &dst);
        SDL_DestroyTexture(t);
    }
}

void drawAddMediaPage(const UI &ui, bool running, const std::string &urlByName, const std::string &urlByIp,
                      const std::string &code) {
    SDL_Renderer *r = ui.r;
    drawTopBar(ui, tr("Add Media"));
    TTF_Font *medium = ui.medium ? ui.medium : ui.font;
    TTF_Font *bold = ui.bold ? ui.bold : ui.font;
    const SDL_Color link{20, 100, 220, 255};
    int cx = ui.w / 2;

    if (!running) {
        drawText(r, ui.font, tr("Web upload isn't available"), TEXT_COLOR, cx, ui.h / 2 - 10, 1, ui.w - 20);
        return;
    }
    if (urlByIp.empty()) {
        drawText(r, ui.font, tr("Connect to Wi-Fi to add media"), TEXT_COLOR, cx, ui.h / 2 - 20, 1, ui.w - 20);
        drawText(r, ui.small, tr("Settings > Setup Assistant"), SUBTEXT_COLOR, cx, ui.h / 2 + 6, 1, ui.w - 20);
        return;
    }

    int y = TOP_BAR_HEIGHT + 10;
    y += drawText(r, ui.small, tr("On your computer or phone, open:"), SUBTEXT_COLOR, cx, y, 1, ui.w - 20).y + 4;
    y += drawText(r, bold, urlByName, link, cx, y, 1, ui.w - 16).y + 1;
    y += drawText(r, medium, tr("or") + " " + urlByIp, link, cx, y, 1, ui.w - 16).y + 12;

    y += drawText(r, ui.small, tr("Access code"), SUBTEXT_COLOR, cx, y, 1).y + 2;
    std::string spaced;
    for (size_t i = 0; i < code.size(); i++) spaced += (i ? " " : "") + code.substr(i, 1);
    drawBig(ui, spaced, TEXT_COLOR, cx, y, 2);

    drawText(r, ui.small, tr("Both devices must be on the same network."), SUBTEXT_COLOR, cx,
             ui.h - TTF_FontHeight(ui.small) - 8, 1, ui.w - 20);
}
