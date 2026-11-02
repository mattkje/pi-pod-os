#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <future>
#include <string>
#include <vector>
#include "app/AppState.h"
#include "media/Song.h"
#include "media/VideoPlayer.h"

// Playback queue on top of SDL_mixer. Formats SDL_mixer can't decode (M4A/AAC/ALAC, AIFF, WMA, ...)
// are played through FFmpeg instead. Remote (Jellyfin) songs are downloaded on a background
// thread before they start; call update() once per frame.
class Player {
public:
    Player();
    ~Player();

    void setRemote(const std::string &serverUrl, const std::string &token, const std::string &cacheDir);

    // Replaces the queue and starts playing `start`. With shuffle, `start` plays first and
    // the rest of the queue is randomised.
    void playQueue(std::vector<Song> songs, int start, bool shuffle);

    void togglePause();
    void next();
    void previous(); // restarts the song if more than 3s in, otherwise goes back
    void stop();

    void setVolume(int percent);
    void setRepeat(RepeatMode mode) { repeat = mode; }

    void update(SDL_Renderer *renderer);

    const Song *current() const;
    int queueIndex() const { return index; }
    int queueSize() const { return static_cast<int>(queue.size()); }
    bool isPaused() const;
    bool isActive() const { return music != nullptr || stream.isOpen(); }
    bool isLoading() const { return loading; }
    const std::string &error() const { return lastError; }

    double position() const; // seconds, -1 if unknown
    double duration() const; // seconds, -1 if unknown

private:
    void startCurrent();
    void advance(bool userInitiated);
    void freeMusic();

    std::vector<Song> queue;
    int index = -1;
    Mix_Music *music = nullptr;
    VideoPlayer stream; // FFmpeg playback when SDL_mixer can't load the file
    int volume = 100;
    RepeatMode repeat = RepeatMode::Off;
    bool loading = false;
    std::string lastError;

    std::string serverUrl, token, cacheDir;

    struct PendingDownload {
        std::string remoteId;
        std::future<std::pair<Song, std::string>> result;
    };
    std::vector<PendingDownload> downloads;
};
