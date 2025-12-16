#include "MusicPage.h"
#include "../Utils.h"
#include <filesystem>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <SDL2/SDL_image.h>

namespace fs = std::filesystem;

std::vector<Song> loadSongs(SDL_Renderer *renderer) {
    std::vector<Song> songs;
    fs::path musicDir = "assets/music";
    fs::path artworkDir = musicDir / "artwork";

    for (const auto &entry : fs::directory_iterator(musicDir)) {
        if (entry.path().extension() == ".mp3") {
            TagLib::FileRef f(entry.path().c_str());
            Song s;
            s.filePath = entry.path();
            if (!f.isNull() && f.tag()) {
                TagLib::Tag *tag = f.tag();
                s.title = tag->title().isEmpty() ? entry.path().stem().string() : tag->title().to8Bit(true);
                s.artist = tag->artist().to8Bit(true);
            } else {
                s.title = entry.path().stem().string();
                s.artist = "Unknown";
            }
            fs::path artPath = artworkDir / (entry.path().stem().string() + ".png");
            if (fs::exists(artPath)) s.artwork = IMG_LoadTexture(renderer, artPath.c_str());
            songs.push_back(s);
        }
    }
    return songs;
}

void drawSongsMenu(SDL_Renderer *r, TTF_Font *font, AppState &state, const std::vector<Song> &songs,
                   int winWidth, int winHeight) {
    int centerY = (winHeight + 32) / 2;
    state.visualOffset += (state.selected - state.visualOffset) * 0.15f;

    for (int i = 0; i < (int)songs.size(); ++i) {
        float y = centerY + (i - state.visualOffset) * 36;
        if (y < 32 - 36 || y > winHeight + 36) continue;

        bool selected = (i == state.selected);
        if (selected) drawHighlight(r, SDL_Rect{0, (int)y - 4, winWidth, 36});

        SDL_Texture *titleTex = renderText(r, font, songs[i].title, selected ? SDL_Color{255,255,255,255} : SDL_Color{40,40,40,255});
        int tw, th;
        SDL_QueryTexture(titleTex, nullptr, nullptr, &tw, &th);
        SDL_Rect titleRect{24, (int)y, tw, th};
        SDL_RenderCopy(r, titleTex, nullptr, &titleRect);
        SDL_DestroyTexture(titleTex);

        SDL_Texture *artistTex = renderText(r, font, songs[i].artist, {120,120,120,255});
        int aw, ah;
        SDL_QueryTexture(artistTex, nullptr, nullptr, &aw, &ah);
        SDL_Rect artistRect{24, (int)y + th, aw, ah};
        SDL_RenderCopy(r, artistTex, nullptr, &artistRect);
        SDL_DestroyTexture(artistTex);

        if (songs[i].artwork) {
            int artW, artH;
            SDL_QueryTexture(songs[i].artwork, nullptr, nullptr, &artW, &artH);
            float scale = 36.0f / artH * 0.8f;
            int w = (int)(artW * scale);
            int h = (int)(artH * scale);
            SDL_Rect artRect{winWidth - w - 8, (int)y + (36 - h)/2, w, h};
            SDL_RenderCopy(r, songs[i].artwork, nullptr, &artRect);
        }
    }
}

void drawMusicScreen(SDL_Renderer *r, TTF_Font *font, const Song *currentSong, int winWidth, int winHeight) {
    drawTopBar(r, font, "Now Playing", winWidth);
    if (!currentSong) return;

    SDL_Texture *titleTex = renderText(r, font, currentSong->title, {40,40,40,255});
    SDL_Texture *artistTex = renderText(r, font, currentSong->artist, {40,40,40,255});

    int tw, th, aw, ah;
    SDL_QueryTexture(titleTex, nullptr, nullptr, &tw, &th);
    SDL_QueryTexture(artistTex, nullptr, nullptr, &aw, &ah);

    SDL_Rect tdst{(winWidth - tw)/2, 70, tw, th};
    SDL_Rect adst{(winWidth - aw)/2, 100, aw, ah};

    SDL_RenderCopy(r, titleTex, nullptr, &tdst);
    SDL_RenderCopy(r, artistTex, nullptr, &adst);

    SDL_DestroyTexture(titleTex);
    SDL_DestroyTexture(artistTex);

    if (currentSong->artwork) {
        int artW, artH;
        SDL_QueryTexture(currentSong->artwork, nullptr, nullptr, &artW, &artH);
        float scale = std::min(winWidth*0.8f/artW, (winHeight-200)*0.8f/artH);
        int w = (int)(artW*scale);
        int h = (int)(artH*scale);
        SDL_Rect rect{(winWidth-w)/2, 140, w, h};
        SDL_RenderCopy(r, currentSong->artwork, nullptr, &rect);
    }
}
