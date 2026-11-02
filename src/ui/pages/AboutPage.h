#pragma once
#include <string>
#include "app/AppState.h"
#include "system/DeviceInfo.h"
#include "ui/Utils.h"

// iPod classic style About: name, storage bar with legend, then a scrollable list of facts.
// Up/Down scroll the list (state.selected is the first visible row).
void drawAboutPage(const UI &ui, AppState &state, const DeviceInfo &info, const std::string &deviceName);

// Number of scroll positions for the list (used for Up/Down limits).
int aboutRowCount(const AppState &state);
