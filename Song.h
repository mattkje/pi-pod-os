// cpp
// File: Song.h
#pragma once
#include <string>
#include <memory>
#include <SDL2/SDL.h>

// Lightweight song model used throughout the app.
struct Song {
    std::string title;
    std::string artist;
    std::string filePath;      // local path or remote URL
    std::shared_ptr<SDL_Texture> artwork; // optional; nullptr if not loaded

    Song() = default;
};