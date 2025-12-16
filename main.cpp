#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <string>
#include <cmath>

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 320;
constexpr int ITEM_HEIGHT = 36;

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
const SDL_Color BACKGROUND_COLOR = {230, 230, 230, 255}; // light gray
const SDL_Color HIGHLIGHT_COLOR = {0, 122, 255, 255}; // iPod blue
const SDL_Color TEXT_COLOR = {50, 50, 50, 255}; // dark gray
const SDL_Color SELECTED_TEXT_COLOR = {255, 255, 255, 255}; // white
const SDL_Color SHADOW_COLOR = {0, 0, 0, 64}; // subtle shadow

SDL_Texture *renderText(SDL_Renderer *r, TTF_Font *font, const std::string &text, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void drawMenu(SDL_Renderer *r, TTF_Font *font, AppState &state, const std::vector<MenuItem> &items) {
    int centerY = SCREEN_HEIGHT / 2;

    // Smooth scrolling
    state.visualOffset += (state.selected - state.visualOffset) * 0.15f;

    for (int i = 0; i < (int) items.size(); i++) {
        float y = centerY + (i - state.visualOffset) * ITEM_HEIGHT;

        if (y < -ITEM_HEIGHT || y > SCREEN_HEIGHT + ITEM_HEIGHT)
            continue;

        bool selected = (i == state.selected);

        SDL_Color color = selected ? SELECTED_TEXT_COLOR : TEXT_COLOR;

        SDL_Texture *text = renderText(r, font, items[i].label, color);

        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);

        SDL_Rect dst{
            (SCREEN_WIDTH - w) / 2,
            (int) y,
            w,
            h
        };

        // Draw shadow for selected text
        if (selected) {
            SDL_Texture *shadowText = renderText(r, font, items[i].label, SHADOW_COLOR);
            SDL_Rect shadowDst = dst;
            shadowDst.x += 1;
            shadowDst.y += 1;
            SDL_RenderCopy(r, shadowText, nullptr, &shadowDst);
            SDL_DestroyTexture(shadowText);
        }

        // Draw highlight behind selected item
        if (selected) {
            SDL_Rect highlight{
                (SCREEN_WIDTH - 320) / 2,
                (int) y - 4,
                320,
                ITEM_HEIGHT
            };
            SDL_SetRenderDrawColor(r, HIGHLIGHT_COLOR.r, HIGHLIGHT_COLOR.g, HIGHLIGHT_COLOR.b, HIGHLIGHT_COLOR.a);
            SDL_RenderFillRect(r, &highlight);
        }

        SDL_RenderCopy(r, text, nullptr, &dst);
        SDL_DestroyTexture(text);
    }
}

void drawMusicScreen(SDL_Renderer *r, TTF_Font *font) {
    // Track title
    std::string track = "My Favorite Song";
    SDL_Texture *trackText = renderText(r, font, track, TEXT_COLOR);
    int tw, th;
    SDL_QueryTexture(trackText, nullptr, nullptr, &tw, &th);
    SDL_Rect trackDst{(SCREEN_WIDTH - tw) / 2, 50, tw, th};
    SDL_RenderCopy(r, trackText, nullptr, &trackDst);
    SDL_DestroyTexture(trackText);

    // Media controls symbols
    std::string prevSymbol = "⏮"; // previous
    std::string playSymbol = "⏯"; // play/pause
    std::string nextSymbol = "⏭"; // next

    SDL_Texture *prevText = renderText(r, font, prevSymbol, TEXT_COLOR);
    SDL_Texture *playText = renderText(r, font, playSymbol, TEXT_COLOR);
    SDL_Texture *nextText = renderText(r, font, nextSymbol, TEXT_COLOR);

    int pw, ph, plw, plh, nw, nh;
    SDL_QueryTexture(prevText, nullptr, nullptr, &pw, &ph);
    SDL_QueryTexture(playText, nullptr, nullptr, &plw, &plh);
    SDL_QueryTexture(nextText, nullptr, nullptr, &nw, &nh);

    int y = SCREEN_HEIGHT - 100;
    int spacing = 50;
    int totalWidth = pw + plw + nw + spacing * 2;

    int xStart = (SCREEN_WIDTH - totalWidth) / 2;

    SDL_Rect prevDst{xStart, y, pw, ph};
    SDL_Rect playDst{xStart + pw + spacing, y, plw, plh};
    SDL_Rect nextDst{xStart + pw + spacing + plw + spacing, y, nw, nh};

    SDL_RenderCopy(r, prevText, nullptr, &prevDst);
    SDL_RenderCopy(r, playText, nullptr, &playDst);
    SDL_RenderCopy(r, nextText, nullptr, &nextDst);

    SDL_DestroyTexture(prevText);
    SDL_DestroyTexture(playText);
    SDL_DestroyTexture(nextText);
}

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();

    SDL_Window *window = SDL_CreateWindow(
        "iPodOS",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_FULLSCREEN
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    TTF_Font *font = TTF_OpenFont("/Users/matti.kjellstadli/CLionProjects/untitled/MyriadPro-Regular.otf", 20);


    AppState state;

    std::vector<MenuItem> mainMenu{
        {"Music", Screen::Music},
        {"Settings", Screen::Settings},
        {"About", Screen::About}
    };

    bool running = true;
    Uint32 last = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;

                    case SDLK_UP:
                        state.selected = (state.selected - 1 + mainMenu.size()) % mainMenu.size();
                        break;

                    case SDLK_DOWN:
                        state.selected = (state.selected + 1) % mainMenu.size();
                        break;

                    case SDLK_RETURN:
                        state.current = mainMenu[state.selected].next;
                        break;

                    case SDLK_BACKSPACE:
                        state.current = Screen::MainMenu;
                        break;
                }
            }
        }

        // Clear screen with iPod background
        SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b,
                               BACKGROUND_COLOR.a);
        SDL_RenderClear(renderer);

        switch (state.current) {
            case Screen::MainMenu:
                drawMenu(renderer, font, state, mainMenu);
                break;

            case Screen::Music:
                drawMusicScreen(renderer, font);
                break;

            case Screen::Settings:
            case Screen::About: {
                SDL_Color c = TEXT_COLOR;
                std::string label = (state.current == Screen::Settings) ? "Settings" : "About";
                SDL_Texture *t = renderText(renderer, font, label, c);
                int w, h;
                SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
                SDL_Rect dst{(SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2, w, h};
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
                break;
            }
        }

        SDL_RenderPresent(renderer);

        Uint32 now = SDL_GetTicks();
        Uint32 frameTime = now - last;
        if (frameTime < 16)
            SDL_Delay(16 - frameTime);
        last = now;
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
