#include "media/Podcasts.h"
#include "net/Net.h"
#include <tinyxml2.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace tinyxml2;

namespace {
    std::string text(const XMLElement *parent, const char *name) {
        const XMLElement *e = parent ? parent->FirstChildElement(name) : nullptr;
        const char *t = e ? e->GetText() : nullptr;
        std::string s = t ? t : "";
        // Trim whitespace and newlines common in feeds
        size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }

    // "Tue, 12 Mar 2026 10:00:00 +0000" -> "12 Mar 2026"
    std::string shortDate(const std::string &rfc822) {
        std::istringstream in(rfc822);
        std::vector<std::string> parts;
        std::string p;
        while (in >> p) parts.push_back(p);
        size_t start = (!parts.empty() && parts[0].back() == ',') ? 1 : 0;
        if (parts.size() < start + 3) return rfc822;
        return parts[start] + " " + parts[start + 1] + " " + parts[start + 2];
    }

    // itunes:duration is either seconds ("2712") or "HH:MM:SS" / "MM:SS".
    std::string formatDuration(const std::string &d) {
        if (d.empty()) return "";
        bool digits = d.find_first_not_of("0123456789") == std::string::npos;
        if (!digits) return d;
        long s = std::atol(d.c_str());
        char buf[48];
        if (s >= 3600) std::snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
        else std::snprintf(buf, sizeof(buf), "%ld:%02ld", s / 60, s % 60);
        return buf;
    }

    std::string readFile(const fs::path &p) {
        std::ifstream in(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    std::string extensionOf(const std::string &url, const std::string &fallback) {
        std::string path = url.substr(0, url.find_first_of("?#"));
        std::string ext = fs::path(path).extension().string();
        for (auto &c: ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext.size() < 2 || ext.size() > 5) return fallback;
        return ext;
    }
}

bool Podcasts::parse(const std::string &xml, Show &show, std::string *imageUrl) {
    XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != XML_SUCCESS) return false;
    const XMLElement *rss = doc.FirstChildElement("rss");
    const XMLElement *channel = rss ? rss->FirstChildElement("channel") : nullptr;
    if (!channel) return false;

    show.title = text(channel, "title");
    show.author = text(channel, "itunes:author");
    if (imageUrl) {
        imageUrl->clear();
        if (const XMLElement *img = channel->FirstChildElement("itunes:image"))
            if (const char *href = img->Attribute("href")) *imageUrl = href;
        if (imageUrl->empty()) *imageUrl = text(channel->FirstChildElement("image"), "url");
    }

    show.episodes.clear();
    for (const XMLElement *item = channel->FirstChildElement("item"); item; item = item->NextSiblingElement("item")) {
        const XMLElement *enc = item->FirstChildElement("enclosure");
        const char *url = enc ? enc->Attribute("url") : nullptr;
        if (!url || !*url) continue;
        const char *type = enc->Attribute("type");
        if (type && std::string(type).rfind("video/", 0) == 0) continue; // audio podcasts only
        Episode e;
        e.title = text(item, "title");
        e.url = url;
        e.date = shortDate(text(item, "pubDate"));
        e.duration = formatDuration(text(item, "itunes:duration"));
        show.episodes.push_back(std::move(e));
    }
    if (show.title.empty()) show.title = "Podcast";
    return true;
}

std::vector<Podcasts::Show> Podcasts::load(const std::vector<std::string> &feedUrls, const std::string &cacheDir) {
    std::vector<Show> shows;
    fs::path dir = fs::path(cacheDir) / "podcasts";
    std::error_code ec;
    fs::create_directories(dir / "feeds", ec);
    fs::create_directories(dir / "art", ec);

    for (const auto &url: feedUrls) {
        Show show;
        show.feedUrl = url;
        std::string id = Net::hashName(url);
        fs::path cached = dir / "feeds" / (id + ".xml");

        std::string xml;
        std::string err = Net::get(url, xml);
        std::string imageUrl;
        if (err.empty() && parse(xml, show, &imageUrl)) {
            std::ofstream(cached, std::ios::binary) << xml;
        } else {
            // Offline or broken feed: fall back to the last good copy.
            if (!fs::exists(cached) || !parse(readFile(cached), show, &imageUrl)) {
                show.title = url;
                show.error = err.empty() ? "Not a podcast feed" : err;
                shows.push_back(std::move(show));
                continue;
            }
        }

        if (!imageUrl.empty()) {
            fs::path art = dir / "art" / (id + extensionOf(imageUrl, ".jpg"));
            if (fs::exists(art) || Net::download(imageUrl, art.string()).empty()) show.artworkPath = art.string();
        }
        shows.push_back(std::move(show));
    }
    return shows;
}

Song Podcasts::toSong(const Show &show, const Episode &episode, const std::string &cacheDir) {
    Song s;
    s.title = episode.title;
    s.artist = show.title;
    s.album = episode.date;
    s.artworkPath = show.artworkPath;
    s.downloadUrl = episode.url;
    fs::path file = fs::path(cacheDir) / "podcasts" / "episodes" /
                    (Net::hashName(episode.url) + extensionOf(episode.url, ".mp3"));
    s.downloadPath = file.string();
    std::error_code ec;
    if (fs::exists(file, ec)) s.filePath = file.string(); // already downloaded
    return s;
}
