#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include "../AppState.h"
#include "../Song.h"

std::vector<Song> loadSongs(SDL_Renderer *renderer);

void drawSongsMenu(SDL_Renderer *renderer, TTF_Font *font, AppState &state, const std::vector<Song> &songs,
                   int winWidth, int winHeight);

void drawMusicScreen(SDL_Renderer *renderer, TTF_Font *font, const Song *currentSong, int winWidth, int winHeight);
