#include "ui/pages/SettingsPage.h"
#include <string>
#include "app/I18n.h"

constexpr int ROW_HEIGHT = 23;

int settingsItemCount(const AppState &state) {
    return static_cast<int>(state.deviceMode ? SettingsItem::Count : SettingsItem::Update);
}

std::vector<ListRow> settingsRows(const AppState &state) {
    const Settings &s = state.settings;
    std::string repeat = tr(s.repeat == RepeatMode::One ? "One" : s.repeat == RepeatMode::All ? "All" : "Off");
    std::string language = "English";
    for (const auto &l: I18n::languages())
        if (l.code == I18n::current()) language = l.nativeName;
    bool connected = !s.jellyfinToken.empty();

    std::vector<ListRow> rows(static_cast<size_t>(SettingsItem::Count));
    rows[size_t(SettingsItem::Update)] = {tr("Update"), state.updateDetail == "New" ? tr("New") : state.updateDetail, false, true};
    rows[size_t(SettingsItem::Restart)] = {tr("Restart"), "", false, false};
    rows[size_t(SettingsItem::ShutDown)] = {tr("Shut Down"), "", false, false};
    rows[size_t(SettingsItem::Language)] = {tr("Language"), language, false, true};
    rows[size_t(SettingsItem::FlipScreen)] = {tr("Flip Screen"), tr(s.screenFlipped ? "On" : "Off"), false, true};
    rows[size_t(SettingsItem::SetupAssistant)] = {tr("Setup Assistant"), "", true, false};
    rows[size_t(SettingsItem::AddMedia)] = {tr("Add Media"), "", true, false};
    rows[size_t(SettingsItem::Volume)] = {tr("Volume"), std::to_string(s.volume) + "%", false, true};
    rows[size_t(SettingsItem::Shuffle)] = {tr("Shuffle"), tr(s.shuffle ? "On" : "Off"), false, true};
    rows[size_t(SettingsItem::Repeat)] = {tr("Repeat"), repeat, false, true};
    rows[size_t(SettingsItem::Jellyfin)] = connected
                                               ? ListRow{"Jellyfin", tr("On"), false, true}
                                               : ListRow{"Jellyfin", "", true, false};
    rows[size_t(SettingsItem::RescanLibrary)] = {tr("Rescan Library"), "", false, false};
    rows[size_t(SettingsItem::Reset)] = {tr("Reset"), "", false, false};
    rows.resize(static_cast<size_t>(settingsItemCount(state)));
    return rows;
}

namespace {
    std::string hostOf(const std::string &url) {
        std::string h = url;
        if (auto p = h.find("://"); p != std::string::npos) h = h.substr(p + 3);
        if (auto p = h.find('/'); p != std::string::npos) h = h.substr(0, p);
        return h;
    }

    std::string hintFor(const AppState &state) {
        const Settings &s = state.settings;
        switch (static_cast<SettingsItem>(state.selected)) {
            case SettingsItem::Volume: return tr("Left/Right to adjust");
            case SettingsItem::Shuffle: return tr("Shuffle when playing a list");
            case SettingsItem::Repeat: return tr("Off, One or All");
            case SettingsItem::Language: return tr("Choose a language");
            case SettingsItem::FlipScreen: return tr("Turn the picture upside down");
            case SettingsItem::Jellyfin:
                return tr(s.jellyfinToken.empty() ? "Select to log in" : "Select to disconnect");
            case SettingsItem::RescanLibrary: return tr("Look for new music files");
            case SettingsItem::SetupAssistant: return tr("Name, Wi-Fi, time zone...");
            case SettingsItem::AddMedia: return tr("Upload music, videos and photos from another device");
            case SettingsItem::Reset: return tr("Forget settings and cache");
            case SettingsItem::Update:
                switch (state.updateState) {
                    case AppState::UpdateState::Checking: return tr("Checking for updates...");
                    case AppState::UpdateState::Available: return trf("Select to install {}", {state.updateVersion});
                    case AppState::UpdateState::Installing: return trf("Installing {}...", {state.updateVersion});
                    case AppState::UpdateState::UpToDate: return tr("PiPod OS is up to date");
                    case AppState::UpdateState::Failed: return tr("Update failed");
                    default: return tr("Select to check for updates");
                }
            case SettingsItem::Restart: return tr("Reboot the PiPod");
            case SettingsItem::ShutDown: return tr("Power off safely");
            default: return "";
        }
    }

    // Status pane: replaces the iPod's storage capacity readout with things that are actually
    // useful on a Pi (library size, cache, network address, server).
    void drawStatusPane(const UI &ui, const AppState &state, const DeviceInfo &info, int x, int w) {
        SDL_Renderer *r = ui.r;
        drawPaneBackground(ui, x, w);

        const SDL_Color white{255, 255, 255, 255};
        const SDL_Color dim{215, 228, 255, 255};
        int pad = 12;
        int lineH = TTF_FontHeight(ui.small) + 3;
        int y = TOP_BAR_HEIGHT + 10;

        int total = info.localSongs + info.jellyfinSongs;
        y += drawText(r, ui.font, (total == 1 ? tr("1 song") : trf("{} songs", {std::to_string(total)})), white,
                      x + pad, y, 0, w - 2 * pad).y + 2;

        // Library bar: share of local vs. Jellyfin songs (instead of a capacity bar)
        SDL_Rect bar{x + pad, y + 2, w - 2 * pad, 8};
        SDL_SetRenderDrawColor(r, 255, 255, 255, 70);
        SDL_RenderFillRect(r, &bar);
        if (total > 0) {
            int localW = bar.w * info.localSongs / total;
            SDL_Rect a{bar.x, bar.y, localW, bar.h};
            SDL_SetRenderDrawColor(r, 255, 255, 255, 235);
            SDL_RenderFillRect(r, &a);
            SDL_Rect b{bar.x + localW, bar.y, bar.w - localW, bar.h};
            SDL_SetRenderDrawColor(r, 0xff, 0xc4, 0x4d, 235);
            SDL_RenderFillRect(r, &b);
        }
        y += 16;

        auto row = [&](const std::string &label, const std::string &value) {
            drawText(r, ui.small, label, dim, x + pad, y);
            int labelW = 0;
            TTF_SizeUTF8(ui.small, label.c_str(), &labelW, nullptr);
            drawText(r, ui.small, value, white, x + w - pad, y, 2, w - 2 * pad - labelW - 6);
            y += lineH;
        };
        row(tr("Local"), std::to_string(info.localSongs));
        row("Jellyfin", std::to_string(info.jellyfinSongs));
        row(tr("Cache"), info.cacheSize.empty() ? "0 B" : info.cacheSize);
        y += 4;
        row("IP", info.ipAddress.empty() ? tr("Offline") : info.ipAddress);
        const Settings &s = state.settings;
        row(tr("Server"), s.jellyfinToken.empty() ? tr("None") : hostOf(s.jellyfinUrl));
        if (!s.jellyfinToken.empty()) row(tr("User"), s.jellyfinUser);

        // Hint for the highlighted setting, pinned to the bottom
        std::string hint = hintFor(state);
        int hintY = ui.h - TTF_FontHeight(ui.small) - 8;
        SDL_SetRenderDrawColor(r, 0, 0, 0, 50);
        SDL_Rect hintBg{x, hintY - 4, w, ui.h - hintY + 4};
        SDL_RenderFillRect(r, &hintBg);
        drawText(r, ui.small, hint, white, x + w / 2, hintY, 1, w - 2 * pad);
    }
}

void drawSettingsPage(const UI &ui, AppState &state, const DeviceInfo &info) {
    int half = ui.w / 2;
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    drawStatusPane(ui, state, info, half, ui.w - half);
    drawTopBar(ui, tr("Settings"));
    drawList(ui, state, settingsRows(state), 0, half, ROW_HEIGHT);
    drawPaneShadow(ui, half);

    if (state.inputMode == SettingsInputMode::None) return;

    // ------------------ Jellyfin login dialog ------------------
    SDL_Renderer *r = ui.r;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 120);
    SDL_Rect dim{0, TOP_BAR_HEIGHT, ui.w, ui.h - TOP_BAR_HEIGHT};
    SDL_RenderFillRect(r, &dim);

    int lineH = TTF_FontHeight(ui.font);
    int smallH = TTF_FontHeight(ui.small);
    int boxH = 16 + lineH + 8 + lineH + 12 + smallH + 12;
    SDL_Rect box{16, TOP_BAR_HEIGHT + (ui.h - TOP_BAR_HEIGHT - boxH) / 2, ui.w - 32, boxH};
    SDL_SetRenderDrawColor(r, 245, 245, 245, 255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
    SDL_RenderDrawRect(r, &box);

    std::string label, value;
    switch (state.inputMode) {
        case SettingsInputMode::JellyfinUrl:
            label = tr("Server address") + " (1/3)";
            value = state.inputUrl;
            break;
        case SettingsInputMode::JellyfinUser:
            label = tr("Username") + " (2/3)";
            value = state.inputUser;
            break;
        case SettingsInputMode::JellyfinPass:
            label = tr("Password") + " (3/3)";
            value = std::string(state.inputPass.size(), '*');
            break;
        default: break;
    }

    int y = box.y + 12;
    drawText(r, ui.font, label, TEXT_COLOR, box.x + 12, y);
    y += lineH + 8;

    SDL_Rect field{box.x + 10, y - 2, box.w - 20, lineH + 4};
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderFillRect(r, &field);
    SDL_SetRenderDrawColor(r, 120, 150, 220, 255);
    SDL_RenderDrawRect(r, &field);

    // Show the tail of long values so the cursor stays visible.
    int maxW = field.w - 12;
    std::string shown = value;
    int w = 0;
    while (!shown.empty() && TTF_SizeUTF8(ui.font, (shown + "|").c_str(), &w, nullptr) == 0 && w > maxW)
        shown.erase(0, 1);
    bool cursor = (SDL_GetTicks() / 500) % 2 == 0;
    drawText(r, ui.font, shown + (cursor ? "|" : ""), TEXT_COLOR, field.x + 6, y);
    y += lineH + 12;

    drawText(r, ui.small, tr("Enter: next   Esc: back"), SUBTEXT_COLOR, box.x + box.w / 2, y, 1);
}
