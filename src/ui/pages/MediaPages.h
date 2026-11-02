#pragma once
#include <string>
#include <vector>
#include "app/AppState.h"
#include "media/MediaLibrary.h"
#include "media/Podcasts.h"
#include "ui/Utils.h"
#include "media/VideoPlayer.h"

// ---- Videos ----
void drawVideosPage(const UI &ui, AppState &state, const std::vector<VideoItem> &videos);
// Full screen video; the overlay (title + progress) shows while paused or briefly after input.
void drawVideoPlayback(const UI &ui, VideoPlayer &player, const std::string &title, bool overlay);

// ---- Photos ----
constexpr int PHOTO_COLUMNS = 4;
void drawPhotosGrid(const UI &ui, AppState &state, const std::vector<MediaFile> &photos, PhotoLoader &loader);
void drawPhotoViewer(const UI &ui, const std::vector<MediaFile> &photos, int index, PhotoLoader &loader,
                     bool slideshow);

// ---- Podcasts ----
void drawPodcastsPage(const UI &ui, AppState &state, const std::vector<Podcasts::Show> &shows, bool loading,
                      PhotoLoader &loader);
void drawEpisodesPage(const UI &ui, AppState &state, const Podcasts::Show &show);
