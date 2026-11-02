#pragma once
#include <string>
#include <memory>
#include <SDL2/SDL.h>

// Lightweight song model used throughout the app.
struct Song {
    std::string title;
    std::string artist;
    std::string album;
    std::string filePath;  // local path, or empty for remote songs until downloaded
    std::string remoteId;  // Jellyfin item id; empty for local songs
    std::string remoteAlbumId; // Jellyfin album id, used as cover fallback
    std::string downloadUrl;   // plain HTTP download (podcast episode); fetched to downloadPath on play
    std::string downloadPath;
    std::string artworkPath; // local image path (sidecar file or downloaded cover)
    std::shared_ptr<SDL_Texture> artwork; // lazily loaded; nullptr if not loaded

    bool isRemote() const { return !remoteId.empty(); }
    bool needsDownload() const { return filePath.empty() && (isRemote() || !downloadUrl.empty()); }
};
