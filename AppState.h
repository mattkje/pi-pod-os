#pragma once
#include <string>

enum class Screen {
    MainMenu,
    Music,
    Video,
    Settings,
    About
};

enum class SettingsInputMode {
    None,
    JellyfinUrl,
    JellyfinUser,
    JellyfinPass
};

struct MenuItem {
    std::string label;
    Screen next;
};

struct AppState {
    // Navigation
    Screen current = Screen::MainMenu;
    int selected = 0;
    float visualOffset = 0.0f;

    // Settings → Jellyfin setup
    SettingsInputMode inputMode = SettingsInputMode::None;
    std::string jellyfinUrl;
    std::string jellyfinUser;
    std::string jellyfinPass;
};
