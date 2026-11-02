#include "app/I18n.h"
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace {
    std::unordered_map<std::string, std::string> table;
    std::string currentCode = "en";
}

const std::vector<I18n::Language> &I18n::languages() {
    static const std::vector<Language> list{
        {"en", "English"},
        {"nb", "Norsk"},
        {"sv", "Svenska"},
        {"da", "Dansk"},
        {"de", "Deutsch"},
        {"fr", "Fran\xC3\xA7" "ais"},
        {"es", "Espa\xC3\xB1ol"},
    };
    return list;
}

bool I18n::load(const std::string &assetsDir, const std::string &code) {
    table.clear();
    currentCode = code.empty() ? "en" : code;
    if (currentCode == "en") return true;

    std::filesystem::path file = std::filesystem::path(assetsDir) / "lang" / (currentCode + ".json");
    std::ifstream in(file);
    if (!in) {
        SDL_Log("No translation file %s", file.c_str());
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(in);
        for (auto it = j.begin(); it != j.end(); ++it)
            if (it.value().is_string()) table[it.key()] = it.value().get<std::string>();
    } catch (const std::exception &e) {
        SDL_Log("Bad translation file %s: %s", file.c_str(), e.what());
        return false;
    }
    return true;
}

const std::string &I18n::current() {
    return currentCode;
}

std::string tr(const std::string &english) {
    auto it = table.find(english);
    return it == table.end() || it->second.empty() ? english : it->second;
}

std::string trf(const std::string &english, const std::vector<std::string> &values) {
    std::string s = tr(english);
    size_t pos = 0;
    for (const auto &v: values) {
        pos = s.find("{}", pos);
        if (pos == std::string::npos) break;
        s.replace(pos, 2, v);
        pos += v.size();
    }
    return s;
}
