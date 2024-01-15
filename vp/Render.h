#ifndef RENDER_H
#define RENDER_H

#include "foundation/CountingQueue.h"
#include "foundation/PacketBuffer.h"
#include "foundation/FFBuffer.h"

#include <memory>
#include <thread>
#include <atomic>
#include <functional>
// #include <chrono>

#define SDL_MAIN_HANDLED
#include "SDL.h"

extern "C" {
#include "libavutil/frame.h"
}

class Render {
public:
    struct AudioPacket {
        std::shared_ptr<PacketBuffer> pBuffer;
        AVRational timeBase;
        int64_t pts;
    };

    Render();
    virtual ~Render();

    void start();
    void stop();
    void pause();
    void flush();
    void queueVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame);
    void queueAudioBuffer(std::shared_ptr<AVFrameBuffer> pFrame);
    AudioPacket dequeAudioBuffer();
    std::shared_ptr<AVFrameBuffer> dequeVideoBuffer();
    void setRenderVideoBufferCallback(std::function<void(std::shared_ptr<AVFrameBuffer>)> callback);
    void updateSystemClock();
    void updateAudioClock(int64_t pts);

    int getVideoFrameWidth() { return mVideoFrameWidth; }
    int getVideoFrameHeight() { return mVideoFrameHeight; }

    int64_t getPlaytime() { return mPlaytime; }

protected:
private:
    int64_t mPlaytime; // milliseconds

    // Audio
    uint16_t mAudioFormat;
    int mAudioFreq;
    uint8_t mAudioChannels;
    uint16_t mAudioSamples;    // Audio buffer size in sample FRAMES
    uint32_t mAudioBufferSize; // buffer size in callback function
    uint32_t mAudioBytesPerSec;
    SDL_AudioDeviceID mAudioDevID;
    std::shared_ptr<CountingQueue<AudioPacket>> mAudioBufferQueue;

    int64_t mAudioClock; // microseconds
    std::chrono::time_point<std::chrono::high_resolution_clock> mSystemClock;

    // Video
    int mVideoFrameWidth;
    int mVideoFrameHeight;
    std::shared_ptr<CountingQueue<std::shared_ptr<AVFrameBuffer>>> mVideoBufferQueue;
    std::function<void(std::shared_ptr<AVFrameBuffer>)> mRenderVideoBufferCallback;
    // std::shared_ptr<CountingQueue<std::shared_ptr<PacketBuffer> > > mVideoBufferQueue;
    // std::function<void(std::shared_ptr<PacketBuffer>)> mRenderVideoBufferCallback;

    std::atomic<bool> mPaused;
    std::mutex mStateMutex;
    std::condition_variable mStateCv;

    std::unique_ptr<std::thread> mRenderThread;
    std::atomic<bool> mRenderThreadExit;
    void renderThread();
};

#endif // RENDER_H
