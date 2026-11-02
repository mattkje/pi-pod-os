#include "media/Library.h"
#include "app/Config.h"
#include <SDL2/SDL_image.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace fs = std::filesystem;

bool Library::isAudioFile(const std::string &path) {
    static const char *EXTENSIONS[] = {".mp3", ".m4a", ".m4b", ".aac", ".flac", ".wav", ".aif", ".aiff",
                                       ".ogg", ".oga", ".opus", ".wma"};
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return std::any_of(std::begin(EXTENSIONS), std::end(EXTENSIONS), [&](const char *e) { return ext == e; });
}

namespace {
    // Looks for "<song>.png/.jpg" in an "artwork" folder next to the file, or next to the file itself,
    // or a "cover.jpg/folder.jpg" in the same folder.
    std::string findArtwork(const fs::path &audio) {
        std::string stem = audio.stem().string();
        fs::path dir = audio.parent_path();
        const char *exts[] = {".png", ".jpg", ".jpeg"};
        for (const fs::path &d: {dir / "artwork", dir}) {
            for (const char *ext: exts) {
                fs::path p = d / (stem + ext);
                if (fs::exists(p)) return p.string();
            }
        }
        for (const char *name: {"cover.jpg", "cover.png", "folder.jpg", "folder.png"}) {
            fs::path p = dir / name;
            if (fs::exists(p)) return p.string();
        }
        return {};
    }
}

std::vector<std::string> Library::defaultMusicDirs() {
    std::vector<std::string> dirs{(fs::path(Config::assetsDir()) / "music").string()};
    if (const char *home = std::getenv("HOME")) dirs.push_back((fs::path(home) / "Music").string());
    return dirs;
}

std::vector<Song> Library::scanLocal(const std::vector<std::string> &dirs) {
    std::vector<Song> songs;
    for (const auto &dir: dirs) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;

        auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
        for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            const fs::path &path = it->path();
            if (!it->is_regular_file(ec) || !Library::isAudioFile(path.string())) continue;

            Song s;
            s.filePath = path.string();
            s.title = path.stem().string();
            s.artist = "Unknown Artist";

            TagLib::FileRef f(path.c_str());
            if (!f.isNull() && f.tag()) {
                TagLib::Tag *tag = f.tag();
                if (!tag->title().isEmpty()) s.title = tag->title().to8Bit(true);
                if (!tag->artist().isEmpty()) s.artist = tag->artist().to8Bit(true);
                s.album = tag->album().to8Bit(true);
            }
            s.artworkPath = findArtwork(path);
            songs.push_back(std::move(s));
        }
    }

    std::sort(songs.begin(), songs.end(), [](const Song &a, const Song &b) { return a.title < b.title; });
    return songs;
}

void Library::ensureArtwork(SDL_Renderer *renderer, Song &song) {
    if (song.artwork || song.artworkPath.empty()) return;
    // Covers are usually much larger than where they're drawn; linear filtering keeps them smooth.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_Texture *tex = IMG_LoadTexture(renderer, song.artworkPath.c_str());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    if (tex) {
        song.artwork.reset(tex, SDL_DestroyTexture);
    } else {
        song.artworkPath.clear(); // don't retry every frame
    }
}
