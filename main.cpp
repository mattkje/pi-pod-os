#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include "AppState.h"
#include "Utils.h"
#include "Pages/MenuPage.h"
#include "Pages/SettingsPage.h"
#include "Pages/AboutPage.h"
#include "Pages/MusicPage.h"
#include <vector>
#include <string>

int main(int argc, char **argv) {
    // ------------------ SDL INIT ------------------
    SDL_StartTextInput();
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
        SDL_Log("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());

    SDL_Window *window = SDL_CreateWindow("iPodOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    TTF_Font *font = TTF_OpenFont("assets/fonts/MyriadPro-Regular.otf", 18);

    // ------------------ APP STATE ------------------
    AppState state;
    std::vector<MenuItem> mainMenu{
        {"Music", Screen::Music},
        {"Videos", Screen::Video},
        {"Photos", Screen::Music},
        {"Podcasts", Screen::Music},
        {"Extras", Screen::Music},
        {"Settings", Screen::Settings},
        {"Shuffle Songs", Screen::Settings},
        {"Now Playing", Screen::Music},
        {"About", Screen::About}
    };

    std::vector<Song> songs = loadSongs(renderer);
    const Song *currentSong = nullptr;
    Mix_Music *currentMusic = nullptr;

    // ------------------ MAIN LOOP ------------------
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
                        else if (state.current == Screen::Settings)
                            state.selected = (state.selected - 1 + 6) % 6; // 6 settings items
                        break;
                    case SDLK_DOWN:
                        if (state.current == Screen::MainMenu)
                            state.selected = (state.selected + 1) % mainMenu.size();
                        else if (state.current == Screen::Music)
                            state.selected = (state.selected + 1) % songs.size();
                        else if (state.current == Screen::Settings)
                            state.selected = (state.selected + 1) % 8;
                        break;
                    case SDLK_RETURN:
                        if (state.current == Screen::MainMenu)
                            state.current = mainMenu[state.selected].next;
                        else if (state.current == Screen::Music) {
                            currentSong = &songs[state.selected];
                            if (currentMusic) {
                                Mix_HaltMusic();
                                Mix_FreeMusic(currentMusic);
                            }
                            currentMusic = Mix_LoadMUS(currentSong->filePath.c_str());
                            if (currentMusic) Mix_PlayMusic(currentMusic, 1);
                        } else if (state.current == Screen::Settings) {
                            if (state.selected == 5) {
                                state.inputMode = SettingsInputMode::JellyfinUrl;
                            }
                        }

                        break;
                    case SDLK_BACKSPACE:
                        state.current = Screen::MainMenu;
                        state.selected = 0; // reset selection to top
                        break;
                }
            }
        }

        if (state.inputMode != SettingsInputMode::None && e.type == SDL_TEXTINPUT) {
            std::string *target = nullptr;
            if (state.inputMode == SettingsInputMode::JellyfinUrl) target = &state.jellyfinUrl;
            else if (state.inputMode == SettingsInputMode::JellyfinUser) target = &state.jellyfinUser;
            else if (state.inputMode == SettingsInputMode::JellyfinPass) target = &state.jellyfinPass;

            if (target) target->append(e.text.text);
        }

        if (state.inputMode != SettingsInputMode::None && e.type == SDL_KEYDOWN) {
            std::string *target = nullptr;
            if (state.inputMode == SettingsInputMode::JellyfinUrl) target = &state.jellyfinUrl;
            else if (state.inputMode == SettingsInputMode::JellyfinUser) target = &state.jellyfinUser;
            else if (state.inputMode == SettingsInputMode::JellyfinPass) target = &state.jellyfinPass;

            if (target) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && !target->empty()) {
                    target->pop_back();
                } else if (e.key.keysym.sym == SDLK_RETURN) {
                    // Move to next field
                    if (state.inputMode == SettingsInputMode::JellyfinUrl)
                        state.inputMode = SettingsInputMode::JellyfinUser;
                    else if (state.inputMode == SettingsInputMode::JellyfinUser)
                        state.inputMode = SettingsInputMode::JellyfinPass;
                    else
                        state.inputMode = SettingsInputMode::None; // finished
                }
            }
        }


        int winWidth, winHeight;
        SDL_GetWindowSize(window, &winWidth, &winHeight);

        SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
        SDL_RenderClear(renderer);

        // ------------------ DRAW CURRENT SCREEN ------------------
        switch (state.current) {
            case Screen::MainMenu:
                drawMenu(renderer, font, state, mainMenu, winWidth, winHeight);
                break;
            case Screen::Music:
                if (currentSong)
                    drawMusicScreen(renderer, font, currentSong, winWidth, winHeight);
                else
                    drawSongsMenu(renderer, font, state, songs, winWidth, winHeight);
                break;
            case Screen::Video:
                drawTopBar(renderer, font, "Videos", winWidth);
            case Screen::Settings:
                drawSettingsPage(renderer, font, state, winWidth, winHeight);
                break;
            case Screen::About:
                drawAboutPage(renderer, font, winWidth);
                break;
        }

        SDL_RenderPresent(renderer);
    }

    // ------------------ CLEANUP ------------------
    for (auto &s: songs) if (s.artwork) SDL_DestroyTexture(s.artwork.get());
    if (currentMusic) Mix_FreeMusic(currentMusic);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_StopTextInput();


    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
