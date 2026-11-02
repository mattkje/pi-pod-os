#pragma once
#include <SDL2/SDL.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Plays a video file with FFmpeg: a decode thread fills a small queue of YUV frames and an audio
// ring buffer. Audio goes out through SDL_mixer's music hook (normal music must be stopped while a
// video plays) and is the master clock; video frames are shown when their time has come.
// On Linux the Pi's hardware H.264 decoder (h264_v4l2m2m) is tried first.
class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    // Starts playing a file or URL (HLS, http). httpHeaders: extra request headers, "Name: value\r\n" each.
    bool open(const std::string &source, SDL_Renderer *renderer, const std::string &httpHeaders = "");
    void close();
    bool isOpen() const { return thread_.joinable(); }

    // Call once per frame: uploads the frame that is due. Returns the texture to draw (may be null
    // before the first frame).
    SDL_Texture *update();

    void setVolume(int percent); // 0-100, applied to the video's audio
    void togglePause();
    bool paused() const { return paused_; }
    void seekBy(double seconds);

    double position() const; // seconds
    double duration() const { return duration_; }
    bool finished() const { return finished_; }
    const std::string &error() const { return error_; }
    int width() const { return width_; }
    int height() const { return height_; }
    const std::string &decoderName() const { return decoderName_; }

private:
    struct Frame {
        double pts = 0;
        int w = 0, h = 0;
        std::vector<uint8_t> y, u, v;
    };

    void decodeLoop();
    static void audioCallback(void *self, Uint8 *stream, int len);
    void fillAudio(Uint8 *stream, int len);

    std::string path_;
    std::string headers_;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    int width_ = 0, height_ = 0; // size of the current texture (frames are scaled to fit 640x480)

    std::thread thread_;
    std::atomic<bool> quit_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> decoderDone_{false};
    std::atomic<double> seekTarget_{-1};
    std::string error_;       // written by the decode thread before finished_ is set
    std::string decoderName_; // written before the first frame is queued
    std::atomic<double> duration_{0};
    std::atomic<bool> hasAudio_{false};

    // Video frame queue
    std::mutex frameMutex_;
    std::condition_variable frameCv_;
    std::deque<Frame> frames_;
    double shownPts_ = 0;

    // Audio ring (interleaved S16 at the mixer's rate/channels)
    std::mutex audioMutex_;
    std::condition_variable audioCv_;
    std::vector<int16_t> audio_;
    size_t audioRead_ = 0, audioSize_ = 0;
    std::atomic<double> audioClock_{0}; // pts of the next sample to play
    double audioEndPts_ = 0;            // pts just after the last buffered sample
    std::atomic<int> volume_{100};
    int mixRate_ = 44100, mixChannels_ = 2;

    // Clock used when there is no audio
    std::atomic<Uint32> wallStart_{0};
    std::atomic<double> wallBase_{0};
};
