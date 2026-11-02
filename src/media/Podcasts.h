#pragma once
#include <future>
#include <string>
#include <vector>
#include "media/Song.h"

// Podcast subscriptions: RSS feeds are fetched in the background, parsed, and cached on disk so
// shows and episodes are still listed when offline. Episodes are played through the normal
// Player as songs with a download URL.
namespace Podcasts {
    struct Episode {
        std::string title;
        std::string url;       // enclosure (audio file)
        std::string date;      // "12 Mar 2026"
        std::string duration;  // "45:12" or "" if unknown
    };

    struct Show {
        std::string feedUrl;
        std::string title;
        std::string author;
        std::string artworkPath; // cached cover, "" if none
        std::vector<Episode> episodes; // newest first
        std::string error;       // set when the feed couldn't be loaded and no cache exists
    };

    // Loads every feed (network first, falling back to the cached copy). Blocking; run it on a
    // background thread. Cover images are downloaded into the cache as well.
    std::vector<Show> load(const std::vector<std::string> &feedUrls, const std::string &cacheDir);

    // Parses RSS XML (exposed for tests). Returns false if the document isn't a podcast feed.
    bool parse(const std::string &xml, Show &show, std::string *imageUrl = nullptr);

    // A playable Song for an episode (downloads to the cache when played).
    Song toSong(const Show &show, const Episode &episode, const std::string &cacheDir);
}
