#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <curl/curl.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <atomic>
#include <cmath>
#include <future>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "app/Config.h"
#include "app/I18n.h"
#include "ui/Icons.h"
#include "net/JellyfinClient.h"
#include "media/Library.h"
#include "media/MediaLibrary.h"
#include "net/Net.h"
#include "media/Player.h"
#include "media/Podcasts.h"
#include "media/VideoPlayer.h"
#include "app/Setup.h"
#include "system/SystemControl.h"
#include "net/WebServer.h"
#include "ui/Utils.h"
#include "ui/pages/AboutPage.h"
#include "ui/pages/AddMediaPage.h"
#include "ui/pages/ComingSoonPage.h"
#include "ui/pages/MediaPages.h"
#include "ui/pages/MenuPage.h"
#include "ui/pages/NowPlayingPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/SongsPage.h"
#include "ui/pages/SplashPage.h"

namespace fs = std::filesystem;

namespace {
    struct Options {
        bool windowed = false;
        int width = 320; // logical screen size (2.8" PiTFT)
        int height = 240;
        bool native = false; // fullscreen at the display's own resolution instead of scaling 320x240
        int scale = 1; // window scale in windowed mode
        int displayW = 0, displayH = 0; // window size override, e.g. 480x640 to simulate a portrait panel
        int flip = -1;                  // -1 = use the Settings value, 0/1 = override (--flip, PIPOD_FLIP)
        std::string exportIcons; // write built-in menu icons as PNGs to this dir and exit
        bool setup = false;      // run the Setup Assistant even if it was completed
    };

    void printUsage(const char *argv0) {
        SDL_Log("Usage: %s [--windowed] [--size WxH] [--scale N]\n"
                "  --windowed   run in a window instead of fullscreen (also: PIPOD_WINDOWED=1)\n"
                "  --size WxH   logical screen size, default 320x240\n"
                "  --scale N    window scale factor in windowed mode, default 1\n"
                "  --setup      run the first-boot Setup Assistant (system changes are simulated off the device)\n"
                "  --display WxH  window size, e.g. 480x640 to try a portrait panel (the UI stays landscape)\n"
                "  --flip       turn the picture 180 degrees (also: PIPOD_FLIP=1, Settings > Flip Screen)\n"
                "  --native     fullscreen at the display resolution instead of scaling the 320x240 layout\n"
                "  --export-icons DIR  write the built-in menu icons as PNGs to DIR and exit", argv0);
    }

    Options parseArgs(int argc, char **argv) {
        Options o;
        if (const char *env = std::getenv("PIPOD_WINDOWED")) o.windowed = std::strcmp(env, "0") != 0;
        if (const char *env = std::getenv("PIPOD_FLIP")) o.flip = std::strcmp(env, "0") != 0 ? 1 : 0;

        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--windowed" || a == "-w") {
                o.windowed = true;
            } else if (a == "--fullscreen") {
                o.windowed = false;
            } else if (a == "--size" && i + 1 < argc) {
                int w = 0, h = 0;
                if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                    o.width = w;
                    o.height = h;
                }
            } else if (a == "--display" && i + 1 < argc) {
                int w = 0, h = 0;
                if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                    o.displayW = w;
                    o.displayH = h;
                    o.windowed = true;
                }
            } else if (a == "--flip") {
                o.flip = 1;
            } else if (a == "--native") {
                o.native = true;
            } else if (a == "--scale" && i + 1 < argc) {
                o.scale = std::clamp(std::atoi(argv[++i]), 1, 8);
            } else if (a == "--export-icons" && i + 1 < argc) {
                o.exportIcons = argv[++i];
                o.windowed = true;
            } else if (a == "--setup") {
                o.setup = true;
            } else if (a == "--version") {
                std::printf("%s\n", PIPOD_VERSION);
                std::exit(0);
            } else if (a == "--help" || a == "-h") {
                printUsage(argv[0]);
                std::exit(0);
            }
        }
        return o;
    }

    // Runs the device updater (`pipod-update check|install`) and returns its last output line,
    // "<status> <detail>", e.g. "available 0.2.0" or "error Can't reach update server".
    // "progress <0-100>" lines are forwarded to `progress` while it runs.
    std::string runUpdater(const std::string &action, std::atomic<int> *progress) {
        const char *cmd = std::getenv("PIPOD_UPDATE_CMD");
        std::string command = std::string(cmd ? cmd : "/usr/local/bin/pipod-update") + " " + action + " 2>&1";
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) return "error Can't run updater";
        std::string last;
        char buf[512];
        while (std::fgets(buf, sizeof(buf), pipe)) {
            std::string line = buf;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            if (line.rfind("progress ", 0) == 0) {
                if (progress) *progress = std::atoi(line.c_str() + 9);
                continue;
            }
            if (!line.empty()) last = line;
        }
        int rc = pclose(pipe);
        if (last.empty()) last = rc == 0 ? "error No output" : "error Updater failed";
        return last;
    }

    std::pair<std::string, std::string> splitStatus(const std::string &line) {
        auto sp = line.find(' ');
        if (sp == std::string::npos) return {line, ""};
        return {line.substr(0, sp), line.substr(sp + 1)};
    }

    struct JellyfinLoad {
        Jellyfin::AuthResult auth; // only set when logging in
        std::vector<Song> songs;
        std::vector<Jellyfin::Video> videos;
        std::string error;
    };

    // Saves the current frame as a PNG (F12). Returns the file path, or empty on failure.
    // With a rotation canvas the UI itself is saved (unrotated, at UI size).
    std::string saveScreenshot(SDL_Renderer *renderer, SDL_Texture *canvas, int w, int h) {
        std::error_code ec;
        fs::path dir = fs::path(Config::cacheDir()) / "screenshots";
        fs::create_directories(dir, ec);
        fs::path file = dir / ("pipod-" + std::to_string(SDL_GetTicks()) + ".png");

        int outW = w, outH = h;
        if (!canvas) SDL_GetRendererOutputSize(renderer, &outW, &outH);
        SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surf) return {};
        if (!canvas && w > 0) SDL_RenderSetLogicalSize(renderer, 0, 0);
        bool ok = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch) == 0;
        if (!canvas && w > 0) SDL_RenderSetLogicalSize(renderer, w, h); // w == 0: raw capture, keep as is
        ok = ok && IMG_SavePNG(surf, file.string().c_str()) == 0;
        SDL_FreeSurface(surf);
        return ok ? file.string() : std::string();
    }

#ifndef NDEBUG
    // Debug builds only: PIPOD_KEYS="down,down,return,..." replays key presses (one every 400 ms)
    // so UI flows can be exercised without a keyboard. "shot" takes a screenshot, "wait" idles.
    std::vector<std::string> scriptedKeys() {
        std::vector<std::string> keys;
        const char *env = std::getenv("PIPOD_KEYS");
        if (!env) return keys;
        std::string all = env, cur;
        for (char c: all) {
            if (c == ',') {
                keys.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) keys.push_back(cur);
        return keys;
    }
#endif

    void setStatus(AppState &state, const std::string &msg, Uint32 ms = 2500) {
        state.statusMessage = msg;
        state.statusUntil = ms ? SDL_GetTicks() + ms : 0;
    }
}

int main(int argc, char **argv) {
    Options opts = parseArgs(argc, argv);

    // ------------------ SDL INIT ------------------
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    TTF_Init();
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
        SDL_Log("SDL_mixer could not initialize! SDL_mixer Error: %s", Mix_GetError());
    Mix_Init(MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_OGG);

    SDL_Window *window;
    if (opts.windowed) {
        int winW = opts.displayW ? opts.displayW : opts.width * opts.scale;
        int winH = opts.displayH ? opts.displayH : opts.height * opts.scale;
        window = SDL_CreateWindow("PiPod OS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, 0);
    } else {
        window = SDL_CreateWindow("PiPod OS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_ShowCursor(SDL_DISABLE);
    }
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // The UI is laid out for a 320x240 landscape screen and scaled up to fit bigger displays.
    // --native lays it out at the display's own resolution instead.
    int outW = 0, outH = 0;
    SDL_GetRendererOutputSize(renderer, &outW, &outH);
    int screenW = opts.width, screenH = opts.height;
    if (!opts.windowed && opts.native) {
        screenW = std::max(outW, outH); // always landscape
        screenH = std::min(outW, outH);
    }

    // The picture is always landscape. On a portrait display (e.g. the 480x640 Waveshare panel) it is
    // turned 90 degrees; "flip" turns it a further 180. When turned, the UI is drawn into `canvas`
    // and copied rotated to the screen; otherwise SDL's logical size does the scaling.
    SDL_Texture *canvas = nullptr;
    double canvasAngle = 0;
    auto applyOrientation = [&](bool flipped) {
        double angle = (outH > outW ? 90 : 0) + (flipped ? 180 : 0);
        if (canvas) {
            SDL_DestroyTexture(canvas);
            canvas = nullptr;
        }
        canvasAngle = angle;
        if (angle == 0) {
            SDL_RenderSetLogicalSize(renderer, screenW, screenH);
            return;
        }
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screenW, screenH);
        if (!canvas) {
            SDL_Log("Rotation unavailable: %s", SDL_GetError());
            canvasAngle = 0;
            SDL_RenderSetLogicalSize(renderer, screenW, screenH);
        }
    };
    auto beginFrame = [&]() {
        if (canvas) SDL_SetRenderTarget(renderer, canvas);
    };
    auto endFrame = [&]() {
        if (!canvas) return;
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        bool sideways = canvasAngle == 90 || canvasAngle == 270;
        double rw = sideways ? screenH : screenW, rh = sideways ? screenW : screenH;
        double scale = std::min(outW / rw, outH / rh);
        if (scale >= 1) scale = std::floor(scale); // whole-number scaling keeps pixels crisp
        int w = int(screenW * scale), h = int(screenH * scale);
        SDL_Rect dst{(outW - w) / 2, (outH - h) / 2, w, h};
        SDL_RenderCopyEx(renderer, canvas, nullptr, &dst, canvasAngle, nullptr, SDL_FLIP_NONE);
    };

    if (!opts.exportIcons.empty()) {
        int n = MenuIcons::exportBuiltIn(renderer, opts.exportIcons);
        SDL_Quit();
        return n > 0 ? 0 : 1;
    }

    fs::path assets = Config::assetsDir();
    // TeX Gyre Heros: a free Helvetica clone (GUST Font License), matching the iPod classic's Helvetica.
    std::string fontPath = (assets / "fonts" / "texgyreheros-regular.otf").string();
    std::string boldPath = (assets / "fonts" / "texgyreheros-bold.otf").string();
    TTF_Font *font = TTF_OpenFont(fontPath.c_str(), 15);
    TTF_Font *smallFont = TTF_OpenFont(fontPath.c_str(), 12);
    TTF_Font *mediumFont = TTF_OpenFont(fontPath.c_str(), 13);
    TTF_Font *boldFont = TTF_OpenFont(boldPath.c_str(), 15);
    TTF_Font *listFont = TTF_OpenFont(boldPath.c_str(), 16);
    TTF_Font *titleFont = TTF_OpenFont(boldPath.c_str(), 13);
    if (!font || !smallFont || !mediumFont || !boldFont || !listFont || !titleFont) {
        SDL_Log("Failed to load fonts from %s: %s", (assets / "fonts").c_str(), TTF_GetError());
        return 1;
    }
    SDL_Texture *arrow = IMG_LoadTexture(renderer, (assets / "icons" / "arrow.png").string().c_str());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_Texture *logo = IMG_LoadTexture(renderer, (assets / "icons" / "logo.png").string().c_str());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    UI ui;
    ui.r = renderer;
    ui.font = font;
    ui.small = smallFont;
    ui.medium = mediumFont;
    ui.bold = boldFont;
    ui.listFont = listFont;
    ui.titleFont = titleFont;
    ui.arrow = arrow;
    ui.w = screenW;
    ui.h = screenH;
    ui.battery = readBatteryPercent();

    // Show the logo right away (same as the boot splash) while the library loads.
    const Uint32 appStart = SDL_GetTicks();
    {
        Settings saved = Config::load();
        applyOrientation(opts.flip >= 0 ? opts.flip == 1 : saved.screenFlipped);
    }
    beginFrame();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    drawSplash(ui, logo);
    endFrame();
    SDL_RenderPresent(renderer);

    // ------------------ APP STATE ------------------
    AppState state;
    state.settings = Config::load();
    if (const char *dev = std::getenv("PIPOD_DEVICE")) state.deviceMode = std::strcmp(dev, "0") != 0;
    SystemControl::setSimulated(!state.deviceMode);
    I18n::load(assets.string(), state.settings.language);

    const std::vector<MenuItem> mainMenu{
        {"Music", Screen::Music, MenuAction::Navigate, MenuIcon::Music},
        {"Videos", Screen::Videos, MenuAction::Navigate, MenuIcon::Videos},
        {"Photos", Screen::Photos, MenuAction::Navigate, MenuIcon::Photos},
        {"Podcasts", Screen::Podcasts, MenuAction::Navigate, MenuIcon::Podcasts},
        {"Extras", Screen::ComingSoon, MenuAction::Navigate, MenuIcon::Extras},
        {"Settings", Screen::Settings, MenuAction::Navigate, MenuIcon::Settings},
        {"Shuffle Songs", Screen::NowPlaying, MenuAction::ShuffleSongs, MenuIcon::Shuffle},
        {"Now Playing", Screen::NowPlaying, MenuAction::NowPlaying, MenuIcon::NowPlaying},
        {"About", Screen::About, MenuAction::Navigate, MenuIcon::About},
    };

    std::unique_ptr<Player> player = std::make_unique<Player>();
    player->setVolume(state.settings.volume);
    player->setRepeat(state.settings.repeat);
    player->setRemote(state.settings.jellyfinUrl, state.settings.jellyfinToken, Config::cacheDir());

    std::vector<Song> localSongs = Library::scanLocal(Library::defaultMusicDirs());
    std::vector<Song> remoteSongs;
    std::vector<Song> songs;
    auto rebuildLibrary = [&]() {
        songs = localSongs;
        songs.insert(songs.end(), remoteSongs.begin(), remoteSongs.end());
        // Covers of Jellyfin songs that were played before are already in the cache.
        for (Song &s: songs) {
            if (!s.isRemote() || !s.artworkPath.empty()) continue;
            fs::path cover = fs::path(Config::cacheDir()) / "jellyfin" / (s.remoteId + ".jpg");
            std::error_code ec;
            if (fs::exists(cover, ec)) s.artworkPath = cover.string();
        }
        std::stable_sort(songs.begin(), songs.end(), [](const Song &a, const Song &b) { return a.title < b.title; });
    };
    rebuildLibrary();

    // Web interface for managing music, videos, photos and podcasts from another device.
    // The service sets PIPOD_WEB_PORT=80 on the device; 8080 elsewhere.
    WebServer web;
    {
        const char *portEnv = std::getenv("PIPOD_WEB_PORT");
        int port = portEnv ? std::atoi(portEnv) : 8080;
        web.start((assets / "web").string(), state.settings.deviceName, port);
    }

    // ------------------ VIDEOS / PHOTOS / PODCASTS ------------------
    std::vector<MediaFile> localVideos = Media::scanVideos();
    std::vector<Jellyfin::Video> jellyfinVideos;
    std::vector<VideoItem> videos; // local files first, then Jellyfin
    auto rebuildVideos = [&]() {
        videos.clear();
        for (const auto &v: localVideos) videos.push_back({v.name, formatBytes(v.size), v.path, ""});
        for (const auto &v: jellyfinVideos)
            videos.push_back({v.title, "Jellyfin" + (v.detail.empty() ? "" : "  \xC2\xB7  " + v.detail), "", v.id});
    };
    rebuildVideos();
    std::vector<MediaFile> photos = Media::scanPhotos();
    VideoPlayer video;
    std::string videoTitle;
    std::string videoSession; // Jellyfin play session of the current video, "" for local files
    auto closeVideo = [&]() {
        video.close();
        if (!videoSession.empty()) {
            Jellyfin::stopTranscode(state.settings.jellyfinUrl, state.settings.jellyfinToken, videoSession);
            videoSession.clear();
        }
    };
    Uint32 overlayUntil = 0; // video overlay (title + progress) visible until this tick
    auto photoLoader = std::make_unique<PhotoLoader>(renderer, Config::cacheDir());
    int photoIndex = 0;
    bool slideshow = false;
    Uint32 nextSlideAt = 0;

    std::vector<Podcasts::Show> shows;
    std::future<std::vector<Podcasts::Show>> podcastTask;
    int showIndex = 0;
    auto publishPodcasts = [&]() {
        std::vector<WebServer::PodcastInfo> info;
        for (const auto &url: state.settings.podcastFeeds) {
            WebServer::PodcastInfo p{url, url, "", 0};
            for (const auto &sh: shows) {
                if (sh.feedUrl != url) continue;
                p.title = sh.title;
                p.error = sh.error;
                p.episodes = static_cast<int>(sh.episodes.size());
            }
            info.push_back(p);
        }
        web.setPodcasts(info);
    };
    auto refreshPodcasts = [&]() {
        if (podcastTask.valid()) return;
        std::vector<std::string> feeds = state.settings.podcastFeeds;
        std::string cache = Config::cacheDir();
        podcastTask = std::async(std::launch::async, [feeds, cache]() { return Podcasts::load(feeds, cache); });
    };
    publishPodcasts();
    if (!state.settings.podcastFeeds.empty()) refreshPodcasts();

    auto icons = std::make_unique<MenuIcons>(renderer, assets.string());
    MenuPreview preview;
    int coverIndex = -1;       // library song whose cover the Music preview shows
    Uint32 coverChangedAt = 0;

    // Album art for the Music entry: the playing song's cover, otherwise cycle through library covers.
    auto musicCover = [&]() -> std::shared_ptr<SDL_Texture> {
        const Song *playing = player ? player->current() : nullptr;
        if (playing && playing->artwork) return playing->artwork;
        if (songs.empty()) return nullptr;

        bool due = coverIndex < 0 || SDL_GetTicks() - coverChangedAt > 4000;
        if (due) {
            int n = static_cast<int>(songs.size());
            for (int step = 1; step <= n; step++) {
                int i = (std::max(coverIndex, -1) + step) % n;
                if (songs[i].artworkPath.empty() && !songs[i].artwork) continue;
                Library::ensureArtwork(renderer, songs[i]);
                if (songs[i].artwork) {
                    coverIndex = i;
                    break;
                }
            }
            coverChangedAt = SDL_GetTicks();
        }
        if (coverIndex >= 0 && coverIndex < static_cast<int>(songs.size())) return songs[coverIndex].artwork;
        return nullptr;
    };
    auto iconTexture = [&](MenuIcon icon) {
        // Icons are owned by MenuIcons; wrap without a deleter.
        return std::shared_ptr<SDL_Texture>(icons->get(icon), [](SDL_Texture *) {});
    };


    // Background Jellyfin work (login and/or library fetch).
    std::future<JellyfinLoad> jellyfinTask;
    auto startJellyfinFetch = [&](const std::string &url, const std::string &user, const std::string &pass) {
        std::string token = state.settings.jellyfinToken, userId = state.settings.jellyfinUserId;
        jellyfinTask = std::async(std::launch::async, [=]() {
            JellyfinLoad load;
            std::string tok = token, uid = userId;
            if (!pass.empty() || tok.empty()) {
                load.auth = Jellyfin::authenticate(url, user, pass);
                if (!load.auth.ok) {
                    load.error = load.auth.error;
                    return load;
                }
                tok = load.auth.token;
                uid = load.auth.userId;
            }
            load.songs = Jellyfin::fetchSongs(url, tok, uid, &load.error);
            std::string videoError;
            load.videos = Jellyfin::fetchVideos(url, tok, uid, &videoError);
            return load;
        });
    };
    if (!state.settings.jellyfinToken.empty())
        startJellyfinFetch(state.settings.jellyfinUrl, state.settings.jellyfinUser, "");

    int confirmItem = -1; // settings row waiting for a confirming second press
    std::string pendingPowerCommand; // restart / shut down, run once the logo screen is drawn
    bool poweringDown = false;       // keep showing the logo until systemd stops us

    // Software update runs in the background; see runUpdater().
    std::future<std::string> updateTask;
    std::atomic<int> updateProgress{0}; // 0-100 while installing
    state.updateDetail = PIPOD_VERSION;
    auto startUpdate = [&](const std::string &action) {
        if (updateTask.valid()) return;
        state.updateState = action == "install" ? AppState::UpdateState::Installing : AppState::UpdateState::Checking;
        state.updateDetail = "\xE2\x80\xA6"; // "…"; the status pane explains what's happening
        updateProgress = 0;
        updateTask = std::async(std::launch::async, [action, &updateProgress]() {
            return runUpdater(action, &updateProgress);
        });
    };
    auto saveSettings = [&]() { Config::save(state.settings); };

    // First-boot Setup Assistant (also from Settings -> Setup Assistant).
    SetupWizard wizard(state.settings, SetupWizard::Callbacks{
        [&](const std::string &code) { I18n::load(assets.string(), code); },
        [&](const std::string &url, const std::string &user, const std::string &pass) {
            state.settings.jellyfinUrl = url;
            state.settings.jellyfinUser = user;
            state.settings.jellyfinToken.clear();
            if (jellyfinTask.valid()) jellyfinTask.wait();
            startJellyfinFetch(url, user, pass);
            setStatus(state, tr("Connecting to Jellyfin..."), 0);
        },
        [&]() {
            saveSettings();
            web.setDeviceName(state.settings.deviceName);
            state.history.clear();
            state.current = Screen::MainMenu;
            state.title = "PiPod";
            state.selected = 0;
            state.scrollTop = 0;
        },
    });
    if ((state.deviceMode && !state.settings.setupDone) || opts.setup) wizard.start();
    auto changeVolume = [&](int delta) {
        state.settings.volume = std::clamp(state.settings.volume + delta, 0, 100);
        player->setVolume(state.settings.volume);
        video.setVolume(state.settings.volume);
        saveSettings();
        setStatus(state, tr("Volume") + " " + std::to_string(state.settings.volume) + "%", 1000);
    };

    auto listSize = [&]() -> int {
        switch (state.current) {
            case Screen::MainMenu: return static_cast<int>(mainMenu.size());
            case Screen::Music: return static_cast<int>(songs.size());
            case Screen::Videos: return static_cast<int>(videos.size());
            case Screen::Photos: return static_cast<int>(photos.size());
            case Screen::Podcasts: return static_cast<int>(shows.size());
            case Screen::PodcastEpisodes:
                return showIndex < static_cast<int>(shows.size()) ? static_cast<int>(shows[showIndex].episodes.size()) : 0;
            case Screen::Settings: return settingsItemCount(state);
            case Screen::Language: return static_cast<int>(I18n::languages().size());
            case Screen::About: return aboutRowCount(state);
            default: return 0;
        }
    };

    auto adjustSetting = [&](int dir) {
        Settings &s = state.settings;
        switch (static_cast<SettingsItem>(state.selected)) {
            case SettingsItem::Volume:
                changeVolume(dir * 10);
                return;
            case SettingsItem::Shuffle:
                s.shuffle = !s.shuffle;
                break;
            case SettingsItem::Repeat:
                s.repeat = static_cast<RepeatMode>((static_cast<int>(s.repeat) + (dir > 0 ? 1 : 2)) % 3);
                player->setRepeat(s.repeat);
                break;
            case SettingsItem::FlipScreen:
                s.screenFlipped = !s.screenFlipped;
                applyOrientation(s.screenFlipped);
                break;
            default:
                return;
        }
        saveSettings();
    };

    auto beginInput = [&]() {
        state.inputMode = SettingsInputMode::JellyfinUrl;
        state.inputUrl = state.settings.jellyfinUrl.empty() ? "http://" : state.settings.jellyfinUrl;
        state.inputUser = state.settings.jellyfinUser;
        state.inputPass.clear();
        SDL_StartTextInput();
    };
    auto endInput = [&]() {
        state.inputMode = SettingsInputMode::None;
        SDL_StopTextInput();
    };
    SDL_StopTextInput(); // some platforms enable text input by default

    auto activateSetting = [&]() {
        Settings &s = state.settings;
        auto item = static_cast<SettingsItem>(state.selected);
        // Destructive items need a second press; moving to another item cancels.
        bool destructive = item == SettingsItem::Reset || item == SettingsItem::Restart || item == SettingsItem::ShutDown;
        if (destructive && confirmItem != state.selected) {
            confirmItem = state.selected;
            setStatus(state, tr("Press Select again to confirm"), 3000);
            return;
        }
        confirmItem = -1;
        switch (item) {
            case SettingsItem::Volume:
            case SettingsItem::Shuffle:
            case SettingsItem::Repeat:
            case SettingsItem::FlipScreen:
                adjustSetting(1);
                break;
            case SettingsItem::Jellyfin:
                if (s.jellyfinToken.empty()) {
                    beginInput();
                } else {
                    s.jellyfinToken.clear();
                    s.jellyfinUserId.clear();
                    saveSettings();
                    remoteSongs.clear();
                    rebuildLibrary();
                    jellyfinVideos.clear();
                    rebuildVideos();
                    setStatus(state, tr("Jellyfin disconnected"));
                }
                break;
            case SettingsItem::Language: {
                state.navigate(Screen::Language, "Language");
                const auto &langs = I18n::languages();
                for (size_t i = 0; i < langs.size(); i++)
                    if (langs[i].code == I18n::current()) state.selected = static_cast<int>(i);
                break;
            }
            case SettingsItem::SetupAssistant:
                wizard.start();
                break;
            case SettingsItem::AddMedia:
                state.navigate(Screen::AddMedia, "Add Media");
                break;
            case SettingsItem::RescanLibrary:
                localSongs = Library::scanLocal(Library::defaultMusicDirs());
                rebuildLibrary();
                if (!s.jellyfinToken.empty() && !jellyfinTask.valid())
                    startJellyfinFetch(s.jellyfinUrl, s.jellyfinUser, "");
                setStatus(state, trf("{} local songs found", {std::to_string(localSongs.size())}));
                break;
            case SettingsItem::Update:
                if (state.updateState == AppState::UpdateState::Available) startUpdate("install");
                else startUpdate("check");
                break;
            case SettingsItem::Restart:
            case SettingsItem::ShutDown:
                // Run after the next frame so the logo screen is visible while the system goes down.
                saveSettings();
                player->stop();
                pendingPowerCommand = item == SettingsItem::ShutDown ? "sudo -n /usr/bin/systemctl poweroff"
                                                                     : "sudo -n /usr/bin/systemctl reboot";
                break;
            case SettingsItem::Reset:
                {
                    player->stop();
                    Config::reset();
                    state.settings = Settings{};
                    player->setVolume(state.settings.volume);
                    player->setRepeat(state.settings.repeat);
                    remoteSongs.clear();
                    rebuildLibrary();
                    setStatus(state, tr("PiPod OS has been reset"));
                }
                break;
            default: break;
        }
    };

    auto select = [&]() {
        switch (state.current) {
            case Screen::MainMenu: {
                const MenuItem &item = mainMenu[state.selected];
                if (item.action == MenuAction::ShuffleSongs) {
                    if (songs.empty()) {
                        setStatus(state, tr("No songs"));
                        break;
                    }
                    static std::mt19937 rng{std::random_device{}()};
                    int start = std::uniform_int_distribution<int>(0, static_cast<int>(songs.size()) - 1)(rng);
                    player->playQueue(songs, start, true);
                }
                state.navigate(item.next, item.label);
                break;
            }
            case Screen::Music:
                if (songs.empty()) break;
                player->playQueue(songs, state.selected, state.settings.shuffle);
                state.navigate(Screen::NowPlaying, "Now Playing");
                break;
            case Screen::NowPlaying:
                player->togglePause();
                break;
            case Screen::Videos:
                if (state.selected < static_cast<int>(videos.size())) {
                    const VideoItem &v = videos[state.selected];
                    player->stop(); // video audio replaces music
                    closeVideo();
                    video.setVolume(state.settings.volume);
                    if (v.jellyfinId.empty()) {
                        video.open(v.path, renderer);
                    } else {
                        videoSession = Net::hashName(v.jellyfinId + std::to_string(SDL_GetTicks()));
                        video.open(Jellyfin::videoStreamUrl(state.settings.jellyfinUrl, state.settings.jellyfinToken,
                                                         v.jellyfinId, videoSession),
                                   renderer, Jellyfin::authorizationHeader(state.settings.jellyfinToken) + "\r\n");
                    }
                    videoTitle = v.title;
                    overlayUntil = SDL_GetTicks() + 3000;
                    state.navigate(Screen::VideoPlayback, v.title);
                }
                break;
            case Screen::Photos:
                if (state.selected < static_cast<int>(photos.size())) {
                    photoIndex = state.selected;
                    slideshow = false;
                    state.navigate(Screen::PhotoViewer, "Photos");
                }
                break;
            case Screen::Podcasts:
                if (state.selected < static_cast<int>(shows.size())) {
                    showIndex = state.selected;
                    state.navigate(Screen::PodcastEpisodes, shows[showIndex].title);
                }
                break;
            case Screen::PodcastEpisodes:
                if (showIndex < static_cast<int>(shows.size()) && !shows[showIndex].episodes.empty()) {
                    // Queue the whole show from the chosen episode; episodes download when played.
                    std::vector<Song> queue;
                    for (const auto &ep: shows[showIndex].episodes)
                        queue.push_back(Podcasts::toSong(shows[showIndex], ep, Config::cacheDir()));
                    player->playQueue(queue, state.selected, false);
                    state.navigate(Screen::NowPlaying, "Now Playing");
                }
                break;
            case Screen::Settings:
                activateSetting();
                break;
            case Screen::Language: {
                const auto &langs = I18n::languages();
                if (state.selected >= 0 && state.selected < static_cast<int>(langs.size())) {
                    state.settings.language = langs[state.selected].code;
                    I18n::load(assets.string(), state.settings.language);
                    saveSettings();
                }
                state.back();
                break;
            }
            default: break;
        }
    };

    auto inputTarget = [&]() -> std::string & {
        if (state.inputMode == SettingsInputMode::JellyfinUrl) return state.inputUrl;
        if (state.inputMode == SettingsInputMode::JellyfinUser) return state.inputUser;
        return state.inputPass;
    };

    auto handleInputKey = [&](SDL_Keycode key) {
        std::string *target = &inputTarget();
        switch (key) {
            case SDLK_ESCAPE:
                endInput();
                break;
            case SDLK_BACKSPACE:
                // Remove one UTF-8 code point
                while (!target->empty()) {
                    unsigned char c = static_cast<unsigned char>(target->back());
                    target->pop_back();
                    if ((c & 0xC0) != 0x80) break;
                }
                break;
            case SDLK_v:
                if (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) {
                    if (char *clip = SDL_GetClipboardText()) {
                        target->append(clip);
                        SDL_free(clip);
                    }
                }
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (state.inputMode == SettingsInputMode::JellyfinUrl) {
                    state.inputMode = SettingsInputMode::JellyfinUser;
                } else if (state.inputMode == SettingsInputMode::JellyfinUser) {
                    state.inputMode = SettingsInputMode::JellyfinPass;
                } else {
                    endInput();
                    state.settings.jellyfinUrl = state.inputUrl;
                    state.settings.jellyfinUser = state.inputUser;
                    state.settings.jellyfinToken.clear();
                    if (jellyfinTask.valid()) jellyfinTask.wait();
                    startJellyfinFetch(state.inputUrl, state.inputUser, state.inputPass);
                    state.inputPass.clear();
                    setStatus(state, tr("Connecting to Jellyfin..."), 0);
                }
                break;
            default: break;
        }
    };

    // Status shown next to the settings list; refreshed while the page is open.
    DeviceInfo deviceInfo;
    Uint32 deviceInfoAt = 0;
    auto updateDeviceInfo = [&]() {
        if (deviceInfoAt != 0 && SDL_GetTicks() - deviceInfoAt < 3000) return;
        refreshDeviceInfo(deviceInfo, static_cast<int>(localSongs.size()), static_cast<int>(remoteSongs.size()));
        deviceInfo.webUrl = webAddressByName(web);
        deviceInfoAt = SDL_GetTicks();
    };

    bool takeScreenshot = false;
    bool takeRawScreenshot = false; // debug: capture the window after rotation
#ifndef NDEBUG
    std::vector<std::string> script = scriptedKeys();
    size_t scriptPos = 0;
    Uint32 nextScripted = SDL_GetTicks() + 800;
#endif

    // Keys for the full-screen video player, the photo grid and the photo viewer.
    // Returns true if the key was handled.
    auto handleMediaKey = [&](SDL_Keycode key) -> bool {
        Uint32 now = SDL_GetTicks();
        if (state.current == Screen::VideoPlayback) {
            overlayUntil = now + 3000;
            switch (key) {
                case SDLK_SPACE:
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_AUDIOPLAY:
                    video.togglePause();
                    return true;
                case SDLK_LEFT: video.seekBy(-10); return true;
                case SDLK_RIGHT: video.seekBy(10); return true;
                case SDLK_UP: changeVolume(5); return true;
                case SDLK_DOWN: changeVolume(-5); return true;
                case SDLK_BACKSPACE:
                case SDLK_ESCAPE:
                    closeVideo();
                    state.back();
                    return true;
                default: return false;
            }
        }
        if (state.current == Screen::Photos && !photos.empty()) {
            int n = static_cast<int>(photos.size());
            switch (key) {
                case SDLK_UP: state.selected = std::max(0, state.selected - PHOTO_COLUMNS); return true;
                case SDLK_DOWN: state.selected = std::min(n - 1, state.selected + PHOTO_COLUMNS); return true;
                case SDLK_LEFT: state.selected = std::max(0, state.selected - 1); return true;
                case SDLK_RIGHT: state.selected = std::min(n - 1, state.selected + 1); return true;
                default: return false;
            }
        }
        if (state.current == Screen::PhotoViewer) {
            int n = static_cast<int>(photos.size());
            switch (key) {
                case SDLK_LEFT:
                    slideshow = false;
                    if (n) photoIndex = (photoIndex - 1 + n) % n;
                    return true;
                case SDLK_RIGHT:
                    slideshow = false;
                    if (n) photoIndex = (photoIndex + 1) % n;
                    return true;
                case SDLK_SPACE:
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    slideshow = !slideshow;
                    nextSlideAt = now + 4000;
                    return true;
                case SDLK_BACKSPACE:
                case SDLK_ESCAPE:
                    slideshow = false;
                    state.back();
                    state.selected = photoIndex; // return to the photo we were looking at
                    return true;
                default: return false;
            }
        }
        return false;
    };

    // ------------------ MAIN LOOP ------------------
    bool running = true;
    bool restartRequested = false; // quit after an update so systemd starts the new version
    Uint32 restartAt = 0;
    Uint32 lastBatteryCheck = SDL_GetTicks();
    while (running) {
        Uint32 frameStart = SDL_GetTicks();

#ifndef NDEBUG
        if (scriptPos < script.size() && SDL_GetTicks() >= nextScripted) {
            const std::string &k = script[scriptPos++];
            nextScripted = SDL_GetTicks() + 400;
            if (k == "shot") {
                takeScreenshot = true;
            } else if (k == "rawshot") {
                takeRawScreenshot = true; // the final (rotated) window contents
            } else if (k == "quit") {
                SDL_Event q{};
                q.type = SDL_QUIT;
                SDL_PushEvent(&q);
            } else if (k != "wait") {
                SDL_Event ev{};
                ev.type = SDL_KEYDOWN;
                ev.key.windowID = SDL_GetWindowID(window);
                ev.key.keysym.sym = SDL_GetKeyFromName(k.c_str());
                SDL_PushEvent(&ev);
                // Synthetic SDL_TEXTINPUT events crash sdl2-compat, so type letters directly.
                if (k.size() == 1 && state.inputMode != SettingsInputMode::None) inputTarget() += k;
                if (k.size() == 1 && wizard.active()) wizard.handleText(k.c_str());
            }
        }
#endif

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }

            if (wizard.active()) {
                if (e.type == SDL_TEXTINPUT) {
                    wizard.handleText(e.text.text);
                } else if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_F12) takeScreenshot = true;
                    else wizard.handleKey(e.key.keysym.sym);
                }
                continue;
            }

            if (state.inputMode != SettingsInputMode::None) {
                if (e.type == SDL_TEXTINPUT) {
                    inputTarget().append(e.text.text);
                } else if (e.type == SDL_KEYDOWN) {
                    handleInputKey(e.key.keysym.sym);
                }
                continue;
            }

            if (e.type != SDL_KEYDOWN) continue;
            // No input while updating or going down.
            if (state.updateState == AppState::UpdateState::Installing || restartRequested || poweringDown) continue;
            if (e.key.keysym.sym == SDLK_F12) {
                takeScreenshot = true;
                continue;
            }
            if (handleMediaKey(e.key.keysym.sym)) continue;
            int count = listSize();
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    if (!state.deviceMode) running = false; // the OS shell can't be quit
                    break;
                case SDLK_UP:
                    if (count > 0) state.selected = std::max(0, state.selected - 1);
                    confirmItem = -1;
                    break;
                case SDLK_DOWN:
                    if (count > 0) state.selected = std::min(count - 1, state.selected + 1);
                    confirmItem = -1;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    select();
                    break;
                case SDLK_RIGHT:
                    if (state.current == Screen::NowPlaying) player->next();
                    else if (state.current == Screen::Settings) adjustSetting(1);
                    else select();
                    break;
                case SDLK_LEFT:
                    if (state.current == Screen::NowPlaying) player->previous();
                    else if (state.current == Screen::Settings) adjustSetting(-1);
                    else state.back();
                    break;
                case SDLK_BACKSPACE:
                    state.back();
                    confirmItem = -1;
                    break;
                case SDLK_SPACE:
                case SDLK_AUDIOPLAY:
                    player->togglePause();
                    break;
                case SDLK_AUDIONEXT:
                    player->next();
                    break;
                case SDLK_AUDIOPREV:
                    player->previous();
                    break;
                case SDLK_EQUALS:
                case SDLK_PLUS:
                case SDLK_KP_PLUS:
                    changeVolume(5);
                    break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                    changeVolume(-5);
                    break;
                default: break;
            }
            // Keep the selection valid if the list shrank (e.g. library reload)
            count = listSize();
            state.selected = count > 0 ? std::clamp(state.selected, 0, count - 1) : 0;
        }

        // ------------------ BACKGROUND WORK ------------------
        if (jellyfinTask.valid() && jellyfinTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            JellyfinLoad load = jellyfinTask.get();
            if (load.auth.ok) {
                state.settings.jellyfinToken = load.auth.token;
                state.settings.jellyfinUserId = load.auth.userId;
                saveSettings();
            }
            player->setRemote(state.settings.jellyfinUrl, state.settings.jellyfinToken, Config::cacheDir());
            if (load.error == "HTTP 401" && !load.auth.ok) {
                // Saved token was revoked (e.g. the server reset); the user has to log in again.
                state.settings.jellyfinToken.clear();
                state.settings.jellyfinUserId.clear();
                saveSettings();
                setStatus(state, tr("Jellyfin login expired - reconnect in Settings"), 4000);
            } else if (!load.error.empty() && load.songs.empty()) {
                setStatus(state, "Jellyfin: " + load.error, 4000);
            } else {
                remoteSongs = std::move(load.songs);
                rebuildLibrary();
                jellyfinVideos = std::move(load.videos);
                rebuildVideos();
                setStatus(state, "Jellyfin: " + trf("{} songs", {std::to_string(remoteSongs.size())}));
            }
        }
        if (updateTask.valid() && updateTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto [status, detail] = splitStatus(updateTask.get());
            if (status == "available") {
                state.updateState = AppState::UpdateState::Available;
                state.updateVersion = detail;
                state.updateDetail = "New";
                setStatus(state, trf("Update {} available", {detail}));
            } else if (status == "uptodate") {
                state.updateState = AppState::UpdateState::UpToDate;
                state.updateDetail = PIPOD_VERSION;
                setStatus(state, tr("PiPod OS is up to date"));
            } else if (status == "installed") {
                // systemd restarts the shell, which then runs the new version.
                setStatus(state, trf("Updated to {}, restarting...", {detail}), 0);
                saveSettings();
                restartRequested = true;
                restartAt = SDL_GetTicks() + 1500;
            } else {
                state.updateState = AppState::UpdateState::Failed;
                state.updateDetail = PIPOD_VERSION;
                setStatus(state, detail.empty() ? tr("Update failed") : detail, 4000);
            }
        }
        if (restartRequested && SDL_GetTicks() > restartAt) running = false;
        if (state.statusUntil && SDL_GetTicks() > state.statusUntil) {
            state.statusMessage.clear();
            state.statusUntil = 0;
        }
        if (SDL_GetTicks() - lastBatteryCheck > 10000) {
            ui.battery = readBatteryPercent();
            lastBatteryCheck = SDL_GetTicks();
        }

        player->update(renderer);
        wizard.update();
        if (int changes = web.takeChanges()) {
            if (changes & WebServer::MusicChanged) {
                localSongs = Library::scanLocal(Library::defaultMusicDirs());
                rebuildLibrary();
            }
            if (changes & WebServer::VideosChanged) {
                localVideos = Media::scanVideos();
                rebuildVideos();
            }
            if (changes & WebServer::PhotosChanged) {
                photos = Media::scanPhotos();
                photoIndex = std::min(photoIndex, std::max(0, static_cast<int>(photos.size()) - 1));
                if (state.current == Screen::PhotoViewer && photos.empty()) state.back();
            }
            deviceInfoAt = 0;
            setStatus(state, tr("Library updated"));
        }
        for (const auto &cmd: web.takePodcastCommands()) {
            auto &feeds = state.settings.podcastFeeds;
            auto it = std::find(feeds.begin(), feeds.end(), cmd.url);
            if (cmd.add && it == feeds.end()) feeds.push_back(cmd.url);
            if (!cmd.add && it != feeds.end()) feeds.erase(it);
            saveSettings();
            publishPodcasts();
            if (podcastTask.valid()) podcastTask.wait();
            refreshPodcasts();
            setStatus(state, tr(cmd.add ? "Podcast added" : "Podcast removed"));
        }
        if (podcastTask.valid() && podcastTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            shows = podcastTask.get();
            publishPodcasts();
            if (state.current == Screen::PodcastEpisodes && showIndex >= static_cast<int>(shows.size())) state.back();
        }
        photoLoader->update();
        if (state.current == Screen::PhotoViewer && slideshow && SDL_GetTicks() >= nextSlideAt && !photos.empty()) {
            photoIndex = (photoIndex + 1) % static_cast<int>(photos.size());
            nextSlideAt = SDL_GetTicks() + 4000;
        }
        if (state.current == Screen::VideoPlayback && video.isOpen() && video.finished() && video.error().empty()) {
            closeVideo();
            state.back();
        }
        ui.play = !player->isActive() ? PlayIndicator::None
                  : player->isPaused() ? PlayIndicator::Paused
                  : PlayIndicator::Playing;

        // Lazily load artwork for songs on screen
        if (state.current == Screen::Music) {
            int last = std::min(static_cast<int>(songs.size()), state.scrollTop + 10);
            for (int i = state.scrollTop; i < last; i++) Library::ensureArtwork(renderer, songs[i]);
        }

        // ------------------ DRAW ------------------
        beginFrame();
        SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
        SDL_RenderClear(renderer);

        if (wizard.active()) wizard.draw(ui, logo);
        else switch (state.current) {
            case Screen::Videos:
                drawVideosPage(ui, state, videos);
                break;
            case Screen::VideoPlayback:
                drawVideoPlayback(ui, video, videoTitle, video.paused() || SDL_GetTicks() < overlayUntil);
                break;
            case Screen::Photos:
                drawPhotosGrid(ui, state, photos, *photoLoader);
                break;
            case Screen::PhotoViewer:
                drawPhotoViewer(ui, photos, photoIndex, *photoLoader, slideshow);
                break;
            case Screen::Podcasts:
                drawPodcastsPage(ui, state, shows, podcastTask.valid(), *photoLoader);
                break;
            case Screen::PodcastEpisodes:
                if (showIndex < static_cast<int>(shows.size())) drawEpisodesPage(ui, state, shows[showIndex]);
                break;
            case Screen::AddMedia:
                drawAddMediaPage(ui, web.running(), webAddressByName(web), webAddressByIp(web), web.accessCode());
                break;
            case Screen::Language: {
                drawTopBar(ui, tr("Language"));
                std::vector<ListRow> rows;
                for (const auto &l: I18n::languages())
                    rows.push_back({l.nativeName, l.code == I18n::current() ? tr("Selected") : "", false, true});
                drawList(ui, state, rows, 0, ui.w, 23);
                break;
            }
            case Screen::MainMenu: {
                const MenuItem &item = mainMenu[state.selected];
                std::shared_ptr<SDL_Texture> art;
                if (item.icon == MenuIcon::Music) art = musicCover();
                else if (item.icon == MenuIcon::NowPlaying && player->current()) art = player->current()->artwork;
                if (art) preview.set(art, true);
                else preview.set(iconTexture(item.icon), false);
                // Like the iPod: say so when a section is empty.
                preview.caption.clear();
                if (!art) {
                    if (item.icon == MenuIcon::Music && songs.empty()) preview.caption = tr("No Music");
                    else if (item.icon == MenuIcon::Videos && videos.empty()) preview.caption = tr("No Videos");
                    else if (item.icon == MenuIcon::Photos && photos.empty()) preview.caption = tr("No Photos");
                    else if (item.icon == MenuIcon::Podcasts && shows.empty()) preview.caption = tr("No Podcasts");
                }
                drawMenu(ui, state, mainMenu, preview);
                break;
            }
            case Screen::Music:
                drawSongsPage(ui, state, songs, jellyfinTask.valid());
                break;
            case Screen::NowPlaying:
                drawNowPlaying(ui, *player, state.settings, *icons);
                break;
            case Screen::Settings:
                updateDeviceInfo();
                drawSettingsPage(ui, state, deviceInfo);
                break;
            case Screen::About:
                updateDeviceInfo();
                drawAboutPage(ui, state, deviceInfo, state.settings.deviceName);
                break;
            case Screen::ComingSoon:
                drawComingSoonPage(ui, state.title);
                break;
        }
        // Full-screen logo: updating (with progress), restarting / shutting down, or fading out
        // the startup splash.
        bool installing = state.updateState == AppState::UpdateState::Installing || restartRequested;
        if (installing) {
            drawSplash(ui, logo, restartRequested ? 1.0f : updateProgress / 100.0f);
        } else if (!pendingPowerCommand.empty() || poweringDown) {
            drawSplash(ui, logo);
        } else {
            drawStatus(ui, state.statusMessage);
            constexpr Uint32 SPLASH_HOLD = 900, SPLASH_FADE = 400;
            Uint32 since = SDL_GetTicks() - appStart;
            if (since < SPLASH_HOLD + SPLASH_FADE) {
                Uint8 a = since < SPLASH_HOLD ? 255 : Uint8(255 - 255 * (since - SPLASH_HOLD) / SPLASH_FADE);
                drawSplash(ui, logo, -1.0f, a);
            }
        }

        if (takeScreenshot) {
            takeScreenshot = false;
            std::string path = saveScreenshot(renderer, canvas, screenW, screenH);
            SDL_Log("Screenshot: %s", path.empty() ? SDL_GetError() : path.c_str());
        }
        endFrame();
        if (takeRawScreenshot && canvas) {
            takeRawScreenshot = false;
            std::string path = saveScreenshot(renderer, nullptr, 0, 0);
            SDL_Log("Screenshot: %s", path.c_str());
        }
        SDL_RenderPresent(renderer);

        if (!pendingPowerCommand.empty()) {
            int rc = std::system(pendingPowerCommand.c_str());
            pendingPowerCommand.clear();
            if (rc != 0) setStatus(state, tr("Not permitted"), 3000);
            else poweringDown = true;
        }

        // Cap at ~60 fps in case vsync isn't available.
        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
    }

    // ------------------ CLEANUP ------------------
    if (jellyfinTask.valid()) jellyfinTask.wait();
    if (updateTask.valid()) updateTask.wait();
    web.stop();
    closeVideo();
    if (podcastTask.valid()) podcastTask.wait();
    photoLoader.reset();
    preview.reset();
    icons.reset();
    player.reset();
    songs.clear();
    localSongs.clear();
    remoteSongs.clear();
    if (arrow) SDL_DestroyTexture(arrow);
    if (canvas) SDL_DestroyTexture(canvas);
    if (logo) SDL_DestroyTexture(logo);
    TTF_CloseFont(font);
    TTF_CloseFont(smallFont);
    TTF_CloseFont(mediumFont);
    TTF_CloseFont(boldFont);
    TTF_CloseFont(listFont);
    TTF_CloseFont(titleFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    Mix_CloseAudio();
    Mix_Quit();
    curl_global_cleanup();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
