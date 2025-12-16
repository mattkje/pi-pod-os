#pragma once
#include <vector>
#include <string>

enum class Screen {
    MainMenu,
    Music,
    Settings,
    About
};

struct MenuItem {
    std::string label;
    Screen next;
};

struct AppState {
    Screen current = Screen::MainMenu;
    int selected = 0;
    float visualOffset = 0.0f;
};
