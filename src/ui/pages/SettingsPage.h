#pragma once
#include <string>
#include <vector>
#include "app/AppState.h"
#include "ui/Utils.h"
#include "system/DeviceInfo.h"

// Row order of the settings list. Keep in sync with settingsRows().
enum class SettingsItem {
    Volume,
    Shuffle,
    Repeat,
    Language,
    FlipScreen,
    Jellyfin,
    RescanLibrary,
    AddMedia,
    SetupAssistant,
    Reset,
    Update,   // device mode only
    Restart,  // device mode only
    ShutDown, // device mode only
    Count
};

// Number of rows shown; the power options only exist in device mode.
int settingsItemCount(const AppState &state);

std::vector<ListRow> settingsRows(const AppState &state);

void drawSettingsPage(const UI &ui, AppState &state, const DeviceInfo &info);
