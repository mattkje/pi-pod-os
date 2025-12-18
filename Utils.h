#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include "Song.h" // provide Song definition

// Call this to fetch songs from Jellyfin. Returns Song objects with filePath set to a stream/download URL.
std::vector<Song> loadJellyfinSongs(SDL_Renderer *renderer,
                                    const std::string &serverUrl,
                                    const std::string &apiKey,
                                    const std::string &userId = "",
                                    const std::string &libraryId = "");

SDL_Texture *renderText(SDL_Renderer *renderer, TTF_Font *font, const std::string &text, SDL_Color color);

void drawTopBar(SDL_Renderer *r, TTF_Font *font, const std::string &title, int winWidth, int batteryPercent = 100);

void drawHighlight(SDL_Renderer *renderer, const SDL_Rect &rect);
