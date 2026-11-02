#include "media/VideoPlayer.h"
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace {
    constexpr int MAX_W = 640, MAX_H = 480; // decode output is scaled down to fit this
    constexpr size_t MAX_FRAMES = 6;

    // Opens a decoder for the stream, preferring hardware H.264 on the Pi.
    AVCodecContext *openDecoder(AVStream *st, bool tryHardware, std::string *name) {
        const AVCodec *codec = nullptr;
        AVCodecContext *ctx = nullptr;
#if defined(__linux__)
        if (tryHardware && st->codecpar->codec_id == AV_CODEC_ID_H264) {
            if (const AVCodec *hw = avcodec_find_decoder_by_name("h264_v4l2m2m")) {
                ctx = avcodec_alloc_context3(hw);
                if (ctx && avcodec_parameters_to_context(ctx, st->codecpar) >= 0 && avcodec_open2(ctx, hw, nullptr) >= 0) {
                    if (name) *name = hw->name;
                    return ctx;
                }
                avcodec_free_context(&ctx);
            }
        }
#else
        (void) tryHardware;
#endif
        codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!codec) return nullptr;
        ctx = avcodec_alloc_context3(codec);
        if (!ctx) return nullptr;
        if (avcodec_parameters_to_context(ctx, st->codecpar) < 0) {
            avcodec_free_context(&ctx);
            return nullptr;
        }
        ctx->thread_count = 0; // automatic
        if (avcodec_open2(ctx, codec, nullptr) < 0) {
            avcodec_free_context(&ctx);
            return nullptr;
        }
        if (name) *name = codec->name;
        return ctx;
    }
}

VideoPlayer::VideoPlayer() = default;

VideoPlayer::~VideoPlayer() {
    close();
}

bool VideoPlayer::open(const std::string &path, SDL_Renderer *renderer, const std::string &httpHeaders,
                       bool audioOnly) {
    close();
    path_ = path;
    headers_ = httpHeaders;
    audioOnly_ = audioOnly;
    renderer_ = renderer;
    error_.clear();
    quit_ = false;
    paused_ = false;
    finished_ = false;
    decoderDone_ = false;
    seekTarget_ = -1;
    frames_.clear();
    shownPts_ = 0;
    audioRead_ = audioSize_ = 0;
    audioClock_ = 0;
    duration_ = 0;
    hasAudio_ = false;

    Uint16 format;
    if (!Mix_QuerySpec(&mixRate_, &format, &mixChannels_)) {
        mixRate_ = 44100;
        mixChannels_ = 2;
    }
    audio_.assign(static_cast<size_t>(mixRate_) * mixChannels_ * 2, 0); // 2 s of audio

    wallStart_ = SDL_GetTicks();
    wallBase_ = 0;
    thread_ = std::thread([this]() { decodeLoop(); });
    Mix_HookMusic(&VideoPlayer::audioCallback, this);
    return true;
}

void VideoPlayer::close() {
    // Unhook first: after this returns the audio callback is no longer running.
    if (thread_.joinable()) Mix_HookMusic(nullptr, nullptr);
    quit_ = true;
    frameCv_.notify_all();
    audioCv_.notify_all();
    if (thread_.joinable()) thread_.join();
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    frames_.clear();
}

void VideoPlayer::audioCallback(void *self, Uint8 *stream, int len) {
    static_cast<VideoPlayer *>(self)->fillAudio(stream, len);
}

void VideoPlayer::setVolume(int percent) {
    volume_ = std::clamp(percent, 0, 100);
}

void VideoPlayer::fillAudio(Uint8 *stream, int len) {
    auto *out = reinterpret_cast<int16_t *>(stream);
    size_t want = static_cast<size_t>(len) / sizeof(int16_t);
    std::fill(out, out + want, 0);
    if (paused_) return;

    std::lock_guard<std::mutex> lock(audioMutex_);
    size_t n = std::min(want, audioSize_);
    int vol = volume_;
    for (size_t i = 0; i < n; i++) {
        out[i] = static_cast<int16_t>(audio_[audioRead_] * vol / 100);
        audioRead_ = (audioRead_ + 1) % audio_.size();
    }
    audioSize_ -= n;
    // Clock = time of the next sample still in the buffer.
    audioClock_ = audioEndPts_ - double(audioSize_) / (double(mixRate_) * mixChannels_);
    audioCv_.notify_all();
}

double VideoPlayer::position() const {
    if (hasAudio_) return std::max(0.0, audioClock_.load());
    if (paused_) return wallBase_;
    return wallBase_ + (SDL_GetTicks() - wallStart_) / 1000.0;
}

void VideoPlayer::togglePause() {
    if (!hasAudio_) {
        if (paused_) wallStart_ = SDL_GetTicks();
        else wallBase_ = position();
    }
    paused_ = !paused_;
}

void VideoPlayer::seekBy(double seconds) {
    double target = std::clamp(position() + seconds, 0.0, std::max(0.0, duration_ - 1.0));
    seekTarget_ = target;
    finished_ = false;
    frameCv_.notify_all();
    audioCv_.notify_all();
}

SDL_Texture *VideoPlayer::update() {
    if (!isOpen()) return texture_;
    double now = position();

    Frame due;
    bool got = false;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        // Show the newest frame that is due; drop any we're late for.
        while (!frames_.empty() && frames_.front().pts <= now + 0.005) {
            due = std::move(frames_.front());
            frames_.pop_front();
            got = true;
        }
        // Without audio the clock starts at the first frame.
        if (!hasAudio_ && !got && shownPts_ == 0 && !frames_.empty() && now < frames_.front().pts - 1.0) {
            wallBase_ = frames_.front().pts;
            wallStart_ = SDL_GetTicks();
        }
        if (decoderDone_ && frames_.empty()) {
            std::lock_guard<std::mutex> alock(audioMutex_);
            if (!hasAudio_ || audioSize_ == 0) finished_ = true;
        }
    }
    frameCv_.notify_all();

    if (got) {
        if (!texture_ || due.w != width_ || due.h != height_) {
            if (texture_) SDL_DestroyTexture(texture_);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
            texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, due.w, due.h);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
            width_ = due.w;
            height_ = due.h;
        }
        if (texture_)
            SDL_UpdateTexture(texture_, nullptr, due.pixels.data(), due.w * 4);
        shownPts_ = due.pts;
    }
    return texture_;
}

void VideoPlayer::decodeLoop() {
    AVFormatContext *fmt = nullptr;
    AVCodecContext *vctx = nullptr, *actx = nullptr;
    SwsContext *sws = nullptr;
    SwrContext *swr = nullptr;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int vIdx = -1, aIdx = -1;
    double skipUntil = -1;
    bool eof = false;

    auto fail = [&](const std::string &msg) {
        error_ = msg;
        decoderDone_ = true;
        finished_ = true;
    };

    // Lets close() abort blocking network reads right away.
    fmt = avformat_alloc_context();
    fmt->interrupt_callback.callback = [](void *self) { return static_cast<VideoPlayer *>(self)->quit_ ? 1 : 0; };
    fmt->interrupt_callback.opaque = this;
    AVDictionary *opts = nullptr;
    if (!headers_.empty()) av_dict_set(&opts, "headers", headers_.c_str(), 0);
    av_dict_set(&opts, "rw_timeout", "20000000", 0); // 20 s network timeout (microseconds)
    int openResult = avformat_open_input(&fmt, path_.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (openResult < 0) {
        fail(path_.rfind("http", 0) == 0 ? "Can't reach the server" : "Can't open file");
    } else if (avformat_find_stream_info(fmt, nullptr) < 0) {
        fail("Unsupported file");
    } else {
        if (fmt->duration > 0) duration_ = double(fmt->duration) / AV_TIME_BASE;
        if (!audioOnly_) vIdx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        aIdx = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, vIdx, nullptr, 0);
        if (audioOnly_) {
            if (aIdx < 0) fail("No audio in file");
        } else if (vIdx < 0) {
            fail("No video in file");
        } else if (!(vctx = openDecoder(fmt->streams[vIdx], true, &decoderName_))) {
            fail("Unsupported video format");
        }
        if ((vctx || audioOnly_) && aIdx >= 0 && (actx = openDecoder(fmt->streams[aIdx], false, nullptr))) {
            AVChannelLayout outLayout;
            av_channel_layout_default(&outLayout, mixChannels_);
            if (swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_S16, mixRate_, &actx->ch_layout,
                                    actx->sample_fmt, actx->sample_rate, 0, nullptr) >= 0 && swr_init(swr) >= 0) {
                hasAudio_ = true;
            } else {
                swr_free(&swr);
                avcodec_free_context(&actx);
            }
            av_channel_layout_uninit(&outLayout);
        }
        if (audioOnly_ && aIdx >= 0 && !actx) fail("Unsupported audio format");
    }

    auto handleVideo = [&](AVFrame *f) {
        double pts = f->best_effort_timestamp == AV_NOPTS_VALUE ? 0
                     : f->best_effort_timestamp * av_q2d(fmt->streams[vIdx]->time_base);
        if (pts < skipUntil) return;
        // Fit into MAX_W x MAX_H, keep the aspect ratio. Frames are converted to RGB here rather than
        // drawn as YUV textures: SDL's OpenGL ES renderer swaps red and blue when it draws a YUV
        // texture into a render target, which the rotated screen uses.
        double scale = std::min({1.0, double(MAX_W) / f->width, double(MAX_H) / f->height});
        int w = std::max(2, static_cast<int>(f->width * scale) & ~1);
        int h = std::max(2, static_cast<int>(f->height * scale) & ~1);
        sws = sws_getCachedContext(sws, f->width, f->height, static_cast<AVPixelFormat>(f->format), w, h,
                                   AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws) return;
        // Use the video's own colour matrix and range (HD video is BT.709; swscale assumes BT.601).
        int space = f->colorspace != AVCOL_SPC_UNSPECIFIED ? f->colorspace
                    : (f->height >= 720 ? AVCOL_SPC_BT709 : AVCOL_SPC_BT470BG);
        int fullRange = f->color_range == AVCOL_RANGE_JPEG;
        const int *coeffs = sws_getCoefficients(space);
        sws_setColorspaceDetails(sws, coeffs, fullRange, coeffs, 1, 0, 1 << 16, 1 << 16);
        Frame out;
        out.pts = pts;
        out.w = w;
        out.h = h;
        out.pixels.resize(static_cast<size_t>(w) * h * 4);
        uint8_t *dst[4] = {out.pixels.data(), nullptr, nullptr, nullptr};
        int dstStride[4] = {w * 4, 0, 0, 0};
        sws_scale(sws, f->data, f->linesize, 0, f->height, dst, dstStride);

        std::unique_lock<std::mutex> lock(frameMutex_);
        frameCv_.wait(lock, [&]() { return quit_ || seekTarget_ >= 0 || frames_.size() < MAX_FRAMES; });
        if (quit_ || seekTarget_ >= 0) return;
        frames_.push_back(std::move(out));
    };

    auto handleAudio = [&](AVFrame *f) {
        double pts = f->best_effort_timestamp == AV_NOPTS_VALUE ? -1
                     : f->best_effort_timestamp * av_q2d(fmt->streams[aIdx]->time_base);
        if (pts >= 0 && pts < skipUntil) return;
        int maxOut = swr_get_out_samples(swr, f->nb_samples);
        if (maxOut <= 0) return;
        std::vector<int16_t> tmp(static_cast<size_t>(maxOut) * mixChannels_);
        uint8_t *outPtr = reinterpret_cast<uint8_t *>(tmp.data());
        int got = swr_convert(swr, &outPtr, maxOut, const_cast<const uint8_t **>(f->extended_data), f->nb_samples);
        if (got <= 0) return;
        size_t n = static_cast<size_t>(got) * mixChannels_;

        std::unique_lock<std::mutex> lock(audioMutex_);
        audioCv_.wait(lock, [&]() { return quit_ || seekTarget_ >= 0 || audio_.size() - audioSize_ >= n; });
        if (quit_ || seekTarget_ >= 0) return;
        if (audioSize_ == 0 && pts >= 0) {
            audioEndPts_ = pts;
            audioClock_ = pts;
        }
        size_t write = (audioRead_ + audioSize_) % audio_.size();
        for (size_t i = 0; i < n; i++) {
            audio_[write] = tmp[i];
            write = (write + 1) % audio_.size();
        }
        audioSize_ += n;
        audioEndPts_ += double(got) / mixRate_;
    };

    auto drain = [&](AVCodecContext *ctx, bool video) {
        while (avcodec_receive_frame(ctx, frame) == 0) {
            if (video) handleVideo(frame);
            else handleAudio(frame);
            av_frame_unref(frame);
        }
    };

    while (!quit_ && (vctx || actx)) {
        double target = seekTarget_;
        if (target >= 0) {
            int64_t ts = static_cast<int64_t>(target * AV_TIME_BASE);
            av_seek_frame(fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
            if (vctx) avcodec_flush_buffers(vctx);
            if (actx) avcodec_flush_buffers(actx);
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                frames_.clear();
            }
            {
                std::lock_guard<std::mutex> lock(audioMutex_);
                audioRead_ = audioSize_ = 0;
                audioEndPts_ = target;
                audioClock_ = target;
            }
            wallBase_ = target;
            wallStart_ = SDL_GetTicks();
            skipUntil = target - 0.05;
            eof = false;
            decoderDone_ = false;
            seekTarget_ = -1;
        }

        if (eof) {
            SDL_Delay(10); // finished decoding; wait for a seek or close
            continue;
        }

        int r = av_read_frame(fmt, pkt);
        if (r < 0) {
            // End of file: flush the decoders.
            if (vctx) {
                avcodec_send_packet(vctx, nullptr);
                drain(vctx, true);
            }
            if (actx) {
                avcodec_send_packet(actx, nullptr);
                drain(actx, false);
            }
            eof = true;
            decoderDone_ = true;
            continue;
        }
        if (vctx && pkt->stream_index == vIdx) {
            if (avcodec_send_packet(vctx, pkt) >= 0) drain(vctx, true);
        } else if (actx && pkt->stream_index == aIdx) {
            if (avcodec_send_packet(actx, pkt) >= 0) drain(actx, false);
        }
        av_packet_unref(pkt);
    }

    sws_freeContext(sws);
    swr_free(&swr);
    avcodec_free_context(&vctx);
    avcodec_free_context(&actx);
    avformat_close_input(&fmt);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
