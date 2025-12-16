#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../AppState.h"

void drawSettingsPage(SDL_Renderer *r, TTF_Font *font, AppState &state, int winWidth, int winHeight);
