#include "media/Player.h"
#include "net/JellyfinClient.h"
#include "net/Net.h"
#include "media/Library.h"
#include <algorithm>
#include <atomic>
#include <random>

namespace {
    // Set from the SDL_mixer audio thread when a song ends.
    std::atomic<bool> musicFinished{false};

    void onMusicFinished() { musicFinished = true; }
}

Player::Player() {
    Mix_HookMusicFinished(onMusicFinished);
}

Player::~Player() {
    Mix_HookMusicFinished(nullptr);
    freeMusic();
    for (auto &d: downloads) d.result.wait();
}

void Player::setRemote(const std::string &url, const std::string &tok, const std::string &cache) {
    serverUrl = url;
    token = tok;
    cacheDir = cache;
}

void Player::playQueue(std::vector<Song> songs, int start, bool shuffle) {
    if (songs.empty()) return;
    start = std::clamp(start, 0, static_cast<int>(songs.size()) - 1);

    if (shuffle) {
        std::swap(songs[0], songs[start]);
        std::shuffle(songs.begin() + 1, songs.end(), std::mt19937{std::random_device{}()});
        start = 0;
    }
    queue = std::move(songs);
    index = start;
    startCurrent();
}

void Player::freeMusic() {
    if (music) {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = nullptr;
    }
    stream.close();
    // Mix_HaltMusic fires the finished hook synchronously; that's not a real "song ended".
    musicFinished = false;
}

void Player::startCurrent() {
    freeMusic();
    lastError.clear();
    loading = false;
    if (index < 0 || index >= static_cast<int>(queue.size())) return;

    Song &song = queue[index];
    if (song.needsDownload()) {
        loading = true;
        std::string key = song.isRemote() ? song.remoteId : song.downloadUrl;
        bool alreadyPending = std::any_of(downloads.begin(), downloads.end(),
                                          [&](const PendingDownload &d) { return d.remoteId == key; });
        if (!alreadyPending) {
            Song copy = song;
            std::string url = serverUrl, tok = token, cache = cacheDir;
            downloads.push_back({
                key,
                std::async(std::launch::async, [copy, url, tok, cache]() mutable {
                    std::string err;
                    if (copy.isRemote()) {
                        Jellyfin::downloadSong(copy, url, tok, cache, &err);
                    } else {
                        err = Net::download(copy.downloadUrl, copy.downloadPath);
                        if (err.empty()) copy.filePath = copy.downloadPath;
                    }
                    return std::make_pair(copy, err);
                })
            });
        }
        return;
    }

    music = Mix_LoadMUS(song.filePath.c_str());
    if (!music) {
        // Not a format SDL_mixer knows: let FFmpeg decode it. Errors show up in update().
        stream.setVolume(volume);
        stream.open(song.filePath, nullptr, "", true);
        return;
    }
    Mix_PlayMusic(music, 1);
}

void Player::update(SDL_Renderer *renderer) {
    // Collect finished downloads.
    for (auto it = downloads.begin(); it != downloads.end();) {
        if (it->result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++it;
            continue;
        }
        std::pair<Song, std::string> done = it->result.get();
        Song &song = done.first;
        std::string &err = done.second;
        it = downloads.erase(it);

        auto same = [&](const Song &q) {
            return song.isRemote() ? q.remoteId == song.remoteId : q.downloadUrl == song.downloadUrl;
        };
        for (Song &q: queue) {
            if (same(q)) {
                q.filePath = song.filePath;
                q.artworkPath = song.artworkPath;
            }
        }
        const Song *cur = current();
        if (loading && cur && same(*cur)) {
            if (song.filePath.empty()) {
                loading = false;
                lastError = err.empty() ? "Download failed" : err;
            } else {
                startCurrent();
            }
        }
    }

    if (musicFinished.exchange(false) && music) advance(false);

    if (stream.isOpen()) {
        stream.update();
        if (stream.finished()) {
            if (stream.error().empty()) {
                advance(false);
            } else {
                lastError = stream.error();
                SDL_Log("Failed to play %s: %s", current() ? current()->filePath.c_str() : "", lastError.c_str());
                stream.close();
            }
        }
    }

    if (index >= 0 && index < static_cast<int>(queue.size()))
        Library::ensureArtwork(renderer, queue[index]);
}

void Player::advance(bool userInitiated) {
    if (queue.empty()) return;
    if (!userInitiated && repeat == RepeatMode::One) {
        startCurrent();
        return;
    }
    if (index + 1 < static_cast<int>(queue.size())) {
        ++index;
    } else if (repeat == RepeatMode::All || userInitiated) {
        index = 0;
    } else {
        freeMusic(); // end of queue
        return;
    }
    startCurrent();
}

void Player::next() {
    advance(true);
}

void Player::previous() {
    if (queue.empty()) return;
    if (position() > 3.0) {
        startCurrent();
        return;
    }
    index = (index - 1 + static_cast<int>(queue.size())) % static_cast<int>(queue.size());
    startCurrent();
}

void Player::togglePause() {
    if (stream.isOpen()) {
        stream.togglePause();
        return;
    }
    if (!music) {
        if (current()) startCurrent(); // restart a stopped queue
        return;
    }
    if (Mix_PausedMusic()) Mix_ResumeMusic();
    else Mix_PauseMusic();
}

void Player::stop() {
    freeMusic();
    queue.clear();
    index = -1;
    loading = false;
}

void Player::setVolume(int percent) {
    volume = std::clamp(percent, 0, 100);
    Mix_VolumeMusic(volume * MIX_MAX_VOLUME / 100);
    stream.setVolume(volume);
}

const Song *Player::current() const {
    if (index < 0 || index >= static_cast<int>(queue.size())) return nullptr;
    return &queue[index];
}

bool Player::isPaused() const {
    if (stream.isOpen()) return stream.paused();
    return music && Mix_PausedMusic();
}

double Player::position() const {
    if (stream.isOpen()) return stream.position();
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
    if (music) return Mix_GetMusicPosition(music);
#endif
    return -1;
}

double Player::duration() const {
    if (stream.isOpen()) return stream.duration() > 0 ? stream.duration() : -1;
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
    if (music) return Mix_MusicDuration(music);
#endif
    return -1;
}
