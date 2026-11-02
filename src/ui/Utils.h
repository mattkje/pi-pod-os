#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include "app/AppState.h"

constexpr int TOP_BAR_HEIGHT = 20;

const SDL_Color TEXT_COLOR = {40, 40, 40, 255};
const SDL_Color SUBTEXT_COLOR = {120, 120, 120, 255};
const SDL_Color SELECTED_TEXT_COLOR = {255, 255, 255, 255};
const SDL_Color SEPARATOR_COLOR = {200, 200, 200, 255};
const SDL_Color BACKGROUND_COLOR = {255, 255, 255, 255};

enum class PlayIndicator { None, Playing, Paused };

// Everything a page needs to draw itself.
struct UI {
    SDL_Renderer *r = nullptr;
    TTF_Font *font = nullptr;  // regular text
    TTF_Font *small = nullptr; // secondary text
    TTF_Font *medium = nullptr; // artist / album on Now Playing
    TTF_Font *bold = nullptr;   // titles
    SDL_Texture *arrow = nullptr; // ">" icon for rows that open a submenu
    int w = 320;
    int h = 240;
    int battery = -1; // percent, -1 if no battery
    PlayIndicator play = PlayIndicator::None;
};

struct ListRow {
    std::string label;
    std::string detail;  // optional second line (artist) or right-aligned value
    bool arrow = false;  // draw ">" on the right
    bool detailRight = false; // draw detail right-aligned on the same line instead of below
    SDL_Texture *icon = nullptr; // optional square thumbnail on the right (e.g. album art)
};

SDL_Texture *renderText(SDL_Renderer *renderer, TTF_Font *font, const std::string &text, SDL_Color color);

// Shortens text with a trailing "…" so it fits maxWidth pixels.
std::string ellipsize(TTF_Font *font, const std::string &text, int maxWidth);

// Draws text (ellipsized to maxWidth when > 0) and returns its size. align: 0 = left (x is left edge), 1 = centre, 2 = right (x is right edge).
SDL_Point drawText(SDL_Renderer *r, TTF_Font *font, const std::string &text, SDL_Color color,
                   int x, int y, int align = 0, int maxWidth = 0);

void drawTopBar(const UI &ui, const std::string &title, int width = -1);

void drawHighlight(SDL_Renderer *renderer, const SDL_Rect &rect);

// Scrolling list below the top bar, iPod style. Uses state.selected / state.visualOffset.
void drawList(const UI &ui, AppState &state, const std::vector<ListRow> &rows, int x, int width, int rowHeight);

// Blue gradient used for the right-hand pane of split screens (main menu, settings).
SDL_Color paneColor(int y, int screenHeight);
void drawPaneBackground(const UI &ui, int x, int width);
// Shadow the left list casts onto the pane starting at x.
void drawPaneShadow(const UI &ui, int x);

// Small toast at the bottom of the screen.
void drawStatus(const UI &ui, const std::string &message);

// Battery percentage from /sys/class/power_supply, or -1 if the device has no battery.
int readBatteryPercent();

std::string formatTime(double seconds);

// "1.2 GB", "340 MB", ...
std::string formatBytes(unsigned long long bytes);

// First non-loopback IPv4 address, or empty if offline.
std::string localIpAddress();
