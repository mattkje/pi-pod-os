#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

SDL_Texture *renderText(SDL_Renderer *renderer, TTF_Font *font, const std::string &text, SDL_Color color);
void drawTopBar(SDL_Renderer *renderer, TTF_Font *font, const std::string &title, int winWidth);
void drawHighlight(SDL_Renderer *renderer, const SDL_Rect &rect);
