#include "media/MediaLibrary.h"
#include "net/Net.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
    std::string home() {
        const char *h = std::getenv("HOME");
        return h ? h : ".";
    }

    std::string lowerExt(const std::string &name) {
        std::string ext = fs::path(name).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        return ext;
    }

    std::vector<MediaFile> scan(const std::string &dir, bool (*accept)(const std::string &)) {
        std::vector<MediaFile> out;
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            const fs::path &p = it->path();
            std::string fname = p.filename().string();
            if (fname.empty() || fname[0] == '.' || !it->is_regular_file(ec) || !accept(fname)) continue;
            MediaFile f;
            f.path = p.string();
            f.name = p.stem().string();
            f.size = it->file_size(ec);
            auto t = it->last_write_time(ec);
            f.mtime = std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
            out.push_back(std::move(f));
        }
        return out;
    }

    // Smooth downscale: halve with linear filtering until close, then a final linear stretch.
    SDL_Surface *scaleTo(SDL_Surface *src, int w, int h) {
        SDL_Surface *cur = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
        if (!cur) return nullptr;
        while (cur->w / 2 >= w && cur->h / 2 >= h) {
            SDL_Surface *half = SDL_CreateRGBSurfaceWithFormat(0, cur->w / 2, cur->h / 2, 32, SDL_PIXELFORMAT_ARGB8888);
            if (!half || SDL_SoftStretchLinear(cur, nullptr, half, nullptr) != 0) {
                if (half) SDL_FreeSurface(half);
                break;
            }
            SDL_FreeSurface(cur);
            cur = half;
        }
        SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        if (out) SDL_SoftStretchLinear(cur, nullptr, out, nullptr);
        SDL_FreeSurface(cur);
        return out;
    }
}

std::string Media::videosDir() { return (fs::path(home()) / "Videos").string(); }
std::string Media::photosDir() { return (fs::path(home()) / "Pictures").string(); }

bool Media::isVideo(const std::string &name) {
    std::string e = lowerExt(name);
    return e == ".mp4" || e == ".m4v" || e == ".mkv" || e == ".mov" || e == ".webm" || e == ".avi";
}

bool Media::isPhoto(const std::string &name) {
    std::string e = lowerExt(name);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".bmp" || e == ".gif" || e == ".webp";
}

std::vector<MediaFile> Media::scanVideos() {
    auto v = scan(videosDir(), &Media::isVideo);
    std::sort(v.begin(), v.end(), [](const MediaFile &a, const MediaFile &b) { return a.name < b.name; });
    return v;
}

std::vector<MediaFile> Media::scanPhotos() {
    auto v = scan(photosDir(), &Media::isPhoto);
    std::sort(v.begin(), v.end(), [](const MediaFile &a, const MediaFile &b) { return a.mtime > b.mtime; });
    return v;
}

// ---------------- PhotoLoader ----------------

PhotoLoader::PhotoLoader(SDL_Renderer *renderer, const std::string &cacheDir)
    : renderer_(renderer), cacheDir_((fs::path(cacheDir) / "thumbs").string()) {
    std::error_code ec;
    fs::create_directories(cacheDir_, ec);
    thread_ = std::thread([this]() { work(); });
}

PhotoLoader::~PhotoLoader() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        quit_ = true;
        jobs_.clear();
    }
    cv_.notify_all();
    thread_.join();
    for (auto &r: results_) SDL_FreeSurface(r.surface);
    clear();
}

void PhotoLoader::clear() {
    for (auto &kv: textures_) SDL_DestroyTexture(kv.second);
    textures_.clear();
    lastUsed_.clear();
}

SDL_Texture *PhotoLoader::thumbnail(const MediaFile &photo) { return request(photo, true); }
SDL_Texture *PhotoLoader::full(const MediaFile &photo) { return request(photo, false); }

SDL_Texture *PhotoLoader::request(const MediaFile &photo, bool thumb) {
    std::string key = (thumb ? "t:" : "f:") + photo.path;
    auto it = textures_.find(key);
    if (it != textures_.end()) {
        lastUsed_[key] = SDL_GetTicks();
        return it->second;
    }
    if (!pending_[key]) {
        pending_[key] = true;
        std::lock_guard<std::mutex> lock(mutex_);
        // Full images jump the queue: the viewer is waiting for them.
        if (thumb) jobs_.push_back({key, photo.path, thumb, photo.mtime});
        else jobs_.push_front({key, photo.path, thumb, photo.mtime});
        cv_.notify_one();
    }
    return nullptr;
}

void PhotoLoader::update() {
    std::vector<Result> ready;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready.swap(results_);
    }
    for (auto &r: ready) {
        if (!r.surface) continue; // unreadable image: leave it marked pending so it isn't retried
        pending_.erase(r.key);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer_, r.surface);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        SDL_FreeSurface(r.surface);
        if (t) {
            textures_[r.key] = t;
            lastUsed_[r.key] = SDL_GetTicks();
        }
    }
    // Keep memory bounded: at most 80 textures, dropping the least recently used.
    while (textures_.size() > 80) {
        auto oldest = std::min_element(lastUsed_.begin(), lastUsed_.end(),
                                       [](const auto &a, const auto &b) { return a.second < b.second; });
        SDL_DestroyTexture(textures_[oldest->first]);
        textures_.erase(oldest->first);
        lastUsed_.erase(oldest);
    }
}

SDL_Surface *PhotoLoader::loadScaled(const Job &job) {
    fs::path cached = fs::path(cacheDir_) / (Net::hashName(job.path + "|" + std::to_string(job.mtime)) + ".png");
    std::error_code ec;
    if (job.thumb && fs::exists(cached, ec)) {
        if (SDL_Surface *s = IMG_Load(cached.string().c_str())) return s;
    }

    SDL_Surface *src = IMG_Load(job.path.c_str());
    if (!src) return nullptr;
    SDL_Surface *out = nullptr;
    if (job.thumb) {
        // Centre-crop to a square, then scale to 160x160.
        int side = std::min(src->w, src->h);
        SDL_Rect crop{(src->w - side) / 2, (src->h - side) / 2, side, side};
        SDL_Surface *sq = SDL_CreateRGBSurfaceWithFormat(0, side, side, 32, SDL_PIXELFORMAT_ARGB8888);
        if (sq) {
            SDL_Surface *conv = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
            if (conv) {
                SDL_BlitSurface(conv, &crop, sq, nullptr);
                SDL_FreeSurface(conv);
                out = scaleTo(sq, 160, 160);
            }
            SDL_FreeSurface(sq);
        }
        if (out) IMG_SavePNG(out, cached.string().c_str());
    } else {
        double scale = std::min({1.0, 640.0 / src->w, 480.0 / src->h});
        out = scaleTo(src, std::max(1, int(src->w * scale)), std::max(1, int(src->h * scale)));
    }
    SDL_FreeSurface(src);
    return out;
}

void PhotoLoader::work() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&]() { return quit_ || !jobs_.empty(); });
            if (quit_) return;
            job = jobs_.front();
            jobs_.pop_front();
        }
        SDL_Surface *s = loadScaled(job);
        std::lock_guard<std::mutex> lock(mutex_);
        if (quit_) {
            if (s) SDL_FreeSurface(s);
            return;
        }
        results_.push_back({job.key, s});
    }
}
