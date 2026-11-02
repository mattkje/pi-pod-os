#include "ui/pages/SongsPage.h"
#include "app/I18n.h"

constexpr int ROW_HEIGHT = 36;

void drawSongsPage(const UI &ui, AppState &state, const std::vector<Song> &songs, bool loading) {
    drawTopBar(ui, tr("Songs"));

    if (songs.empty()) {
        int y = ui.h / 2 - 20;
        drawText(ui.r, ui.font, tr(loading ? "Loading library..." : "No songs"), TEXT_COLOR, ui.w / 2, y, 1);
        if (!loading)
            drawText(ui.r, ui.small, tr("Add music to ~/Music or connect Jellyfin"), SUBTEXT_COLOR,
                     ui.w / 2, y + 26, 1, ui.w - 20);
        return;
    }

    std::vector<ListRow> rows;
    rows.reserve(songs.size());
    for (const auto &s: songs) rows.push_back({s.title, s.artist, false, false, s.artwork.get()});
    drawList(ui, state, rows, 0, ui.w, ROW_HEIGHT);
}
