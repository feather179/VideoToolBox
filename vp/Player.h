#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>

class AVFrameBuffer;
class AVPacketBuffer;
class Render;
class Semaphore;
template <typename T>
class CountingQueue;

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/codec.h"
}

class Player : public std::enable_shared_from_this<Player> {
private:
    enum Message {
        kWhatStart = 0,
        kWhatPause,
        kWhatSeek,
    };

    enum State {
        STATE_UNINITIALIZED,
        STATE_PLAYING,
        STATE_SEEKING,
        STATE_PAUSED,
        STATE_STOPED,
    };

    std::string mFileUrl;
    int64_t mDuration; // milliseconds
    int64_t mPlaytime; // milliseconds
    int64_t mSeekTime;

    std::unique_ptr<std::thread> mMessageThread;
    std::atomic<bool> mMessageThreadExit;
    std::unique_ptr<Semaphore> mMessageSemaphore;
    // std::counting_semaphore<5> mMessageSemaphore { 0 };
    std::queue<Message> mMessageQueue;

    std::unique_ptr<std::thread> mExtractThread;
    std::atomic<bool> mExtractThreadExit;

    std::unique_ptr<std::thread> mVideoDecodeThread;
    std::atomic<bool> mVideoDecodeThreadExit;
    std::function<void(std::shared_ptr<AVFrameBuffer>)> mVideoBufferReadyCallback;
    // AVFifo *mVFifo;
    // std::mutex mVMutex;
    // std::condition_variable mVCv;
    std::unique_ptr<CountingQueue<std::shared_ptr<AVPacketBuffer>>> mVideoInputBufferQueue;
    const AVCodec *mVDecoder = nullptr;
    AVCodecContext *mVDecContext = nullptr;
    AVStream *mpVideoStream = nullptr;

    std::unique_ptr<std::thread> mAudioDecodeThread;
    std::atomic<bool> mAudioDecodeThreadExit;
    std::function<void(std::shared_ptr<AVFrameBuffer>)> mAudioBufferReadyCallback;
    // AVFifo *mAFifo;
    // std::mutex mAMutex;
    // std::condition_variable mACv;
    std::unique_ptr<CountingQueue<std::shared_ptr<AVPacketBuffer>>> mAudioInputBufferQueue;
    const AVCodec *mADecoder = nullptr;
    AVCodecContext *mADecContext = nullptr;
    AVStream *mpAudioStream = nullptr;

    std::function<void(std::shared_ptr<AVFrameBuffer>)> mRenderVideoBufferCallback;

    std::atomic<State> mState;
    std::mutex mStateMutex;
    std::condition_variable mStateCv;

    std::shared_ptr<Render> mRender;

    //
    void onRenderVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame);
    void postMessage(Message msg);
    void messageHandleThread();
    void extractThread();
    void handleStart();
    void handlePause();
    void handleSeek();
    void videoDecodeThread();
    void audioDecodeThread();

public:
    Player();
    Player(std::string fileUrl);
    ~Player();

    void setFileUrl(std::string fileUrl);
    void setVideoBufferReadyCallback(std::function<void(std::shared_ptr<AVFrameBuffer>)> callback);
    void setAudioBufferReadyCallback(std::function<void(std::shared_ptr<AVFrameBuffer>)> callback);
    void setRenderVideoBufferCallback(std::function<void(std::shared_ptr<AVFrameBuffer>)> callback);
    void start();
    void pause();
    void seek(int64_t time);

    int64_t getDuration() { return mDuration; }
    int64_t getPlaytime() { return mPlaytime; }
};

#endif // DECODER_H
