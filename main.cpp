#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <filesystem>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>

namespace fs = std::filesystem;

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 320;
constexpr int ITEM_HEIGHT = 36;
constexpr int TOP_BAR_HEIGHT = 32;

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

// iPod-style colors
const SDL_Color BACKGROUND_COLOR = {235, 235, 235, 255};
const SDL_Color TEXT_COLOR = {40, 40, 40, 255};
const SDL_Color SELECTED_TEXT_COLOR = {255, 255, 255, 255};
const SDL_Color SEPARATOR_COLOR = {200, 200, 200, 255};

struct Song {
    std::string filePath;
    std::string title;
    std::string artist;
    SDL_Texture *artwork = nullptr; // optional
};

// ------------------ UTILITY FUNCTIONS ------------------
SDL_Texture *renderText(SDL_Renderer *r, TTF_Font *font, const std::string &text, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void drawTopBar(SDL_Renderer *r, TTF_Font *font, const std::string &title) {
    for (int y = 0; y < TOP_BAR_HEIGHT; y++) {
        Uint8 shade = 245 - y;
        SDL_SetRenderDrawColor(r, shade, shade, shade, 255);
        SDL_RenderDrawLine(r, 0, y, SCREEN_WIDTH, y);
    }
    SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
    SDL_RenderDrawLine(r, 0, TOP_BAR_HEIGHT - 1, SCREEN_WIDTH, TOP_BAR_HEIGHT - 1);

    SDL_Texture *text = renderText(r, font, title, TEXT_COLOR);
    int w, h;
    SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
    SDL_Rect dst{12, (TOP_BAR_HEIGHT - h) / 2, w, h};
    SDL_RenderCopy(r, text, nullptr, &dst);
    SDL_DestroyTexture(text);
}

void drawHighlight(SDL_Renderer *r, const SDL_Rect &rect) {
    for (int i = 0; i < rect.h; i++) {
        float t = i / (float) rect.h;
        Uint8 rCol = (Uint8) (20 + t * 20);
        Uint8 gCol = (Uint8) (120 + t * 60);
        Uint8 bCol = 255;
        SDL_SetRenderDrawColor(r, rCol, gCol, bCol, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y + i, rect.x + rect.w, rect.y + i);
    }
    SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
    SDL_RenderDrawLine(r, rect.x, rect.y, rect.x + rect.w, rect.y);
}

void drawMenu(SDL_Renderer *r, TTF_Font *font,
              AppState &state,
              const std::vector<MenuItem> &items) {
    int centerY = (SCREEN_HEIGHT + TOP_BAR_HEIGHT) / 2;

    state.visualOffset += (state.selected - state.visualOffset) * 0.15f;

    for (int i = 0; i < (int) items.size(); i++) {
        float y = centerY + (i - state.visualOffset) * ITEM_HEIGHT;

        if (y < TOP_BAR_HEIGHT - ITEM_HEIGHT ||
            y > SCREEN_HEIGHT + ITEM_HEIGHT)
            continue;

        bool selected = (i == state.selected);

        if (selected) {
            SDL_Rect hl{0, (int) y - 4, SCREEN_WIDTH, ITEM_HEIGHT};
            drawHighlight(r, hl);
        }

        SDL_Color color = selected ? SELECTED_TEXT_COLOR : TEXT_COLOR;
        SDL_Texture *text = renderText(r, font, items[i].label, color);

        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);

        SDL_Rect dst{24, (int) y, w, h};
        SDL_RenderCopy(r, text, nullptr, &dst);
        SDL_DestroyTexture(text);

        if (selected) {
            SDL_Texture *arrow = IMG_LoadTexture(r, "assets/icons/arrow.png");
            if (!arrow) {
                SDL_Log("Failed to load arrow.png: %s", IMG_GetError());
            }

            // Keep aspect ratio
            int texW, texH;
            SDL_QueryTexture(arrow, nullptr, nullptr, &texW, &texH);
            float scale = (float) ITEM_HEIGHT / texH; // scale to ITEM_HEIGHT
            int w = (int) (texW * scale) / 2;
            int h = (int) (texH * scale) / 2;

            SDL_Rect adst{
                SCREEN_WIDTH - w - 16, // right padding
                (int) y + (ITEM_HEIGHT - h) / 2, // vertically center
                w,
                h
            };

            SDL_RenderCopy(r, arrow, nullptr, &adst);
            SDL_DestroyTexture(arrow);
        } else {
            SDL_SetRenderDrawColor(r,
                                   SEPARATOR_COLOR.r,
                                   SEPARATOR_COLOR.g,
                                   SEPARATOR_COLOR.b,
                                   255);
            SDL_RenderDrawLine(r,
                               12,
                               (int) y + ITEM_HEIGHT - 2,
                               SCREEN_WIDTH - 12,
                               (int) y + ITEM_HEIGHT - 2);
        }
    }
}

void drawSettingsPage(SDL_Renderer *r, TTF_Font *font) {
    drawTopBar(r, font, "Settings");

    int y = TOP_BAR_HEIGHT + 24;
    auto drawCentered = [&](const std::string &text, SDL_Color color) {
        SDL_Texture *t = renderText(r, font, text, color);
        int w, h;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        SDL_Rect dst{(SCREEN_WIDTH - w) / 2, y, w, h};
        SDL_RenderCopy(r, t, nullptr, &dst);
        SDL_DestroyTexture(t);
        y += h + 10;
    };

    drawCentered("Option 1: TBD", TEXT_COLOR);
    drawCentered("Option 2: TBD", TEXT_COLOR);
    drawCentered("Option 3: TBD", TEXT_COLOR);
}


void drawAboutPage(SDL_Renderer *r, TTF_Font *font) {
    drawTopBar(r, font, "About");

    int y = TOP_BAR_HEIGHT + 24;

    auto drawCentered = [&](const std::string &text, SDL_Color color) {
        SDL_Texture *t = renderText(r, font, text, color);
        int w, h;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        SDL_Rect dst{(SCREEN_WIDTH - w) / 2, y, w, h};
        SDL_RenderCopy(r, t, nullptr, &dst);
        SDL_DestroyTexture(t);
        y += h + 10;
    };

    // Device name
    drawCentered("PiPod Classic", {40, 40, 40, 255});

    y += 6;
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawLine(r, 60, y, SCREEN_WIDTH - 60, y);
    y += 16;

    // Version info
    drawCentered("PiPod OS 0.0.1", {60, 60, 60, 255});
    drawCentered("Firmware 1A543", {60, 60, 60, 255});

    y += 12;
    SDL_RenderDrawLine(r, 80, y, SCREEN_WIDTH - 80, y);
    y += 16;

    // Storage info (fake but believable)
    drawCentered("Storage: 30 GB", {80, 80, 80, 255});
    drawCentered("Songs: 1,284", {80, 80, 80, 255});

    y += 24;

    // Copyright
    drawCentered("© 2025 Shelstad Studios", {120, 120, 120, 255});
    drawCentered("All Rights Reserved", {120, 120, 120, 255});
}

const char *get_asset_path(const char *relative) {
    static char path[1024];
    char *base = SDL_GetBasePath();
    snprintf(path, sizeof(path), "%s%s", base, relative);
    SDL_free(base);
    return path;
}

// ------------------ LOAD SONGS ------------------
std::vector<Song> loadSongs(SDL_Renderer *renderer) {
    std::vector<Song> songs;

    fs::path musicDir = "assets/music";
    fs::path artworkDir = musicDir / "artwork";

    for (const auto &entry: fs::directory_iterator(musicDir)) {
        if (entry.path().extension() == ".mp3") {
            TagLib::FileRef f(entry.path().c_str());
            Song s;
            s.filePath = entry.path();

            if (!f.isNull() && f.tag()) {
                TagLib::Tag *tag = f.tag();
                s.title = tag->title().isEmpty() ? entry.path().stem().string() : tag->title().to8Bit(true);
                s.artist = tag->artist().to8Bit(true);
            } else {
                s.title = entry.path().stem().string();
                s.artist = "Unknown";
            }

            // optional artwork: assets/music/artwork/songname.png
            fs::path artPath = artworkDir / (entry.path().stem().string() + ".png");
            if (fs::exists(artPath)) {
                s.artwork = IMG_LoadTexture(renderer, artPath.c_str());
            }

            songs.push_back(s);
        }
    }

    return songs;
}

// ------------------ DRAW SONGS MENU ------------------
void drawSongsMenu(SDL_Renderer *r, TTF_Font *font, AppState &state, const std::vector<Song> &songs) {
    int centerY = (SCREEN_HEIGHT + TOP_BAR_HEIGHT) / 2;
    state.visualOffset += (state.selected - state.visualOffset) * 0.15f;

    for (int i = 0; i < (int) songs.size(); ++i) {
        float y = centerY + (i - state.visualOffset) * ITEM_HEIGHT;
        if (y < TOP_BAR_HEIGHT - ITEM_HEIGHT || y > SCREEN_HEIGHT + ITEM_HEIGHT)
            continue;

        bool selected = (i == state.selected);
        if (selected) drawHighlight(r, SDL_Rect{0, (int) y - 4, SCREEN_WIDTH, ITEM_HEIGHT});

        // title
        SDL_Texture *titleTex = renderText(r, font, songs[i].title, selected ? SELECTED_TEXT_COLOR : TEXT_COLOR);
        int tw, th;
        SDL_QueryTexture(titleTex, nullptr, nullptr, &tw, &th);
        SDL_Rect titleRect{24, (int) y, tw, th};
        SDL_RenderCopy(r, titleTex, nullptr, &titleRect);
        SDL_DestroyTexture(titleTex);

        // artist
        SDL_Texture *artistTex = renderText(r, font, songs[i].artist, {120, 120, 120, 255});
        int aw, ah;
        SDL_QueryTexture(artistTex, nullptr, nullptr, &aw, &ah);
        SDL_Rect artistRect{24, (int) y + th, aw, ah};
        SDL_RenderCopy(r, artistTex, nullptr, &artistRect);
        SDL_DestroyTexture(artistTex);

        // artwork
        if (songs[i].artwork) {
            int artW, artH;
            SDL_QueryTexture(songs[i].artwork, nullptr, nullptr, &artW, &artH);
            float scale = (float) ITEM_HEIGHT / artH * 0.8f; // smaller than item height
            int w = (int) (artW * scale);
            int h = (int) (artH * scale);
            SDL_Rect artRect{SCREEN_WIDTH - w - 8, (int) y + (ITEM_HEIGHT - h) / 2, w, h};
            SDL_RenderCopy(r, songs[i].artwork, nullptr, &artRect);
        }

        // arrow
        if (selected) {
            SDL_Texture *arrow = IMG_LoadTexture(r, "assets/icons/arrow.png");
            if (arrow) {
                int texW, texH;
                SDL_QueryTexture(arrow, nullptr, nullptr, &texW, &texH);
                float scale = (float) ITEM_HEIGHT / texH / 2; // smaller
                int w = (int) (texW * scale);
                int h = (int) (texH * scale);
                SDL_Rect adst{SCREEN_WIDTH - w - 16, (int) y + (ITEM_HEIGHT - h) / 2, w, h};
                SDL_RenderCopy(r, arrow, nullptr, &adst);
                SDL_DestroyTexture(arrow);
            }
        }
    }
}

// ------------------ MUSIC SCREEN ------------------
void drawMusicScreen(SDL_Renderer *r, TTF_Font *font, const Song *currentSong = nullptr) {
    drawTopBar(r, font, "Now Playing");

    if (currentSong) {
        SDL_Texture *title = renderText(r, font, currentSong->title, TEXT_COLOR);
        SDL_Texture *artist = renderText(r, font, currentSong->artist, TEXT_COLOR);

        int tw, th, aw, ah;
        SDL_QueryTexture(title, nullptr, nullptr, &tw, &th);
        SDL_QueryTexture(artist, nullptr, nullptr, &aw, &ah);

        SDL_Rect tdst{(SCREEN_WIDTH - tw) / 2, 70, tw, th};
        SDL_Rect adst{(SCREEN_WIDTH - aw) / 2, 100, aw, ah};

        SDL_RenderCopy(r, title, nullptr, &tdst);
        SDL_RenderCopy(r, artist, nullptr, &adst);

        SDL_DestroyTexture(title);
        SDL_DestroyTexture(artist);

        if (currentSong->artwork) {
            int artW, artH;
            SDL_QueryTexture(currentSong->artwork, nullptr, nullptr, &artW, &artH);
            float scale = 1.0f; // scale artwork to fit nicely
            int w = (int) (artW * scale);
            int h = (int) (artH * scale);
            SDL_Rect rect{(SCREEN_WIDTH - w) / 2, 140, w, h};
            SDL_RenderCopy(r, currentSong->artwork, nullptr, &rect);
        }
    }
}

// ------------------ MAIN ------------------
int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
    }
    Mix_Music *currentMusic = nullptr;


    SDL_Window *window = SDL_CreateWindow("iPodOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                                          SCREEN_HEIGHT, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    TTF_Font *font = TTF_OpenFont(get_asset_path("assets/fonts/MyriadPro-Regular.otf"), 18);

    AppState state;

    std::vector<MenuItem> mainMenu{
        {"Songs", Screen::Music},
        {"Settings", Screen::Settings},
        {"About", Screen::About}
    };

    std::vector<Song> songs = loadSongs(renderer);
    const Song *currentSong = nullptr;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false;
                        break;
                    case SDLK_UP:
                        if (state.current == Screen::MainMenu)
                            state.selected = (state.selected - 1 + mainMenu.size()) % mainMenu.size();
                        else if (state.current == Screen::Music)
                            state.selected = (state.selected - 1 + songs.size()) % songs.size();
                        break;
                    case SDLK_DOWN:
                        if (state.current == Screen::MainMenu)
                            state.selected = (state.selected + 1) % mainMenu.size();
                        else if (state.current == Screen::Music)
                            state.selected = (state.selected + 1) % songs.size();
                        break;
                    case SDLK_RETURN:
                        if (state.current == Screen::MainMenu) {
                            state.current = mainMenu[state.selected].next;
                        } else if (state.current == Screen::Music) {
                            currentSong = &songs[state.selected];
                            // Stop previous music
                            if (currentMusic) Mix_HaltMusic();
                            if (currentMusic) Mix_FreeMusic(currentMusic);

                            currentMusic = Mix_LoadMUS(currentSong->filePath.c_str());
                            if (currentMusic) Mix_PlayMusic(currentMusic, 1);
                        }

                        break;

                    case SDLK_BACKSPACE:
                        state.current = Screen::MainMenu;
                        break;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
        SDL_RenderClear(renderer);

        switch (state.current) {
            case Screen::MainMenu:
                drawTopBar(renderer, font, "iPod");
                drawMenu(renderer, font, state, mainMenu);
                break;
            case Screen::Music:
                if (currentSong)
                    drawMusicScreen(renderer, font, currentSong);
                else
                    drawSongsMenu(renderer, font, state, songs);
                break;
            case Screen::Settings:
                drawSettingsPage(renderer, font);
                break;
            case Screen::About:
                drawAboutPage(renderer, font);
                break;
        }

        SDL_RenderPresent(renderer);
    }

    // cleanup
    for (auto &s: songs) if (s.artwork) SDL_DestroyTexture(s.artwork);

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
