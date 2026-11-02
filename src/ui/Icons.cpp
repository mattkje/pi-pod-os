#include "ui/Icons.h"
#include <SDL2/SDL_image.h>
#include <cmath>
#include <filesystem>
#include <ctime>
#include <vector>

namespace {
    constexpr float PI = 3.14159265f;

    // Small immediate-mode painter on top of SDL_RenderGeometry. Coordinates are in the
    // 256x256 icon canvas. erase() switches to punching transparent holes.
    struct Painter {
        SDL_Renderer *r;
        SDL_Color color{255, 255, 255, 255};

        void paint(SDL_Color c) {
            color = c;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        }

        void erase() {
            color = {0, 0, 0, 0};
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }

        SDL_Vertex v(float x, float y) const {
            return SDL_Vertex{{x, y}, color, {0, 0}};
        }

        // Convex polygon as a triangle fan.
        void poly(const std::vector<SDL_FPoint> &pts) {
            if (pts.size() < 3) return;
            std::vector<SDL_Vertex> verts;
            for (size_t i = 1; i + 1 < pts.size(); i++) {
                verts.push_back(v(pts[0].x, pts[0].y));
                verts.push_back(v(pts[i].x, pts[i].y));
                verts.push_back(v(pts[i + 1].x, pts[i + 1].y));
            }
            SDL_RenderGeometry(r, nullptr, verts.data(), static_cast<int>(verts.size()), nullptr, 0);
        }

        void circle(float cx, float cy, float rad) {
            std::vector<SDL_FPoint> pts;
            for (int i = 0; i < 64; i++) {
                float a = 2 * PI * i / 64;
                pts.push_back({cx + rad * std::cos(a), cy + rad * std::sin(a)});
            }
            poly(pts);
        }

        // Ring segment between angles a0..a1 (radians, 0 = right, clockwise on screen).
        void arc(float cx, float cy, float rOut, float rIn, float a0 = 0, float a1 = 2 * PI) {
            int steps = std::max(8, static_cast<int>(64 * (a1 - a0) / (2 * PI)));
            std::vector<SDL_Vertex> verts;
            for (int i = 0; i < steps; i++) {
                float t0 = a0 + (a1 - a0) * i / steps, t1 = a0 + (a1 - a0) * (i + 1) / steps;
                SDL_Vertex o0 = v(cx + rOut * std::cos(t0), cy + rOut * std::sin(t0));
                SDL_Vertex o1 = v(cx + rOut * std::cos(t1), cy + rOut * std::sin(t1));
                SDL_Vertex i0 = v(cx + rIn * std::cos(t0), cy + rIn * std::sin(t0));
                SDL_Vertex i1 = v(cx + rIn * std::cos(t1), cy + rIn * std::sin(t1));
                verts.insert(verts.end(), {o0, o1, i1, o0, i1, i0});
            }
            SDL_RenderGeometry(r, nullptr, verts.data(), static_cast<int>(verts.size()), nullptr, 0);
        }

        void line(float x1, float y1, float x2, float y2, float w, bool roundCaps = true) {
            float dx = x2 - x1, dy = y2 - y1;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0) return;
            float nx = -dy / len * w / 2, ny = dx / len * w / 2;
            poly({{x1 + nx, y1 + ny}, {x2 + nx, y2 + ny}, {x2 - nx, y2 - ny}, {x1 - nx, y1 - ny}});
            if (roundCaps) {
                circle(x1, y1, w / 2);
                circle(x2, y2, w / 2);
            }
        }

        void roundRect(float x, float y, float w, float h, float rad) {
            // Corners in clockwise screen order: top-left, top-right, bottom-right, bottom-left.
            const float cx[4] = {x + rad, x + w - rad, x + w - rad, x + rad};
            const float cy[4] = {y + rad, y + rad, y + h - rad, y + h - rad};
            std::vector<SDL_FPoint> pts;
            for (int c = 0; c < 4; c++) {
                float start = PI + c * PI / 2;
                for (int i = 0; i <= 8; i++) {
                    float a = start + (PI / 2) * i / 8.0f;
                    pts.push_back({cx[c] + rad * std::cos(a), cy[c] + rad * std::sin(a)});
                }
            }
            poly(pts);
        }

        void triangle(float x1, float y1, float x2, float y2, float x3, float y3) {
            poly({{x1, y1}, {x2, y2}, {x3, y3}});
        }
    };

    const SDL_Color WHITE{255, 255, 255, 255};

    void drawMusic(Painter &p) {
        // Two beamed eighth notes
        p.circle(78, 186, 30);
        p.circle(178, 166, 30);
        p.line(104, 186, 104, 64, 16, false);
        p.line(204, 166, 204, 44, 16, false);
        p.poly({{96, 56}, {212, 30}, {212, 66}, {96, 92}});
    }

    void drawVideos(Painter &p) {
        // Film strip
        p.roundRect(44, 28, 168, 200, 14);
        p.erase();
        for (int i = 0; i < 6; i++) {
            float y = 40 + i * 31;
            p.roundRect(54, y, 20, 18, 4);
            p.roundRect(182, y, 20, 18, 4);
        }
        p.roundRect(86, 40, 84, 82, 6);
        p.roundRect(86, 134, 84, 82, 6);
    }

    void drawPhotos(Painter &p) {
        // Framed landscape with sun
        p.roundRect(24, 48, 208, 160, 16);
        p.erase();
        p.roundRect(42, 66, 172, 124, 6);
        p.paint(WHITE);
        p.circle(170, 100, 16);
        p.triangle(46, 186, 106, 106, 166, 186);
        p.triangle(124, 186, 164, 136, 210, 186);
    }

    void drawPodcasts(Painter &p) {
        // Broadcast waves around a microphone head
        p.circle(128, 104, 22);
        float a0 = PI * 0.75f, a1 = PI * 2.25f; // open at the bottom
        p.arc(128, 104, 66, 50, a0, a1);
        p.arc(128, 104, 104, 88, a0, a1);
        p.poly({{114, 138}, {142, 138}, {136, 226}, {120, 226}});
    }

    void drawClock(Painter &p) {
        p.circle(128, 128, 104);
        p.paint({70, 70, 70, 255});
        for (int i = 0; i < 12; i++) {
            float a = 2 * PI * i / 12;
            float inner = (i % 3 == 0) ? 74 : 82;
            p.line(128 + inner * std::cos(a), 128 + inner * std::sin(a),
                   128 + 92 * std::cos(a), 128 + 92 * std::sin(a), i % 3 == 0 ? 8 : 5);
        }
        std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        float minuteA = 2 * PI * local.tm_min / 60 - PI / 2;
        float hourA = 2 * PI * ((local.tm_hour % 12) + local.tm_min / 60.0f) / 12 - PI / 2;
        p.paint({40, 40, 40, 255});
        p.line(128, 128, 128 + 52 * std::cos(hourA), 128 + 52 * std::sin(hourA), 12);
        p.line(128, 128, 128 + 80 * std::cos(minuteA), 128 + 80 * std::sin(minuteA), 8);
        p.paint({200, 40, 30, 255});
        p.circle(128, 128, 9);
    }

    void drawSettings(Painter &p) {
        // Gear
        constexpr int teeth = 9;
        for (int i = 0; i < teeth; i++) {
            float a = 2 * PI * i / teeth;
            float c = std::cos(a), s = std::sin(a);
            float tx = -s, ty = c; // tangent
            float rIn = 70, rOut = 112, wIn = 22, wOut = 16;
            p.poly({{128 + c * rIn + tx * wIn, 128 + s * rIn + ty * wIn},
                    {128 + c * rOut + tx * wOut, 128 + s * rOut + ty * wOut},
                    {128 + c * rOut - tx * wOut, 128 + s * rOut - ty * wOut},
                    {128 + c * rIn - tx * wIn, 128 + s * rIn - ty * wIn}});
        }
        p.circle(128, 128, 86);
        p.erase();
        p.circle(128, 128, 34);
    }

    void drawShuffle(Painter &p) {
        // Two crossing arrows
        p.line(36, 72, 92, 72, 18);
        p.line(92, 72, 164, 184, 18);
        p.line(164, 184, 194, 184, 18, false);
        p.triangle(192, 154, 232, 184, 192, 214);

        p.line(36, 184, 92, 184, 18);
        p.line(92, 184, 164, 72, 18);
        p.line(164, 72, 194, 72, 18, false);
        p.triangle(192, 42, 232, 72, 192, 102);
    }

    void drawNowPlaying(Painter &p) {
        p.arc(128, 128, 108, 92);
        p.triangle(100, 76, 186, 128, 100, 180);
    }

    void drawRepeat(Painter &p) {
        // Two arrows chasing each other around a rounded loop
        p.arc(84, 128, 56, 34, PI, 1.5f * PI);
        p.line(30 + 11, 128, 30 + 11, 150, 22, false);
        p.line(84, 83, 160, 83, 22, false);
        p.triangle(156, 48, 210, 83, 156, 118);

        p.arc(172, 128, 56, 34, 0, 0.5f * PI);
        p.line(226 - 11, 128, 226 - 11, 106, 22, false);
        p.line(172, 173, 96, 173, 22, false);
        p.triangle(100, 138, 46, 173, 100, 208);
    }

    void drawAbout(Painter &p) {
        p.arc(128, 128, 108, 92);
        p.circle(128, 74, 15);
        p.roundRect(114, 104, 28, 92, 8);
    }
}

const char *MenuIcons::fileName(MenuIcon icon) {
    switch (icon) {
        case MenuIcon::Music: return "music.png";
        case MenuIcon::Videos: return "videos.png";
        case MenuIcon::Photos: return "photos.png";
        case MenuIcon::Podcasts: return "podcasts.png";
        case MenuIcon::Extras: return "extras.png";
        case MenuIcon::Settings: return "settings.png";
        case MenuIcon::Shuffle: return "shuffle.png";
        case MenuIcon::NowPlaying: return "now-playing.png";
        case MenuIcon::About: return "about.png";
        case MenuIcon::Repeat: return "repeat.png";
        default: return "";
    }
}

MenuIcons::MenuIcons(SDL_Renderer *renderer, const std::string &assetsDir) : r(renderer) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(assetsDir) / "icons" / "menu";
    for (size_t i = 0; i < textures.size(); i++) {
        auto icon = static_cast<MenuIcon>(i);
        fs::path file = dir / fileName(icon);
        std::error_code ec;
        if (fs::exists(file, ec)) {
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
            textures[i] = IMG_LoadTexture(r, file.string().c_str());
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
            if (textures[i]) {
                fromFile[i] = true;
                continue;
            }
            SDL_Log("Failed to load %s: %s", file.c_str(), IMG_GetError());
        }
        textures[i] = createTarget();
        if (textures[i]) render(icon, textures[i]);
    }
}

int MenuIcons::exportBuiltIn(SDL_Renderer *renderer, const std::string &dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dir, ec);

    MenuIcons drawer(renderer, ""); // empty assets dir: everything is drawn
    int written = 0;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, SIZE, SIZE, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return 0;
    for (size_t i = 0; i < drawer.textures.size(); i++) {
        if (!drawer.textures[i]) continue;
        SDL_SetRenderTarget(renderer, drawer.textures[i]);
        bool ok = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, surf->pixels, surf->pitch) == 0;
        SDL_SetRenderTarget(renderer, nullptr);
        fs::path file = fs::path(dir) / fileName(static_cast<MenuIcon>(i));
        if (ok && IMG_SavePNG(surf, file.string().c_str()) == 0) {
            SDL_Log("Wrote %s", file.c_str());
            written++;
        }
    }
    SDL_FreeSurface(surf);
    return written;
}

MenuIcons::~MenuIcons() {
    for (SDL_Texture *t: textures)
        if (t) SDL_DestroyTexture(t);
}

SDL_Texture *MenuIcons::createTarget() {
    // Linear filtering is what makes the downscaled icons smooth.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, SIZE, SIZE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    if (!t) {
        SDL_Log("Icon texture failed: %s", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

void MenuIcons::render(MenuIcon icon, SDL_Texture *target) {
    SDL_Texture *prevTarget = SDL_GetRenderTarget(r);
    SDL_BlendMode prevBlend;
    SDL_GetRenderDrawBlendMode(r, &prevBlend);
    // Logical size scaling must not apply while drawing into the icon texture.
    int lw, lh;
    SDL_RenderGetLogicalSize(r, &lw, &lh);
    SDL_SetRenderTarget(r, target);
    SDL_RenderSetLogicalSize(r, 0, 0);
    SDL_RenderSetViewport(r, nullptr);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 0);
    SDL_RenderClear(r);

    Painter p{r};
    p.paint(WHITE);
    switch (icon) {
        case MenuIcon::Music: drawMusic(p); break;
        case MenuIcon::Videos: drawVideos(p); break;
        case MenuIcon::Photos: drawPhotos(p); break;
        case MenuIcon::Podcasts: drawPodcasts(p); break;
        case MenuIcon::Extras: drawClock(p); break;
        case MenuIcon::Settings: drawSettings(p); break;
        case MenuIcon::Shuffle: drawShuffle(p); break;
        case MenuIcon::NowPlaying: drawNowPlaying(p); break;
        case MenuIcon::About: drawAbout(p); break;
        case MenuIcon::Repeat: drawRepeat(p); break;
        default: break;
    }

    SDL_SetRenderTarget(r, prevTarget);
    if (lw > 0 && lh > 0) SDL_RenderSetLogicalSize(r, lw, lh);
    SDL_SetRenderDrawBlendMode(r, prevBlend);
}

SDL_Texture *MenuIcons::get(MenuIcon icon) {
    auto index = static_cast<size_t>(icon);
    if (index >= textures.size()) return nullptr;

    if (icon == MenuIcon::Extras && !fromFile[index]) {
        std::time_t now = std::time(nullptr);
        int minute = static_cast<int>(now / 60);
        if (minute != clockMinute && textures[index]) {
            clockMinute = minute;
            render(icon, textures[index]);
        }
    }
    return textures[index];
}
