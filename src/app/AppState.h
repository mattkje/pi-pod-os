#pragma once
#include <string>
#include <vector>

enum class Screen {
    MainMenu,
    Language,
    AddMedia,
    Videos,
    VideoPlayback,
    Photos,
    PhotoViewer,
    Podcasts,
    PodcastEpisodes,
    Music,
    NowPlaying,
    Settings,
    About,
    ComingSoon
};

enum class SettingsInputMode {
    None,
    JellyfinUrl,
    JellyfinUser,
    JellyfinPass
};

enum class RepeatMode {
    Off,
    One,
    All
};

enum class MenuAction {
    Navigate,     // open `next` screen
    ShuffleSongs, // shuffle the whole library and start playing
    NowPlaying    // jump to the now playing screen
};

// Glyph shown in the main menu's right pane for the highlighted entry (see Icons.h).
enum class MenuIcon {
    Music,
    Videos,
    Photos,
    Podcasts,
    Extras,
    Settings,
    Shuffle,
    NowPlaying,
    About,
    Repeat, // status icon on Now Playing, not a menu entry
    Count
};

struct MenuItem {
    std::string label;
    Screen next;
    MenuAction action = MenuAction::Navigate;
    MenuIcon icon = MenuIcon::Music;
};

// Persisted user settings (see Config.h).
struct Settings {
    bool setupDone = false;  // first-boot Setup Assistant completed
    std::string language = "en";
    std::string deviceName = "PiPod";
    std::string timezone;    // as chosen in Setup ("Europe/Oslo"); empty = system default
    bool screenFlipped = false; // turn the (always landscape) picture 180 degrees

    int volume = 75; // 0..100
    bool shuffle = false;
    RepeatMode repeat = RepeatMode::Off;

    std::string jellyfinUrl;
    std::string jellyfinUser;
    std::string jellyfinUserId;
    std::string jellyfinToken;

    std::vector<std::string> podcastFeeds; // RSS feed URLs
};

struct NavEntry {
    Screen screen;
    int selected;
    int scrollTop;
    std::string title;
};

struct AppState {
    // Navigation
    Screen current = Screen::MainMenu;
    std::string title = "iPod"; // title shown in top bar for generic pages
    int selected = 0;
    int scrollTop = 0;         // first visible row in list pages
    float visualOffset = 0.0f; // animated scroll position, eases towards scrollTop
    std::vector<NavEntry> history;

    Settings settings;
    bool deviceMode = false; // running as the OS shell on a Pi (PIPOD_DEVICE=1): power options, no Esc-to-quit

    // Software update (device mode). updateDetail is shown next to the "Update" row.
    enum class UpdateState { Idle, Checking, Available, Installing, UpToDate, Failed } updateState = UpdateState::Idle;
    std::string updateDetail;
    std::string updateVersion; // newest version found by the last check

    // Settings -> Jellyfin setup (transient input buffers)
    SettingsInputMode inputMode = SettingsInputMode::None;
    std::string inputUrl;
    std::string inputUser;
    std::string inputPass;

    // Short status message shown at the bottom of the screen (e.g. "Connecting...")
    std::string statusMessage;
    unsigned int statusUntil = 0; // SDL ticks; 0 = sticky

    void navigate(Screen screen, const std::string &newTitle = "") {
        history.push_back({current, selected, scrollTop, title});
        current = screen;
        selected = 0;
        scrollTop = 0;
        visualOffset = 0.0f;
        if (!newTitle.empty()) title = newTitle;
    }

    bool back() {
        if (history.empty()) return false;
        NavEntry e = history.back();
        history.pop_back();
        current = e.screen;
        selected = e.selected;
        scrollTop = e.scrollTop;
        visualOffset = static_cast<float>(e.scrollTop);
        title = e.title;
        return true;
    }
};
