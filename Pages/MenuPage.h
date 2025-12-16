#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../AppState.h"
#include <vector>

void drawMenu(SDL_Renderer *renderer, TTF_Font *font, AppState &state,
              const std::vector<MenuItem> &items, int winWidth, int winHeight);
