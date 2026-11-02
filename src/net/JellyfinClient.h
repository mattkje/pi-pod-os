#pragma once
#include <string>
#include <vector>
#include "media/Song.h"

namespace Jellyfin {
    struct AuthResult {
        bool ok = false;
        std::string error;
        std::string userId;
        std::string token;
    };

    // Logs in with username/password (POST /Users/AuthenticateByName).
    AuthResult authenticate(const std::string &serverUrl,
                            const std::string &username,
                            const std::string &password);

    // Lists every audio item in the user's libraries. Songs come back with `remoteId` set and
    // no `filePath`; call downloadSong() before playback.
    std::vector<Song> fetchSongs(const std::string &serverUrl,
                                 const std::string &token,
                                 const std::string &userId,
                                 std::string *error = nullptr);

    struct Video {
        std::string id;
        std::string title;
        std::string detail; // "Pioneer One · S1E2" or "2009", plus running time
    };

    // Movies, episodes and other videos in the user's libraries.
    std::vector<Video> fetchVideos(const std::string &serverUrl, const std::string &token,
                                   const std::string &userId, std::string *error = nullptr);

    // HLS stream transcoded to H.264/AAC at most 640x480, which the Pi decodes in hardware.
    // The token is part of the URL because FFmpeg doesn't send custom headers for HLS segments;
    // Jellyfin repeats the query in every segment URL.
    std::string videoStreamUrl(const std::string &serverUrl, const std::string &token, const std::string &id,
                               const std::string &playSessionId);

    // HTTP header line (without trailing CRLF) that authenticates requests with the token.
    std::string authorizationHeader(const std::string &token);

    // Tells the server to stop the transcode for a play session (fire and forget).
    void stopTranscode(const std::string &serverUrl, const std::string &token, const std::string &playSessionId);

    // Downloads the audio (transcoded to mp3) and the primary cover image into cacheDir.
    // Sets song.filePath (and song.artworkPath if a cover exists). Returns false on failure.
    bool downloadSong(Song &song,
                      const std::string &serverUrl,
                      const std::string &token,
                      const std::string &cacheDir,
                      std::string *error = nullptr);
}
