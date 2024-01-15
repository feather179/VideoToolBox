#ifndef RTSP_PROGRAM_H
#define RTSP_PROGRAM_H

#include "foundation/Utils.h"
#include "foundation/FFBuffer.h"
#include "vr/ScreenRecorder.h"
#include "rtsp/server/SdpServerHelper.h"

#include <cstdint>

#include <vector>
#include <memory>
#include <functional>
#include <atomic>

extern "C" {
#include "libavformat/avformat.h"
}

class RtspProgram {
public:
    enum RtspProgramType {
        RTSP_PROGRAM_FILE,
        RTSP_PROGRAM_SCREEN, // + audio record
        RTSP_PROGRAM_CAMERA, // + audio record
    };

private:
    struct ProgramStream {
        int streamId = 0;
        int payloadType = 96;
        int timescale = 0; // Audio freq / Video timebase
        MediaCodecType mediaType = MEDIA_CODEC_TYPE_UNKNOWN;
        std::string mime;
        std::vector<uint8_t> csdData;
    };

    int mNextStreamId;
    int mNextPayloadType;
    RtspProgramType mProgramType;
    std::string mProgramName;

    // for RTSP_PROGRAM_FILE
    std::string mProgramFilePath;
    AVFormatContext *mpFormatCtx;
    // for RTSP_PROGRAM_SCREEN
    std::unique_ptr<ScreenRecorder> mScreenRecorder;
    // for RTSP_PROGRAM_CAMERA

    std::vector<std::shared_ptr<ProgramStream>> mProgramStreams;
    std::unique_ptr<SdpServerHelper> mSdpHelper;
    std::vector<std::pair<int, int>> mTimescalePairs;

    std::function<void(std::shared_ptr<AVPacketBuffer>)> mVideoBufferReadyCB;
    std::function<void(std::shared_ptr<AVPacketBuffer>)> mAudioBufferReadyCB;

    std::atomic_bool mIsStarted = false;

    std::shared_ptr<ProgramStream> getProgramStream(int streamId);

public:
    RtspProgram(RtspProgramType type, std::string programName, std::string filePath = "");
    RtspProgram(const RtspProgram &) = delete;
    RtspProgram &operator=(const RtspProgram &) = delete;
    virtual ~RtspProgram();

    void init();
    std::string getProgramName() { return mProgramName; }
    std::string getSdpString(std::string localIPAddr, uint16_t localPort);
    int getPayloadType(int streamId);
    MediaCodecType getMediaType(int streamId);
    std::string getMime(int streamId);
    void getCsd(int streamId, std::vector<uint8_t> &csd);

    void setVideoBufferReadyCB(std::function<void(std::shared_ptr<AVPacketBuffer>)> cb) {
        mVideoBufferReadyCB = cb;
    }
    void setAudioBufferReadyCB(std::function<void(std::shared_ptr<AVPacketBuffer>)> cb) {
        mAudioBufferReadyCB = cb;
    }

    void start();
};

#endif
