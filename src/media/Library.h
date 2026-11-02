#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include "media/Song.h"

namespace Library {
    // Scans the given directories (recursively) for audio files and reads their tags.
    // Missing directories are skipped.
    std::vector<Song> scanLocal(const std::vector<std::string> &dirs);

    // Default music directories: <assets>/music and ~/Music.
    std::vector<std::string> defaultMusicDirs();

    // Loads song.artworkPath into song.artwork if not already loaded.
    void ensureArtwork(SDL_Renderer *renderer, Song &song);
}
