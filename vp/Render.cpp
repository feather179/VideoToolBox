#include "Render.h"
#include "foundation/Log.h"

#include <iostream>
#include <chrono>

#include <windows.h>
#include <processthreadsapi.h>

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
}

static void sdlAudioCallback(void *userdata, uint8_t *stream, int len) {
    Render *pRender = (Render *)userdata;
    pRender->updateSystemClock();

    Render::AudioPacket packet = pRender->dequeAudioBuffer();
    int64_t pts = av_rescale_q(packet.pts, packet.timeBase, {1, AV_TIME_BASE});
    pRender->updateAudioClock(pts);

    SDL_memset(stream, 0, len);
    auto pBuffer = packet.pBuffer;
    if (pBuffer) {
        len = std::min<uint32_t>(len, pBuffer->size());
        SDL_MixAudioFormat(stream, pBuffer->data(), AUDIO_S16SYS, len, SDL_MIX_MAXVOLUME);
    }
}

Render::Render() {
    // ctor
    mAudioFormat = AUDIO_S16SYS;
    mAudioFreq = 48000;
    mAudioChannels = 2;
    mAudioSamples = 1024;
    mAudioBufferSize = SDL_AUDIO_BITSIZE(mAudioFormat) * mAudioSamples * mAudioChannels;
    mAudioBytesPerSec = mAudioFreq * mAudioChannels * SDL_AUDIO_BITSIZE(mAudioFormat);
    mAudioDevID = 0;

    mAudioClock = 0;

    mPaused = false;

    mAudioBufferQueue = std::make_shared<CountingQueue<AudioPacket>>(10);
    mVideoBufferQueue = std::make_shared<CountingQueue<std::shared_ptr<AVFrameBuffer>>>(10);
}

Render::~Render() {
    stop();
    // dtor
    if (mRenderThread != nullptr) {
        mRenderThreadExit = true;
        mRenderThread->join();
    }
}

void Render::start() {
    if (mAudioDevID == 0) {

        int flags = SDL_INIT_AUDIO;
        SDL_AudioSpec wantedSpec, spec;

        if (SDL_Init(flags)) {
            printf("Failed to init SDL\n");
            return;
        }

        wantedSpec.freq = mAudioFreq;
        wantedSpec.format = mAudioFormat;
        wantedSpec.channels = mAudioChannels;
        wantedSpec.silence = 0;
        wantedSpec.samples = mAudioSamples;
        wantedSpec.callback = sdlAudioCallback;
        wantedSpec.userdata = (void *)this;

        mAudioDevID =
            SDL_OpenAudioDevice(nullptr, SDL_FALSE, &wantedSpec, &spec,
                                SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
        if (mAudioDevID == 0) {
            printf("Failed to open SDL Audio Device\n");
            return;
        }

        // update audio parameters
        mAudioFormat = spec.format;
        mAudioFreq = spec.freq;
        mAudioChannels = spec.channels;
        mAudioSamples = spec.samples;
        mAudioBufferSize = spec.size;
        mAudioBytesPerSec = mAudioFreq * mAudioChannels * SDL_AUDIO_BITSIZE(mAudioFormat);

        SDL_PauseAudioDevice(mAudioDevID, 0);

        mRenderThread = std::make_unique<std::thread>(&Render::renderThread, this);
        // mRenderThread = std::unique_ptr<std::thread>(new std::thread(&Render::renderThread,
        // this));
    } else {
        // Resume from paused state
        SDL_PauseAudioDevice(mAudioDevID, 0);
        mAudioBufferQueue->init();
        mVideoBufferQueue->init();
        std::unique_lock<std::mutex> lock(mStateMutex);
        mPaused = false;
        mStateCv.notify_all();
    }
}

void Render::stop() {
    if (mAudioDevID > 0) {
        SDL_CloseAudioDevice(mAudioDevID);
        mAudioDevID = 0;
        SDL_Quit();
    }
    mAudioBufferQueue->abort();
    mVideoBufferQueue->abort();
}

void Render::pause() {
    SDL_PauseAudioDevice(mAudioDevID, 1);
    mAudioClock = 0;
    std::unique_lock<std::mutex> lock(mStateMutex);
    mPaused = true;
}

void Render::flush() {
    LOGD("render flush\n");
    mAudioBufferQueue->abort();
    mVideoBufferQueue->abort();

    pause();
}

void Render::queueVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame) {
    LOGD("queue video buffer, pts=%lld\n", (*pFrame)->pts);
    // AVFrame* frame = av_frame_clone(pFrame);
    mVideoBufferQueue->push(pFrame);
#if 0
    SwsContext *pCtx = nullptr;
    AVPixelFormat srcFormat = (AVPixelFormat)pFrame->format;
    AVPixelFormat dstFormat = AV_PIX_FMT_RGBA;
    uint8_t *srcData[4];
    int srcLinesize[4];
    uint8_t *dstData[4];
    int dstLinesize[4];

    mVideoFrameWidth = pFrame->width;
    mVideoFrameHeight = pFrame->height;

    size_t size = mVideoFrameWidth * mVideoFrameHeight * 4;     // for RGBA8888
    auto pBuffer = std::make_shared<PacketBuffer>(size);

    pCtx = sws_getContext(mVideoFrameWidth,
                          mVideoFrameHeight,
                          srcFormat,
                          mVideoFrameWidth,
                          mVideoFrameHeight,
                          dstFormat,
                          SWS_FAST_BILINEAR,
                          nullptr,
                          nullptr,
                          nullptr);

    for (int i = 0; i < 4; i++) {
        srcData[i] = pFrame->data[i];
        srcLinesize[i] = pFrame->linesize[i];
    }

    av_image_fill_arrays(dstData,
                         dstLinesize,
                         pBuffer->data(),
                         AV_PIX_FMT_RGBA,
                         mVideoFrameWidth,
                         mVideoFrameHeight,
                         1);

    sws_scale(pCtx,
              srcData,
              srcLinesize,
              0,
              mVideoFrameHeight,
              dstData,
              dstLinesize);

    if (pCtx)
        sws_freeContext(pCtx);

    mVideoBufferQueue->push(pBuffer);
#endif
}

void Render::queueAudioBuffer(std::shared_ptr<AVFrameBuffer> pFrame) {
    LOGD("queue audio buffer, pts=%lld\n", (*pFrame)->pts);
    SwrContext *pCtx = nullptr;
    AVChannelLayout chLayout;

    //    size_t size = mAudioBufferSize;

    av_channel_layout_default(&chLayout, mAudioChannels);
    //    av_get_channel_layout_nb_channels
    swr_alloc_set_opts2(&pCtx, &chLayout, AV_SAMPLE_FMT_S16, mAudioFreq, &((*pFrame)->ch_layout),
                        (AVSampleFormat)(*pFrame)->format, (*pFrame)->sample_rate, 0, nullptr);
    if (!pCtx || swr_init(pCtx) < 0) {
        printf("Cannot create sample rate converter\n");
        swr_free(&pCtx);
        pCtx = nullptr;
        return;
    }

    int outSampleCount = mAudioSamples * mAudioFreq / (*pFrame)->sample_rate + 256;
    int outSize = av_samples_get_buffer_size(nullptr, chLayout.nb_channels, outSampleCount,
                                             AV_SAMPLE_FMT_S16, 0);
    uint8_t *pOutBuffer = new uint8_t[outSize];
    int sampleCount =
        swr_convert(pCtx, &pOutBuffer, outSampleCount, (const uint8_t **)(*pFrame)->extended_data,
                    (*pFrame)->nb_samples);
    //    memcpy(pOutBuffer, pFrame->extended_data[0], outSize);
    auto pBuffer = std::make_shared<PacketBuffer>((void *)pOutBuffer, outSize);
    int realSize =
        av_samples_get_buffer_size(nullptr, mAudioChannels, sampleCount, AV_SAMPLE_FMT_S16, 0);
    pBuffer->setRange(0, realSize);

    if (pCtx) swr_free(&pCtx);

    mAudioBufferQueue->push({pBuffer, (*pFrame)->time_base, (*pFrame)->pts});
}

Render::AudioPacket Render::dequeAudioBuffer() {
    AudioPacket packet;
    mAudioBufferQueue->pop(packet);
    return packet;
}

std::shared_ptr<AVFrameBuffer> Render::dequeVideoBuffer() {
    std::shared_ptr<AVFrameBuffer> frame = nullptr;
    mVideoBufferQueue->pop(frame);
    return frame;
}

void Render::setRenderVideoBufferCallback(
    std::function<void(std::shared_ptr<AVFrameBuffer>)> callback) {
    mRenderVideoBufferCallback = callback;
}

void Render::updateSystemClock() {
    mSystemClock = std::chrono::high_resolution_clock::now();
}

void Render::updateAudioClock(int64_t pts) {
    int64_t audioClk = pts - 2 * mAudioBufferSize * 1000000 / mAudioBytesPerSec;
    // printf("audio clock:%lld\n", audioClk);
    if (audioClk < 0) audioClk = 0;
    mAudioClock = audioClk;
}

void Render::renderThread() {

    // SetThreadDescription(GetCurrentThread(), L"RenderThread");

    while (true) {
        if (mRenderThreadExit) break;

        if (mPaused == true) {
            std::unique_lock<std::mutex> lock(mStateMutex);
            mStateCv.wait(lock, [this]() { return mPaused == false; });
        }

        std::shared_ptr<AVFrameBuffer> pFrame = dequeVideoBuffer();

        if (pFrame) {

            LOGD("render video buffer, pts=%lld\n", (*pFrame)->pts);

            int64_t pts = av_rescale_q((*pFrame)->pts, (*pFrame)->time_base, {1, AV_TIME_BASE});
            auto now = std::chrono::high_resolution_clock::now();
            int64_t deltaUs =
                std::chrono::duration_cast<std::chrono::microseconds>(now - mSystemClock).count();
            int64_t audioClk = mAudioClock;
            int64_t delayUs = pts - audioClk - deltaUs - 1000; // 1000us for render process time
            if (delayUs > 0 && audioClk > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(delayUs));
            }
            mPlaytime = pts / 1000;
            if (mRenderVideoBufferCallback) mRenderVideoBufferCallback(pFrame);
            // av_frame_free(&pFrame);
        }
    }
}
