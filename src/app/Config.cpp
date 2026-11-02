#include "app/Config.h"
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
    std::string envOr(const char *name, const std::string &fallback) {
        const char *v = std::getenv(name);
        return (v && *v) ? std::string(v) : fallback;
    }

    std::string home() {
        return envOr("HOME", ".");
    }

    fs::path configFile() {
        return fs::path(Config::configDir()) / "config.json";
    }

    std::string repeatToString(RepeatMode m) {
        switch (m) {
            case RepeatMode::One: return "one";
            case RepeatMode::All: return "all";
            default: return "off";
        }
    }

    RepeatMode repeatFromString(const std::string &s) {
        if (s == "one") return RepeatMode::One;
        if (s == "all") return RepeatMode::All;
        return RepeatMode::Off;
    }
}

std::string Config::configDir() {
    return (fs::path(envOr("XDG_CONFIG_HOME", home() + "/.config")) / "pi-pod-os").string();
}

std::string Config::cacheDir() {
    return (fs::path(envOr("XDG_CACHE_HOME", home() + "/.cache")) / "pi-pod-os").string();
}

std::string Config::assetsDir() {
    if (char *base = SDL_GetBasePath()) {
        fs::path p = fs::path(base) / "assets";
        SDL_free(base);
        if (fs::exists(p)) return p.string();
    }
    return "assets";
}

Settings Config::load() {
    Settings s;
    std::ifstream in(configFile());
    if (!in) return s;

    try {
        json j = json::parse(in);
        s.setupDone = j.value("setupDone", s.setupDone);
        s.language = j.value("language", s.language);
        s.deviceName = j.value("deviceName", s.deviceName);
        s.timezone = j.value("timezone", s.timezone);
        s.screenFlipped = j.value("screenFlipped", s.screenFlipped);
        s.volume = std::clamp(j.value("volume", s.volume), 0, 100);
        s.shuffle = j.value("shuffle", s.shuffle);
        s.repeat = repeatFromString(j.value("repeat", std::string("off")));
        if (j.contains("podcasts") && j["podcasts"].is_array())
            for (const auto &u: j["podcasts"])
                if (u.is_string()) s.podcastFeeds.push_back(u.get<std::string>());
        if (j.contains("jellyfin")) {
            const json &jf = j["jellyfin"];
            s.jellyfinUrl = jf.value("url", std::string());
            s.jellyfinUser = jf.value("user", std::string());
            s.jellyfinUserId = jf.value("userId", std::string());
            s.jellyfinToken = jf.value("token", std::string());
        }
    } catch (const std::exception &e) {
        SDL_Log("Failed to parse config: %s", e.what());
    }
    return s;
}

void Config::save(const Settings &s) {
    json j;
    j["setupDone"] = s.setupDone;
    j["language"] = s.language;
    j["deviceName"] = s.deviceName;
    j["timezone"] = s.timezone;
    j["screenFlipped"] = s.screenFlipped;
    j["volume"] = s.volume;
    j["shuffle"] = s.shuffle;
    j["repeat"] = repeatToString(s.repeat);
    j["podcasts"] = s.podcastFeeds;
    j["jellyfin"] = {
        {"url", s.jellyfinUrl},
        {"user", s.jellyfinUser},
        {"userId", s.jellyfinUserId},
        {"token", s.jellyfinToken},
    };

    std::error_code ec;
    fs::create_directories(configDir(), ec);
    std::ofstream out(configFile());
    if (!out) {
        SDL_Log("Failed to write config to %s", configFile().c_str());
        return;
    }
    out << j.dump(2);
}

void Config::reset() {
    std::error_code ec;
    fs::remove(configFile(), ec);
    fs::remove_all(cacheDir(), ec);
}
