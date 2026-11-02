#include "net/JellyfinClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <thread>

#ifndef PIPOD_VERSION
#define PIPOD_VERSION "dev"
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
    // Newer Jellyfin servers reject the legacy X-Emby-Token header and api_key query parameter,
    // so the access token always goes in the Authorization header.
    std::string authHeader(const std::string &token) {
        std::string h = "Authorization: MediaBrowser Client=\"PiPod OS\", Device=\"PiPod\", "
                        "DeviceId=\"pi-pod-os\", Version=\"" PIPOD_VERSION "\"";
        if (!token.empty()) h += ", Token=\"" + token + "\"";
        return h;
    }

    size_t writeToString(void *data, size_t size, size_t nmemb, void *userp) {
        static_cast<std::string *>(userp)->append(static_cast<char *>(data), size * nmemb);
        return size * nmemb;
    }

    size_t writeToFile(void *data, size_t size, size_t nmemb, void *userp) {
        static_cast<std::ofstream *>(userp)->write(static_cast<char *>(data),
                                                   static_cast<std::streamsize>(size * nmemb));
        return size * nmemb;
    }

    std::string trimSlash(std::string url) {
        while (!url.empty() && url.back() == '/') url.pop_back();
        return url;
    }

    struct Response {
        bool ok = false;
        long status = 0;
        std::string body;
        std::string error;
    };

    Response request(const std::string &url, const std::string &token, const std::string *postBody) {
        Response res;
        CURL *curl = curl_easy_init();
        if (!curl) {
            res.error = "curl init failed";
            return res;
        }

        curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, authHeader(token).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (postBody) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody->c_str());

        CURLcode rc = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.status);
        if (rc != CURLE_OK) {
            res.error = curl_easy_strerror(rc);
        } else if (res.status >= 400) {
            res.error = "HTTP " + std::to_string(res.status);
        } else {
            res.ok = true;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return res;
    }

    bool downloadTo(const std::string &url, const std::string &token, const fs::path &dest, std::string *error) {
        fs::path tmp = dest;
        tmp += ".part";
        std::ofstream out(tmp, std::ios::binary);
        if (!out) {
            if (error) *error = "Cannot write cache";
            return false;
        }

        CURL *curl = curl_easy_init();
        if (!curl) return false;
        curl_slist *headers = curl_slist_append(nullptr, authHeader(token).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

        CURLcode rc = curl_easy_perform(curl);
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        out.close();

        std::error_code ec;
        if (rc != CURLE_OK || status >= 400) {
            fs::remove(tmp, ec);
            if (error) *error = rc != CURLE_OK ? curl_easy_strerror(rc) : "HTTP " + std::to_string(status);
            return false;
        }
        fs::rename(tmp, dest, ec);
        return !ec;
    }
}

Jellyfin::AuthResult Jellyfin::authenticate(const std::string &serverUrl,
                                            const std::string &username,
                                            const std::string &password) {
    AuthResult result;
    json body = {{"Username", username}, {"Pw", password}};
    std::string payload = body.dump();
    Response res = request(trimSlash(serverUrl) + "/Users/AuthenticateByName", "", &payload);
    if (!res.ok) {
        result.error = res.status == 401 ? "Wrong username or password" : res.error;
        return result;
    }

    try {
        json j = json::parse(res.body);
        result.token = j.value("AccessToken", std::string());
        if (j.contains("User")) result.userId = j["User"].value("Id", std::string());
        result.ok = !result.token.empty() && !result.userId.empty();
        if (!result.ok) result.error = "Unexpected server response";
    } catch (const std::exception &) {
        result.error = "Invalid server response";
    }
    return result;
}

std::vector<Song> Jellyfin::fetchSongs(const std::string &serverUrl,
                                       const std::string &token,
                                       const std::string &userId,
                                       std::string *error) {
    std::vector<Song> result;
    std::string url = trimSlash(serverUrl) + "/Users/" + userId +
                      "/Items?Recursive=true&IncludeItemTypes=Audio&SortBy=SortName"
                      "&Fields=Album,Artists,AlbumArtist,AlbumId";
    Response res = request(url, token, nullptr);
    if (!res.ok) {
        if (error) *error = res.error;
        return result;
    }

    try {
        json parsed = json::parse(res.body);
        for (auto &it: parsed.value("Items", json::array())) {
            Song s;
            s.remoteId = it.value("Id", std::string());
            if (s.remoteId.empty()) continue;
            s.title = it.value("Name", std::string("Unknown Title"));
            s.album = it.value("Album", std::string());
            if (it.contains("AlbumId") && it["AlbumId"].is_string()) s.remoteAlbumId = it["AlbumId"].get<std::string>();
            if (it.contains("Artists") && it["Artists"].is_array() && !it["Artists"].empty())
                s.artist = it["Artists"][0].get<std::string>();
            else
                s.artist = it.value("AlbumArtist", std::string("Unknown Artist"));
            result.push_back(std::move(s));
        }
    } catch (const std::exception &e) {
        if (error) *error = "Invalid server response";
    }
    return result;
}

std::vector<Jellyfin::Video> Jellyfin::fetchVideos(const std::string &serverUrl, const std::string &token,
                                                   const std::string &userId, std::string *error) {
    std::vector<Video> result;
    std::string url = trimSlash(serverUrl) + "/Users/" + userId +
                      "/Items?Recursive=true&IncludeItemTypes=Movie,Episode,Video,MusicVideo&SortBy=SortName"
                      "&Fields=RunTimeTicks,SeriesName,ProductionYear,ParentIndexNumber,IndexNumber";
    Response res = request(url, token, nullptr);
    if (!res.ok) {
        if (error) *error = res.error;
        return result;
    }
    try {
        json parsed = json::parse(res.body);
        for (auto &it: parsed.value("Items", json::array())) {
            Video v;
            v.id = it.value("Id", std::string());
            if (v.id.empty()) continue;
            v.title = it.value("Name", std::string("Video"));
            std::string detail;
            if (it.contains("SeriesName") && it["SeriesName"].is_string()) {
                detail = it["SeriesName"].get<std::string>();
                if (it.contains("ParentIndexNumber") && it.contains("IndexNumber") &&
                    it["ParentIndexNumber"].is_number() && it["IndexNumber"].is_number())
                    detail += " \xC2\xB7 S" + std::to_string(it["ParentIndexNumber"].get<int>()) + "E" +
                              std::to_string(it["IndexNumber"].get<int>());
            } else if (it.contains("ProductionYear") && it["ProductionYear"].is_number()) {
                detail = std::to_string(it["ProductionYear"].get<int>());
            }
            if (it.contains("RunTimeTicks") && it["RunTimeTicks"].is_number()) {
                long long secs = it["RunTimeTicks"].get<long long>() / 10000000;
                char buf[32];
                if (secs >= 3600) std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", secs / 3600, (secs / 60) % 60, secs % 60);
                else std::snprintf(buf, sizeof(buf), "%lld:%02lld", secs / 60, secs % 60);
                detail += (detail.empty() ? "" : "  \xC2\xB7  ") + std::string(buf);
            }
            v.detail = detail;
            result.push_back(std::move(v));
        }
    } catch (const std::exception &) {
        if (error) *error = "Invalid server response";
    }
    return result;
}

std::string Jellyfin::videoStreamUrl(const std::string &serverUrl, const std::string &token, const std::string &id,
                                     const std::string &session) {
    return trimSlash(serverUrl) + "/Videos/" + id +
           "/main.m3u8?VideoCodec=h264&AudioCodec=aac&MaxWidth=640&MaxHeight=480&VideoBitRate=1500000"
           "&AudioBitRate=128000&AudioChannels=2&SegmentContainer=ts&MediaSourceId=" + id +
           "&DeviceId=pi-pod-os&PlaySessionId=" + session + "&api_key=" + token;
}

std::string Jellyfin::authorizationHeader(const std::string &token) {
    return authHeader(token);
}

void Jellyfin::stopTranscode(const std::string &serverUrl, const std::string &token, const std::string &session) {
    std::string url = trimSlash(serverUrl) + "/Videos/ActiveEncodings?DeviceId=pi-pod-os&PlaySessionId=" + session;
    std::thread([url, token]() {
        CURL *curl = curl_easy_init();
        if (!curl) return;
        curl_slist *headers = curl_slist_append(nullptr, authHeader(token).c_str());
        std::string sink;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }).detach();
}

bool Jellyfin::downloadSong(Song &song,
                            const std::string &serverUrl,
                            const std::string &token,
                            const std::string &cacheDir,
                            std::string *error) {
    std::error_code ec;
    fs::path dir = fs::path(cacheDir) / "jellyfin";
    fs::create_directories(dir, ec);

    std::string base = trimSlash(serverUrl);
    fs::path audio = dir / (song.remoteId + ".mp3");
    if (!fs::exists(audio)) {
        std::string url = base + "/Audio/" + song.remoteId + "/stream.mp3?audioCodec=mp3&audioBitRate=192000";
        if (!downloadTo(url, token, audio, error)) return false;
    }
    song.filePath = audio.string();

    // Cover is optional: try the track's own image, then the album's.
    fs::path cover = dir / (song.remoteId + ".jpg");
    if (!fs::exists(cover)) {
        const std::string query = "/Images/Primary?maxHeight=300&format=Jpg";
        if (!downloadTo(base + "/Items/" + song.remoteId + query, token, cover, nullptr) && !song.remoteAlbumId.empty())
            downloadTo(base + "/Items/" + song.remoteAlbumId + query, token, cover, nullptr);
    }
    if (fs::exists(cover)) song.artworkPath = cover.string();
    return true;
}
