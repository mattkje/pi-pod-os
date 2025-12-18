#include "JellyfinClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

static size_t write_cb(void* data, size_t size, size_t nmemb, void* userp) {
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

std::vector<Song> Jellyfin::fetchSongs(const std::string& serverUrl,
                                       const std::string& apiKey,
                                       const std::string& userId,
                                       const std::string& libraryId) {
    std::vector<Song> result;
    CURL* curl = curl_easy_init();
    if (!curl) return result;

    std::ostringstream url;
    // Build Items query (Audio items, recursive). Use ParentId if provided.
    url << serverUrl;
    if (url.str().back() == '/') url.seekp(-1, std::ios_base::end);
    url << "/Users";
    if (!userId.empty()) url << "/" << userId;
    url << "/Items?Recursive=true&IncludeItemTypes=Audio&Fields=Album,Artists,ProviderIds";
    if (!libraryId.empty()) url << "&ParentId=" << libraryId;
    if (!apiKey.empty()) url << "&api_key=" << apiKey;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    // optional: skip TLS verification for local servers (not recommended for production)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        curl_easy_cleanup(curl);
        return result;
    }

    try {
        auto parsed = json::parse(response);
        if (!parsed.contains("Items")) {
            curl_easy_cleanup(curl);
            return result;
        }
        for (auto &it : parsed["Items"]) {
            Song s;
            s.title = it.value("Name", std::string("Unknown Title"));
            if (it.contains("Artists") && it["Artists"].is_array() && !it["Artists"].empty())
                s.artist = it["Artists"][0].get<std::string>();
            else
                s.artist = "";

            std::string id = it.value("Id", std::string());
            if (id.empty()) continue;

            // Build a download/stream URL. Many Jellyfin endpoints accept /Items/{id}/Download
            std::ostringstream fileUrl;
            fileUrl << serverUrl;
            if (serverUrl.back() == '/') fileUrl.seekp(-1, std::ios_base::end);
            fileUrl << "/Items/" << id << "/Download";
            if (!apiKey.empty())
                fileUrl << "?api_key=" << apiKey;

            s.filePath = fileUrl.str();
            s.artwork = nullptr; // let the app load artwork later (or implement image fetch)
            result.push_back(std::move(s));
        }
    } catch (...) {
        // parse error -> return what we have
    }

    curl_easy_cleanup(curl);
    return result;
}