#pragma once
#include <string>
#include <vector>
#include "Song.h"

namespace Jellyfin {
    std::vector<Song> fetchSongs(const std::string &serverUrl,
                                 const std::string &apiKey,
                                 const std::string &userId = "",
                                 const std::string &libraryId = "");
}