#ifndef SCREEN_RECORDER_H
#define SCREEN_RECORDER_H

#include <cstdint>
#include <thread>
#include <memory>
#include <atomic>
#include <map>
#include <vector>

#include "foundation/CountingQueue.h"
#include "foundation/FFBuffer.h"

#define SDL_MAIN_HANDLED
#include "SDL.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

class ScreenRecorder {
private:
    std::atomic<bool> mVideoRecordThreadExit;
    std::unique_ptr<std::thread> mVideoRecordThread;
    std::atomic<bool> mVideoEncodeThreadExit;
    std::unique_ptr<std::thread> mVideoEncodeThread;
    std::unique_ptr<CountingQueue<std::shared_ptr<AVFrameBuffer>>> mVideoFrameBufferQueue;
    std::unique_ptr<CountingQueue<std::shared_ptr<AVPacketBuffer>>> mVideoPacketBufferQueue;
    uint32_t mVideoNextPts;
    AVCodecContext *mpVideoEncoderCtx = nullptr;
    std::atomic<bool> mVideoInitDone;
    int mOffsetX, mOffsetY;
    int mWidth, mHeight;

    std::atomic<bool> mAudioEncodeThreadExit;
    std::unique_ptr<std::thread> mAudioEncodeThread;
    std::unique_ptr<CountingQueue<std::shared_ptr<AVFrameBuffer>>> mAudioFrameBufferQueue;
    std::unique_ptr<CountingQueue<std::shared_ptr<AVPacketBuffer>>> mAudioPacketBufferQueue;
    uint32_t mAudioNextPts;
    AVCodecContext *mpAudioEncoderCtx = nullptr;
    std::atomic<bool> mAudioInitDone;

    int mAudioFreq;
    AVSampleFormat mAudioFormat;
    AVChannelLayout mAudioLayout;
    int mAudioSamples; // Audio buffer size in sample FRAMES
    SDL_AudioDeviceID mAudioDevID;
    std::map<AVSampleFormat, SDL_AudioFormat> mAv2SDLAudioFormatMap;
    std::map<SDL_AudioFormat, AVSampleFormat> mSDL2AvAudioFormatMap;

    std::atomic<bool> mMuxThreadExit;
    std::unique_ptr<std::thread> mMuxThread;

    void videoRecordThread();
    void videoEncodeThread();
    void audioEncodeThread();
    void muxThread();

public:
    ScreenRecorder(int width, int height);
    ScreenRecorder(int width, int height, int offsetX, int offsetY);
    ~ScreenRecorder();
    void init();
    void start();
    void stop();
    void queueVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame);
    void queueAudioBuffer(std::shared_ptr<AVFrameBuffer> pFrame);
    void queueAudioBuffer(uint8_t *buffer, int len);

    int getVideoTimescale();
    std::string getVideoMime();
    void getVideoCsdData(std::vector<uint8_t> &csd);

    int getAudioTimescale();
    std::string getAudioMime();
    void getAudioCsdData(std::vector<uint8_t> &csd);
};

#endif
