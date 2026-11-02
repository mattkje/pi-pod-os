#include "net/WebServer.h"
#include "media/Library.h"
#include "media/MediaLibrary.h"
#include <SDL2/SDL.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sys/statvfs.h>
#include <unistd.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

// From Utils.h, which can't be included here: OpenSSL (pulled in by httplib) also defines `UI`.
std::string localIpAddress();

namespace {
    constexpr unsigned long long MAX_UPLOAD = 4ULL * 1024 * 1024 * 1024; // 4 GB per file (videos)
    const char *AUDIO_EXTENSIONS[] = {".mp3", ".flac", ".ogg", ".wav", ".opus"};

    std::string randomDigits(int n) {
        std::random_device rd;
        std::uniform_int_distribution<int> d(0, 9);
        std::string s;
        for (int i = 0; i < n; i++) s += static_cast<char>('0' + d(rd));
        return s;
    }

    std::string randomHex(int bytes) {
        std::random_device rd;
        std::uniform_int_distribution<int> d(0, 255);
        static const char *hex = "0123456789abcdef";
        std::string s;
        for (int i = 0; i < bytes; i++) {
            int b = d(rd);
            s += hex[b >> 4];
            s += hex[b & 15];
        }
        return s;
    }

    long long nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    bool isAudioName(const std::string &name) {
        std::string ext = lower(fs::path(name).extension().string());
        return std::any_of(std::begin(AUDIO_EXTENSIONS), std::end(AUDIO_EXTENSIONS),
                           [&](const char *e) { return ext == e; });
    }

    // Keeps only the file name and drops characters that are unsafe in file names.
    // Non-ASCII (UTF-8) characters are kept so "Kåre - Sång.mp3" survives.
    std::string sanitizeFileName(const std::string &raw) {
        std::string name = raw;
        auto slash = name.find_last_of("/\\");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        std::string out;
        for (unsigned char c: name) {
            if (c >= 0x80 || std::isalnum(c) || std::string(" ._-()&',![]+").find(static_cast<char>(c)) != std::string::npos)
                out += static_cast<char>(c);
            else
                out += '_';
        }
        while (!out.empty() && (out.front() == '.' || out.front() == ' ')) out.erase(0, 1);
        if (out.size() > 180) {
            std::string ext = fs::path(out).extension().string();
            out = out.substr(0, 180 - ext.size()) + ext;
        }
        return out;
    }

    // "song.mp3" -> "song (1).mp3" while the name is taken.
    fs::path uniquePath(const fs::path &dir, const std::string &name) {
        fs::path p = dir / name;
        std::string stem = fs::path(name).stem().string(), ext = fs::path(name).extension().string();
        for (int i = 1; fs::exists(p) && i < 1000; i++) p = dir / (stem + " (" + std::to_string(i) + ")" + ext);
        return p;
    }

    // Resolves a path relative to the music folder; empty if it would escape the folder.
    fs::path insideMusicDir(const std::string &musicDir, const std::string &relative) {
        if (relative.empty() || relative.find("..") != std::string::npos || relative.front() == '/') return {};
        std::error_code ec;
        fs::path root = fs::weakly_canonical(musicDir, ec);
        fs::path target = fs::weakly_canonical(root / relative, ec);
        if (ec) return {};
        auto r = root.string(), t = target.string();
        if (t.size() <= r.size() || t.compare(0, r.size(), r) != 0 || t[r.size()] != '/') return {};
        return target;
    }

    unsigned long long freeSpace(const std::string &dir) {
        struct statvfs v{};
        if (statvfs(dir.c_str(), &v) != 0) return 0;
        return static_cast<unsigned long long>(v.f_bavail) * v.f_frsize;
    }

    void sendJson(httplib::Response &res, const json &j, int status = 200) {
        res.status = status;
        res.set_content(j.dump(), "application/json");
    }

    std::string hostName() {
        char buf[256] = {};
        if (gethostname(buf, sizeof(buf) - 1) != 0) return "pipod";
        std::string h = buf;
        if (h.size() > 6 && lower(h.substr(h.size() - 6)) == ".local") h = h.substr(0, h.size() - 6);
        return h;
    }

    std::string portSuffix(int port) {
        return port == 80 ? "" : ":" + std::to_string(port);
    }
}

WebServer::WebServer() = default;

WebServer::~WebServer() {
    stop();
}

void WebServer::setDeviceName(const std::string &name) {
    std::lock_guard<std::mutex> lock(nameMutex_);
    deviceName_ = name;
}

namespace {
    struct Category {
        const char *name;   // URL segment
        std::string dir;
        bool (*accept)(const std::string &);
        int change;
        const char *typesError;
    };

    bool acceptAudio(const std::string &n) { return isAudioName(n); }

    std::vector<Category> categories() {
        const char *home = std::getenv("HOME");
        std::string h = home ? home : ".";
        return {
            {"music", (fs::path(h) / "Music").string(), &acceptAudio, WebServer::MusicChanged,
             "Only mp3, flac, ogg, opus and wav files are supported"},
            {"videos", Media::videosDir(), &Media::isVideo, WebServer::VideosChanged,
             "Only mp4, m4v, mkv, mov, webm and avi files are supported"},
            {"photos", Media::photosDir(), &Media::isPhoto, WebServer::PhotosChanged,
             "Only jpg, png, gif, webp and bmp files are supported"},
        };
    }
}

bool WebServer::start(const std::string &webDir, const std::string &deviceName, int port) {
    if (running()) return true;
    webDir_ = webDir;
    setDeviceName(deviceName);
    std::error_code ec;
    for (const auto &c: categories()) fs::create_directories(c.dir, ec);

    code_ = randomDigits(4);
    token_ = randomHex(16);
    // A failed bind leaves an httplib::Server unusable, so each port gets a fresh server.
    for (int candidate: {port, 8080, 8081}) {
        if (candidate <= 0) continue;
        server_ = std::make_unique<httplib::Server>();
        routes();
        if (server_->bind_to_port("0.0.0.0", candidate)) {
            port_ = candidate;
            break;
        }
    }
    if (!running()) {
        SDL_Log("Web interface: could not bind a port");
        server_.reset();
        return false;
    }
    SDL_Log("Web interface on port %d", port_.load());
    thread_ = std::thread([this]() { server_->listen_after_bind(); });
    return true;
}

void WebServer::stop() {
    if (server_) server_->stop();
    if (thread_.joinable()) thread_.join();
    server_.reset();
    port_ = 0;
}

std::vector<WebServer::PodcastCommand> WebServer::takePodcastCommands() {
    std::lock_guard<std::mutex> lock(podcastMutex_);
    std::vector<PodcastCommand> out;
    out.swap(podcastCommands_);
    return out;
}

void WebServer::setPodcasts(const std::vector<PodcastInfo> &podcasts) {
    std::lock_guard<std::mutex> lock(podcastMutex_);
    podcasts_ = podcasts;
}

void WebServer::routes() {
    auto &svr = *server_;
    svr.set_payload_max_length(MAX_UPLOAD);

    auto authorized = [this](const httplib::Request &req) {
        std::string cookie = req.get_header_value("Cookie");
        return cookie.find("pipod=" + token_) != std::string::npos;
    };

    // ---- Page ----
    svr.Get("/", [this](const httplib::Request &, httplib::Response &res) {
        std::ifstream in(fs::path(webDir_) / "index.html", std::ios::binary);
        if (!in) {
            res.status = 500;
            res.set_content("Web interface files are missing.", "text/plain");
            return;
        }
        std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        res.set_header("Cache-Control", "no-store");
        res.set_content(html, "text/html; charset=utf-8");
    });

    // ---- Login with the code shown on the device ----
    svr.Post("/api/login", [this](const httplib::Request &req, httplib::Response &res) {
        if (nowMs() < lockedUntil_) {
            sendJson(res, {{"error", "Too many attempts. Wait a moment and try again."}}, 429);
            return;
        }
        std::string code;
        try {
            code = json::parse(req.body).value("code", std::string());
        } catch (...) {
        }
        if (code != code_) {
            if (++failedLogins_ >= 5) {
                failedLogins_ = 0;
                lockedUntil_ = nowMs() + 30000;
            }
            sendJson(res, {{"error", "Wrong code"}}, 401);
            return;
        }
        failedLogins_ = 0;
        res.set_header("Set-Cookie", "pipod=" + token_ + "; Path=/; HttpOnly; SameSite=Strict");
        sendJson(res, {{"ok", true}});
    });

    // ---- Device info ----
    svr.Get("/api/info", [this, authorized](const httplib::Request &req, httplib::Response &res) {
        std::string name;
        {
            std::lock_guard<std::mutex> lock(nameMutex_);
            name = deviceName_;
        }
        json j{{"name", name}, {"authorized", authorized(req)}};
        if (authorized(req)) j["free"] = freeSpace(categories()[0].dir);
        sendJson(res, j);
    });

    // ---- Files: /api/<music|videos|photos>/files[/<path>] ----
    for (const Category &cat: categories()) {
        std::string base = std::string("/api/") + cat.name + "/files";

        svr.Get(base, [cat, authorized](const httplib::Request &req, httplib::Response &res) {
            if (!authorized(req)) return sendJson(res, {{"error", "Not logged in"}}, 401);
            json list = json::array();
            std::error_code ec;
            fs::path root = fs::weakly_canonical(cat.dir, ec);
            if (cat.change == MusicChanged) {
                for (const Song &s: Library::scanLocal({cat.dir})) {
                    fs::path p = fs::weakly_canonical(s.filePath, ec);
                    list.push_back({{"path", fs::relative(p, root, ec).generic_string()}, {"title", s.title},
                                    {"subtitle", s.artist + (s.album.empty() ? "" : " — " + s.album)},
                                    {"size", fs::file_size(p, ec)}});
                }
            } else {
                for (const MediaFile &f: cat.change == VideosChanged ? Media::scanVideos() : Media::scanPhotos()) {
                    fs::path p = fs::weakly_canonical(f.path, ec);
                    list.push_back({{"path", fs::relative(p, root, ec).generic_string()}, {"title", f.name},
                                    {"subtitle", fs::path(f.path).extension().string().substr(1)}, {"size", f.size}});
                }
            }
            sendJson(res, list);
        });

        // Upload: body = file contents
        svr.Put(base + "/(.+)", [this, cat, authorized](const httplib::Request &req, httplib::Response &res,
                                                        const httplib::ContentReader &reader) {
            if (!authorized(req)) return sendJson(res, {{"error", "Not logged in"}}, 401);
            std::string name = sanitizeFileName(req.matches[1]);
            if (name.empty() || !cat.accept(name)) return sendJson(res, {{"error", cat.typesError}}, 415);

            unsigned long long declared = 0;
            if (req.has_header("Content-Length")) declared = std::stoull(req.get_header_value("Content-Length"));
            if (declared > MAX_UPLOAD) return sendJson(res, {{"error", "File is too large"}}, 413);
            if (declared + 50ULL * 1024 * 1024 > freeSpace(cat.dir))
                return sendJson(res, {{"error", "Not enough space on the PiPod"}}, 507);

            fs::path tmp = fs::path(cat.dir) / (".upload-" + randomHex(6) + ".part");
            std::ofstream out(tmp, std::ios::binary);
            if (!out) return sendJson(res, {{"error", "Can't write to the folder"}}, 500);

            unsigned long long received = 0;
            bool tooBig = false;
            bool ok = reader([&](const char *data, size_t len) {
                received += len;
                if (received > MAX_UPLOAD) {
                    tooBig = true;
                    return false;
                }
                out.write(data, static_cast<std::streamsize>(len));
                return out.good();
            });
            out.close();

            std::error_code ec;
            if (!ok || tooBig || received == 0) {
                fs::remove(tmp, ec);
                return sendJson(res, {{"error", tooBig ? "File is too large" : "Upload failed"}}, tooBig ? 413 : 400);
            }
            fs::path dest = uniquePath(cat.dir, name);
            fs::rename(tmp, dest, ec);
            if (ec) {
                fs::remove(tmp, ec);
                return sendJson(res, {{"error", "Can't save the file"}}, 500);
            }
            changes_ |= cat.change;
            sendJson(res, {{"ok", true}, {"path", dest.filename().string()}});
        });

        // Delete: path relative to the folder
        svr.Delete(base + "/(.+)", [this, cat, authorized](const httplib::Request &req, httplib::Response &res) {
            if (!authorized(req)) return sendJson(res, {{"error", "Not logged in"}}, 401);
            fs::path target = insideMusicDir(cat.dir, req.matches[1]);
            std::error_code ec;
            if (target.empty() || !cat.accept(target.filename().string()) || !fs::is_regular_file(target, ec))
                return sendJson(res, {{"error", "File not found"}}, 404);
            fs::remove(target, ec);
            if (ec) return sendJson(res, {{"error", "Can't delete the file"}}, 500);
            if (cat.change == MusicChanged) {
                // Also remove a sidecar cover with the same name, if any.
                for (const char *ext: {".png", ".jpg", ".jpeg"}) {
                    fs::path cover = target;
                    cover.replace_extension(ext);
                    fs::remove(cover, ec);
                }
            }
            changes_ |= cat.change;
            sendJson(res, {{"ok", true}});
        });
    }

    // ---- Podcasts ----
    svr.Get("/api/podcasts", [this, authorized](const httplib::Request &req, httplib::Response &res) {
        if (!authorized(req)) return sendJson(res, {{"error", "Not logged in"}}, 401);
        json list = json::array();
        std::lock_guard<std::mutex> lock(podcastMutex_);
        for (const auto &p: podcasts_)
            list.push_back({{"url", p.url}, {"title", p.title}, {"episodes", p.episodes}, {"error", p.error}});
        sendJson(res, list);
    });

    svr.Post("/api/podcasts", [this, authorized](const httplib::Request &req, httplib::Response &res) {
        if (!authorized(req)) return sendJson(res, {{"error", "Not logged in"}}, 401);
        std::string url;
        try {
            url = json::parse(req.body).value("url", std::string());
        } catch (...) {
        }
        while (!url.empty() && std::isspace(static_cast<unsigned char>(url.back()))) url.pop_back();
        while (!url.empty() && std::isspace(static_cast<unsigned char>(url.front()))) url.erase(0, 1);
        if ((url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) || url.size() > 2000)
            return sendJson(res, {{"error", "Enter a feed address starting with http:// or https://"}}, 400);
        std::lock_guard<std::mutex> lock(podcastMutex_);
        podcastCommands_.push_back({true, url});
        sendJson(res, {{"ok", true}});
    });

    svr.Delete("/api/podcasts", [this, authorized](const httplib::Request &req, httplib::Response &res) {
        if (!authorized(req)) return sendJson(res, {{"error", "Not logged in"}}, 401);
        std::string url = req.get_param_value("url");
        if (url.empty()) return sendJson(res, {{"error", "Missing url"}}, 400);
        std::lock_guard<std::mutex> lock(podcastMutex_);
        podcastCommands_.push_back({false, url});
        sendJson(res, {{"ok", true}});
    });
}

std::string webAddressByName(const WebServer &server) {
    if (!server.running()) return {};
    return "http://" + hostName() + ".local" + portSuffix(server.port());
}

std::string webAddressByIp(const WebServer &server) {
    std::string ip = localIpAddress();
    if (!server.running() || ip.empty()) return {};
    return "http://" + ip + portSuffix(server.port());
}
