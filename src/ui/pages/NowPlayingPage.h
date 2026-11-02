#pragma once
#include "app/AppState.h"
#include "ui/Icons.h"
#include "media/Player.h"
#include "ui/Utils.h"

// iPod classic (6th/7th gen) style Now Playing: cover with reflection on the left, song info on
// the right, shuffle/repeat status in the corner and a full-width progress bar at the bottom.
void drawNowPlaying(const UI &ui, const Player &player, const Settings &settings, MenuIcons &icons);
