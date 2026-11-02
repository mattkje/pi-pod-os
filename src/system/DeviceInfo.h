#pragma once
#include <string>

// Facts about the device shown in Settings (status pane) and About.
struct DeviceInfo {
    int localSongs = 0;
    int jellyfinSongs = 0;
    std::string cacheSize;
    std::string ipAddress;
    std::string webUrl; // where the music upload page is reachable (set by the app)

    // Storage of the root file system
    unsigned long long diskTotal = 0;
    unsigned long long diskFree = 0;
    unsigned long long musicBytes = 0; // local music folders
    unsigned long long cacheBytes = 0; // downloaded Jellyfin songs and covers

    // Static facts
    std::string model;   // "Raspberry Pi 3 Model B Plus Rev 1.3"
    std::string serial;
    std::string system;  // "Debian GNU/Linux 13 (trixie)"
};

// Refreshes everything; cheap enough to call every few seconds while a page shows it.
void refreshDeviceInfo(DeviceInfo &info, int localSongs, int jellyfinSongs);
