#pragma once
#include <string>

// Small libcurl helpers for podcasts (feeds, artwork, episodes).
namespace Net {
    // GET into a string. Returns an empty error string on success.
    std::string get(const std::string &url, std::string &body);

    // Download to a file (written as <path>.part, then renamed). Returns an empty string on success.
    std::string download(const std::string &url, const std::string &path);

    // Stable short hex name for a URL or path, used for cache file names.
    std::string hashName(const std::string &s);
}
