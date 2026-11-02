#pragma once
#include <string>
#include <vector>

// Minimal UI translation: English strings are the keys, assets/lang/<code>.json maps them to the
// selected language. Missing entries fall back to English.
namespace I18n {
    struct Language {
        std::string code;       // "en", "nb", ...
        std::string nativeName; // shown in the language list, e.g. "Norsk"
    };

    // Languages offered in Setup and Settings (English first).
    const std::vector<Language> &languages();

    // Loads assets/lang/<code>.json ("en" clears the table). Returns false if the file is missing.
    bool load(const std::string &assetsDir, const std::string &code);

    const std::string &current();
}

// Translate an English UI string.
std::string tr(const std::string &english);

// Translate, then replace each "{}" in order with the given values.
std::string trf(const std::string &english, const std::vector<std::string> &values);
