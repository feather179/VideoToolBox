#include "Player.h"
#include "foundation/Log.h"

#include <chrono>

#include <windows.h>
#include <processthreadsapi.h>

#include "foundation/Semaphore.h"
#include "vp/Render.h"

// auto logStartTime = std::chrono::system_clock::now();
// char logBuffer[256];
// std::mutex logMutex;
// static inline void LOGD(const char *buf) {
//     std::unique_lock<std::mutex> lock(logMutex);
//     auto now = std::chrono::system_clock::now();
//     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - logStartTime);
//     std::cout << "[" << duration.count() << "] " << buf << std::endl;
// }

Player::Player() {
    printf("%s\n", __func__);
    mDuration = 0;
    mPlaytime = -1;
    mSeekTime = -1;

    mState = STATE_UNINITIALIZED;
    mMessageThreadExit = false;
    mMessageSemaphore = std::unique_ptr<Semaphore>(new Semaphore(5));
    // mMessageSemaphore = 0;
    mMessageThread =
        std::unique_ptr<std::thread>(new std::thread(&Player::messageHandleThread, this));
    // mVFifo = av_fifo_alloc2(10, sizeof(void *), 0);
    mVideoInputBufferQueue = std::make_unique<CountingQueue<std::shared_ptr<AVPacketBuffer>>>(10);
    // mAFifo = av_fifo_alloc2(10, sizeof(void *), 0);
    mAudioInputBufferQueue = std::make_unique<CountingQueue<std::shared_ptr<AVPacketBuffer>>>(10);

    mRender = std::make_shared<Render>();
}

// Delegating Constructor
Player::Player(std::string fileUrl) : Player() {
    mFileUrl = fileUrl;
}

Player::~Player() {
    printf("%s\n", __func__);
    mMessageThreadExit = true;
    mMessageSemaphore->release();
    mMessageThread->join();

    if (mExtractThread != nullptr) {
        mExtractThreadExit = true;
        mExtractThread->join();
    }

    if (mVideoDecodeThread != nullptr) {
        mVideoDecodeThreadExit = true;
        // mVCv.notify_one();
        mVideoDecodeThread->join();
    }

    if (mAudioDecodeThread != nullptr) {
        mAudioDecodeThreadExit = true;
        // mACv.notify_one();
        mAudioDecodeThread->join();
    }

    // if (mVFifo)
    //     av_fifo_freep2(&mVFifo);
    // if (mAFifo)
    //     av_fifo_freep2(&mAFifo);

    if (mVDecContext) avcodec_free_context(&mVDecContext);

    mStateCv.notify_all();
}

void Player::setFileUrl(std::string fileUrl) {
    mFileUrl = fileUrl;
}

void Player::setVideoBufferReadyCallback(
    std::function<void(std::shared_ptr<AVFrameBuffer>)> callback) {
    mVideoBufferReadyCallback = callback;
}

void Player::setAudioBufferReadyCallback(
    std::function<void(std::shared_ptr<AVFrameBuffer>)> callback) {
    mAudioBufferReadyCallback = callback;
}

void Player::setRenderVideoBufferCallback(
    std::function<void(std::shared_ptr<AVFrameBuffer>)> callback) {
    mRenderVideoBufferCallback = callback;
    mRender->setRenderVideoBufferCallback(
        std::bind(&Player::onRenderVideoBuffer, this, std::placeholders::_1));
}

void Player::onRenderVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame) {
    int64_t pts = av_rescale_q((*pFrame)->pts, (*pFrame)->time_base, {1, AV_TIME_BASE});
    mPlaytime = pts / 1000;
    if (mRenderVideoBufferCallback) mRenderVideoBufferCallback(pFrame);
}

void Player::postMessage(Message msg) {
    LOGD("post message %d\n", msg);
    mMessageQueue.push(msg);
    mMessageSemaphore->release();
}

void Player::start() {
    // cout << "[" << std::this_thread::get_id() << "]" << __func__ << endl;
    postMessage(kWhatStart);
}

void Player::pause() {
    postMessage(kWhatPause);
}

void Player::seek(int64_t time) {
    if (time < 0) {
        mSeekTime = 0;
    } else if (time > mDuration) {
        mSeekTime = mDuration;
    } else {
        mSeekTime = time;
    }

    printf("seek time:%lld\n", mSeekTime);
    postMessage(kWhatSeek);
}

void Player::handleStart() {
    // cout << "[" << std::this_thread::get_id() << "]" << __func__ << endl;
    LOGD("handle start\n");
    std::unique_lock<std::mutex> lock(mStateMutex);

    // switch (mState) {
    //     case STATE_UNINITIALIZED: {

    //     }
    //     break;
    //     case STATE_SEEKING:
    //     break;
    //     case STATE_PAUSED: {
    //         mStateCv.notify_all();
    //         mRender->start();
    //     }

    //     break;

    //     default:
    //     break;
    // }
    if (mState == STATE_UNINITIALIZED) {
        mState = STATE_PLAYING;
        mRender->start();
        mExtractThreadExit = false;
        mExtractThread =
            std::unique_ptr<std::thread>(new std::thread(&Player::extractThread, this));
    } else if (mState == STATE_PAUSED || mState == STATE_SEEKING) {
        mState = STATE_PLAYING;
        mStateCv.notify_all();
        mRender->start();
    }

    if (!mVideoDecodeThread) {
        mVideoInputBufferQueue->init();
        mVideoDecodeThreadExit = false;
        mVideoDecodeThread =
            std::unique_ptr<std::thread>(new std::thread(&Player::videoDecodeThread, this));
    }

    if (!mAudioDecodeThread) {
        mAudioInputBufferQueue->init();
        mAudioDecodeThreadExit = false;
        mAudioDecodeThread =
            std::unique_ptr<std::thread>(new std::thread(&Player::audioDecodeThread, this));
    }
}

void Player::handlePause() {
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (mState == STATE_PLAYING) {
        mState = STATE_PAUSED;
        mRender->pause();
    }
}

void Player::handleSeek() {
    LOGD("handle seek\n");
    if (mState == STATE_PLAYING) {
        {
            std::unique_lock<std::mutex> lock(mStateMutex);
            mState = STATE_SEEKING;

            mVideoInputBufferQueue->abort();
            mAudioInputBufferQueue->abort();

            mRender->flush();
        }

        mVideoDecodeThreadExit = true;
        mAudioDecodeThreadExit = true;
        mStateCv.notify_all();
        if (mVideoDecodeThread->joinable()) {
            mVideoDecodeThread->join();
            mVideoDecodeThread.reset();
        }
        if (mAudioDecodeThread->joinable()) {
            mAudioDecodeThread->join();
            mAudioDecodeThread.reset();
        }

        avcodec_flush_buffers(mVDecContext);
        avcodec_flush_buffers(mADecContext);
    }
}

void Player::messageHandleThread() {
    Message msg;
    while (true) {
        mMessageSemaphore->acquire();
        if (mMessageThreadExit) break;
        if (mMessageQueue.empty()) continue;
        msg = mMessageQueue.front();
        mMessageQueue.pop();

        switch (msg) {
            case kWhatStart:
                handleStart();
                break;
            case kWhatPause:
                handlePause();
                break;
            case kWhatSeek:
                handleSeek();
                break;
            default:
                break;
        }
    }
}

void Player::extractThread() {
    int ret;
    int videoStreamId = -1, audioStreamId = -1;
    // int videoIndex = 0, audioIndex = 0;

    AVFormatContext *pFmtCtx = nullptr;
    // AVPacket *pPacket = nullptr;
    AVCodecParameters *pPar = nullptr;

    // SetThreadDescription(GetCurrentThread(), L"ExtractThread");

    if (mFileUrl.empty()) return;

    pFmtCtx = avformat_alloc_context();
    if (!pFmtCtx) {
        printf("Failed to alloc format comtext\n");
        return;
    }

    ret = avformat_open_input(&pFmtCtx, mFileUrl.c_str(), nullptr, nullptr);
    if (ret < 0) {
        printf("Failed to open input\n");
        return;
    }

    for (unsigned int i = 0; i < pFmtCtx->nb_streams; i++) {
        pPar = pFmtCtx->streams[i]->codecpar;
        if (pPar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamId = i;
            mpVideoStream = pFmtCtx->streams[i];
            mDuration = max(mDuration, av_rescale_q(mpVideoStream->duration,
                                                    mpVideoStream->time_base, {1, AV_TIME_BASE}));
            mVDecoder = avcodec_find_decoder(pPar->codec_id);
            mVDecContext = avcodec_alloc_context3(mVDecoder);
            avcodec_parameters_to_context(mVDecContext, pPar);
            avcodec_open2(mVDecContext, mVDecoder, nullptr);
        } else if (pPar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamId = i;
            mpAudioStream = pFmtCtx->streams[i];
            mDuration = max(mDuration, av_rescale_q(mpAudioStream->duration,
                                                    mpAudioStream->time_base, {1, AV_TIME_BASE}));
            mADecoder = avcodec_find_decoder(pPar->codec_id);
            mADecContext = avcodec_alloc_context3(mADecoder);
            avcodec_parameters_to_context(mADecContext, pPar);
            avcodec_open2(mADecContext, mADecoder, nullptr);
        }
    }
    mDuration /= 1000; // microseconds to milliseconds

    // auto putInputBuffer = [&](bool isVideo, AVPacket **packet) -> int {
    //     int ret2;
    //     if (isVideo) {
    //         std::unique_lock<std::mutex> lock(mVMutex);
    //         ret2 = av_fifo_write(mVFifo, packet, 1);
    //         if (ret2 >= 0) {
    //             printf("put video input buffer: 0x%x, pts: %lld\n", *(int *)packet,
    //             (*packet)->pts); mVCv.notify_one();
    //         }
    //     } else {
    //         // Audio
    //         std::unique_lock<std::mutex> lock(mAMutex);
    //         ret2 = av_fifo_write(mAFifo, packet, 1);
    //         if (ret2 >= 0) {
    //             printf("put audio input buffer: 0x%x, pts: %lld\n", *(int *)packet,
    //             (*packet)->pts); mACv.notify_one();
    //         }
    //     }
    //     return ret2;
    // };

    while (true) {
        if (mExtractThreadExit) break;

        if (mState == STATE_SEEKING) {
            std::unique_lock<std::mutex> lock(mStateMutex);
            if (mSeekTime >= 0) {
                int64_t pts = av_rescale_q(mSeekTime, {1, 1000}, mpVideoStream->time_base);
                av_seek_frame(pFmtCtx, videoStreamId, pts, AVSEEK_FLAG_BACKWARD);
                mSeekTime = -1;
                start();
                continue;
            } else {
                mStateCv.wait(lock, [this]() { return mState != STATE_SEEKING; });
            }
        }

        // pPacket = av_packet_alloc();
        std::shared_ptr<AVPacketBuffer> pPacket = std::make_shared<AVPacketBuffer>();
        ret = av_read_frame(pFmtCtx, pPacket->get());
        if (ret >= 0) {
            if ((*pPacket)->stream_index == videoStreamId) {
                // do {
                //     ret = putInputBuffer(true, &pPacket);
                // } while (ret < 0);
                // int64_t pts = av_rescale_q(pPacket->dts, mpVideoStream->time_base, {1, 1000});
                LOGD("read video packet, pts=%lld\n", (*pPacket)->pts);
                mVideoInputBufferQueue->push(pPacket);
            } else if ((*pPacket)->stream_index == audioStreamId) {
                // do {
                //     ret = putInputBuffer(false, &pPacket);
                // } while (ret < 0);
                // int64_t pts = av_rescale_q(pPacket->dts, mpAudioStream->time_base, { 1, 1000 });
                LOGD("read audio packet, pts=%lld\n", (*pPacket)->pts);
                mAudioInputBufferQueue->push(pPacket);
            }
        } else {
            mExtractThreadExit = true;
        }
    }

    if (mpVideoStream) mpVideoStream = nullptr;
    if (mpAudioStream) mpVideoStream = nullptr;

    if (pFmtCtx) avformat_free_context(pFmtCtx);
}

void Player::videoDecodeThread() {

    // AVPacket *pPacket = nullptr;
    // AVFrame *pFrame = nullptr;
    int ret;

    // SetThreadDescription(GetCurrentThread(), L"VideoDecodeThread");

    // auto getInputBuffer = [&]() -> void {
    //     int i;

    //    std::unique_lock<std::mutex> lock(mVMutex);

    //    while (true) {
    //        i = av_fifo_read(mVFifo, (void *)&pPacket, 1);
    //        if (i >= 0)
    //            break;
    //        else {
    //            mVCv.wait(lock);
    //        }
    //        if (mVideoDecodeThreadExit)
    //            break;
    //    }
    //};

    while (true) {
        // if (pPacket)  {
        //     av_packet_free(&pPacket);
        //     pPacket = nullptr;
        // }
        // getInputBuffer();

        if (mState == STATE_PAUSED) {
            std::unique_lock<std::mutex> lock(mStateMutex);
            mStateCv.wait(lock,
                          [this]() { return mState != STATE_PAUSED || mVideoDecodeThreadExit; });
        }

        if (mVideoDecodeThreadExit) break;

        std::shared_ptr<AVPacketBuffer> pPacket = nullptr;
        if (!mVideoInputBufferQueue->pop(pPacket)) {
            // failed to pop packet buffer
            // queue aborted in seek scenario
            break;
        }
        // seek
        // if (mVideoInputBufferQueue->isAbort()) {
        //    //mVideoInputBufferQueue->init();
        //    //continue;
        //    break;
        //}

        // if (pPacket) {
        // printf("get video input buffer: 0x%x, pts: %lld\n",
        // (int)reinterpret_cast<intptr_t>(pPacket), pPacket->pts);
        LOGD("get video input buffer, pts=%lld\n", (*pPacket)->pts);

        ret = avcodec_send_packet(mVDecContext, pPacket->get());
        while (ret >= 0) {
            std::shared_ptr<AVFrameBuffer> pFrame = std::make_shared<AVFrameBuffer>();
            ret = avcodec_receive_frame(mVDecContext, pFrame->get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (mpVideoStream) (*pFrame)->time_base = mpVideoStream->time_base;
            if (mVideoBufferReadyCallback) {
                mVideoBufferReadyCallback(pFrame);
            } else {
                mRender->queueVideoBuffer(pFrame);
            }
            // av_frame_free(&pFrame);
        }
        // av_packet_free(&pPacket);
        // pPacket = nullptr;
        //}
    }

    LOGD("%s exit\n", __func__);
}

void Player::audioDecodeThread() {

    // AVPacket *pPacket = nullptr;
    // AVFrame *pFrame = nullptr;
    int ret;

    // SetThreadDescription(GetCurrentThread(), L"AudioDecodeThread");

    // auto getInputBuffer = [&]() -> void {
    //     int i;

    //    std::unique_lock<std::mutex> lock(mAMutex);

    //    while (true) {
    //        i = av_fifo_read(mAFifo, (void *)&pPacket, 1);
    //        if (i >= 0)
    //            break;
    //        else {
    //            mACv.wait(lock);
    //        }
    //        if (mAudioDecodeThreadExit)
    //            break;
    //    }
    //};

    while (true) {
        // if (pPacket)  {
        //     av_packet_free(&pPacket);
        //     pPacket = nullptr;
        // }
        // getInputBuffer();

        if (mState == STATE_PAUSED) {
            std::unique_lock<std::mutex> lock(mStateMutex);
            mStateCv.wait(lock,
                          [this]() { return mState != STATE_PAUSED || mAudioDecodeThreadExit; });
        }

        if (mAudioDecodeThreadExit) break;

        std::shared_ptr<AVPacketBuffer> pPacket = nullptr;
        if (!mAudioInputBufferQueue->pop(pPacket)) {
            // failed to pop packet buffer
            // queue aborted in seek scenario
            break;
        }
        // seek
        // if (mAudioInputBufferQueue->isAbort()) {
        //    //mAudioInputBufferQueue->init();
        //    //continue;
        //    break;
        //}

        // if (pPacket) {
        // printf("get audio input buffer: 0x%x, pts: %lld\n",
        // (int)reinterpret_cast<intptr_t>(pPacket), pPacket->pts);
        LOGD("get audio input buffer, pts=%lld\n", (*pPacket)->pts);

        ret = avcodec_send_packet(mADecContext, pPacket->get());
        while (ret >= 0) {
            std::shared_ptr<AVFrameBuffer> pFrame = std::make_shared<AVFrameBuffer>();
            ret = avcodec_receive_frame(mADecContext, pFrame->get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (mpAudioStream) (*pFrame)->time_base = mpAudioStream->time_base;
            if (mAudioBufferReadyCallback) {
                mAudioBufferReadyCallback(pFrame);
            } else {
                mRender->queueAudioBuffer(pFrame);
            }

            // av_frame_free(&pFrame);
        }
        // av_packet_free(&pPacket);
        // pPacket = nullptr;
        //}
    }
    LOGD("%s exit\n", __func__);
}
