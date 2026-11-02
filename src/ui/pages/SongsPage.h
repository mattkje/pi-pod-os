#pragma once
#include <vector>
#include "app/AppState.h"
#include "media/Song.h"
#include "ui/Utils.h"

// Song list. `loading` shows a hint while the Jellyfin library is being fetched.
void drawSongsPage(const UI &ui, AppState &state, const std::vector<Song> &songs, bool loading);
