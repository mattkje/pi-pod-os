#pragma once
#include <SDL2/SDL.h>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct MediaFile {
    std::string path;
    std::string name;  // file name without extension, for display
    unsigned long long size = 0;
    long long mtime = 0;
};

// An entry in the Videos list: a local file or a Jellyfin video (streamed).
struct VideoItem {
    std::string title;
    std::string detail;
    std::string path;       // local file; empty for Jellyfin
    std::string jellyfinId; // empty for local files
};

namespace Media {
    std::string videosDir(); // ~/Videos
    std::string photosDir(); // ~/Pictures

    std::vector<MediaFile> scanVideos(); // sorted by name
    std::vector<MediaFile> scanPhotos(); // newest first

    bool isVideo(const std::string &name);
    bool isPhoto(const std::string &name);
}

// Loads photos on a background thread: square thumbnails (cached as PNG on disk) and screen-sized
// full images. get() returns a texture once it's ready, otherwise nullptr and queues the work.
class PhotoLoader {
public:
    PhotoLoader(SDL_Renderer *renderer, const std::string &cacheDir);
    ~PhotoLoader();

    SDL_Texture *thumbnail(const MediaFile &photo); // 160x160, centre-cropped
    SDL_Texture *full(const MediaFile &photo);      // fits 640x480

    // Moves finished images to textures; call once per frame on the main thread.
    void update();
    void clear(); // drop all textures (e.g. after the photo list changed)

private:
    struct Job {
        std::string key;
        std::string path;
        bool thumb;
        long long mtime;
    };
    struct Result {
        std::string key;
        SDL_Surface *surface;
    };

    SDL_Texture *request(const MediaFile &photo, bool thumb);
    void work();
    SDL_Surface *loadScaled(const Job &job);

    SDL_Renderer *renderer_;
    std::string cacheDir_;
    std::map<std::string, SDL_Texture *> textures_;
    std::map<std::string, Uint32> lastUsed_;
    std::map<std::string, bool> pending_;

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::vector<Result> results_;
    bool quit_ = false;
};
