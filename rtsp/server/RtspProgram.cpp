#include "RtspProgram.h"

RtspProgram::RtspProgram(RtspProgramType type, std::string programName, std::string filePath)
    : mNextStreamId(0), mNextPayloadType(96), mProgramType(type), mProgramName(programName),
      mProgramFilePath(filePath), mpFormatCtx(nullptr) {}

RtspProgram::~RtspProgram() {
    if (mpFormatCtx) {
        avformat_free_context(mpFormatCtx);
    }
}

std::shared_ptr<RtspProgram::ProgramStream> RtspProgram::getProgramStream(int streamId) {
    auto iter = std::find_if(mProgramStreams.begin(), mProgramStreams.end(),
                             [streamId](auto item) { return item->streamId == streamId; });

    if (iter == mProgramStreams.end()) return nullptr;

    return *iter;
}

void RtspProgram::init() {
    if (mProgramType == RTSP_PROGRAM_FILE && !mProgramFilePath.empty()) {
        mpFormatCtx = avformat_alloc_context();

        AVDictionary *options = nullptr;
        av_dict_set_int(&options, "ignore_editlist", 1, 0);
        avformat_open_input(&mpFormatCtx, mProgramFilePath.c_str(), nullptr, &options);

        for (unsigned int i = 0; i < mpFormatCtx->nb_streams; ++i) {
            AVCodecParameters *pCodecParam = mpFormatCtx->streams[i]->codecpar;
            auto programStream = std::make_shared<ProgramStream>();
            programStream->streamId = mNextStreamId++;
            programStream->payloadType = mNextPayloadType++;
            programStream->timescale = mpFormatCtx->streams[i]->time_base.den;

            if (pCodecParam->codec_type == AVMEDIA_TYPE_VIDEO) {
                programStream->mediaType = MEDIA_CODEC_TYPE_VIDEO;
                if (pCodecParam->codec_id == AV_CODEC_ID_H264)
                    programStream->mime = "H264";
                else if (pCodecParam->codec_id == AV_CODEC_ID_HEVC)
                    programStream->mime = "HEVC";
            } else if (pCodecParam->codec_type == AVMEDIA_TYPE_AUDIO) {
                programStream->mediaType = MEDIA_CODEC_TYPE_AUDIO;
                if (pCodecParam->codec_id == AV_CODEC_ID_AAC) programStream->mime = "AAC";
            }

            if (pCodecParam->extradata_size > 0) {
                programStream->csdData.insert(programStream->csdData.end(), pCodecParam->extradata,
                                              pCodecParam->extradata + pCodecParam->extradata_size);
            }

            mProgramStreams.emplace_back(programStream);
        }

    } else if (mProgramType == RTSP_PROGRAM_SCREEN) {
        mScreenRecorder = std::unique_ptr<ScreenRecorder>();
        mScreenRecorder->init();

        auto videoStream = std::make_shared<ProgramStream>();
        videoStream->streamId = mNextStreamId++;
        videoStream->payloadType = mNextPayloadType++;
        videoStream->timescale = mScreenRecorder->getVideoTimescale();
        videoStream->mediaType = MEDIA_CODEC_TYPE_VIDEO;
        videoStream->mime = mScreenRecorder->getVideoMime();
        mScreenRecorder->getVideoCsdData(videoStream->csdData);
        mProgramStreams.emplace_back(videoStream);

        auto audioStream = std::make_shared<ProgramStream>();
        audioStream->streamId = mNextStreamId++;
        audioStream->payloadType = mNextPayloadType++;
        audioStream->timescale = mScreenRecorder->getAudioTimescale();
        audioStream->mediaType = MEDIA_CODEC_TYPE_AUDIO;
        audioStream->mime = mScreenRecorder->getAudioMime();
        mScreenRecorder->getAudioCsdData(audioStream->csdData);
        mProgramStreams.emplace_back(audioStream);

    } else if (mProgramType == RTSP_PROGRAM_CAMERA) {
    }

    mSdpHelper = std::make_unique<SdpServerHelper>();
    for (auto &stream : mProgramStreams) {
        mSdpHelper->addStream(stream->streamId, stream->payloadType, stream->mime, mProgramName);
        if (stream->csdData.size() > 0)
            mSdpHelper->parseCsd(stream->streamId, stream->csdData.data(), stream->csdData.size());
        mTimescalePairs.emplace_back(
            std::make_pair(stream->timescale, mSdpHelper->getTimescale(stream->streamId)));
    }
}

std::string RtspProgram::getSdpString(std::string localIPAddr, uint16_t localPort) {
    if (mSdpHelper) return mSdpHelper->toString(localIPAddr, localPort);

    return "";
}

int RtspProgram::getPayloadType(int streamId) {
    auto stream = getProgramStream(streamId);
    if (stream) return stream->payloadType;

    return -1;
}

MediaCodecType RtspProgram::getMediaType(int streamId) {
    auto stream = getProgramStream(streamId);
    if (stream) return stream->mediaType;

    return MEDIA_CODEC_TYPE_UNKNOWN;
}

std::string RtspProgram::getMime(int streamId) {
    auto stream = getProgramStream(streamId);
    if (stream) return stream->mime;

    return "";
}

void RtspProgram::getCsd(int streamId, std::vector<uint8_t> &csd) {
    auto stream = getProgramStream(streamId);
    if (stream) csd.insert(csd.end(), stream->csdData.begin(), stream->csdData.end());
}

void RtspProgram::start() {
    auto readfile = [this]() {
        if (mpFormatCtx) {
            for (;;) {
                auto packetBuffer = std::make_shared<AVPacketBuffer>();
                AVPacket *packet = packetBuffer->get();
                int ret = av_read_frame(mpFormatCtx, packet);
                if (ret >= 0) {
                    int streamIndex = packet->stream_index;
                    packet->dts = rescaleTimeStamp(packet->dts, mTimescalePairs[streamIndex].first,
                                                   mTimescalePairs[streamIndex].second);
                    packet->pts = rescaleTimeStamp(packet->pts, mTimescalePairs[streamIndex].first,
                                                   mTimescalePairs[streamIndex].second);
                    packet->time_base = {1, mTimescalePairs[streamIndex].second};

                    if (mVideoBufferReadyCB &&
                        mProgramStreams[streamIndex]->mediaType == MEDIA_CODEC_TYPE_VIDEO) {
                        mVideoBufferReadyCB(packetBuffer);
                    } else if (mAudioBufferReadyCB &&
                               mProgramStreams[streamIndex]->mediaType == MEDIA_CODEC_TYPE_AUDIO) {
                        mAudioBufferReadyCB(packetBuffer);
                    }
                } else {
                    if (mVideoBufferReadyCB) mVideoBufferReadyCB(nullptr);
                    if (mAudioBufferReadyCB) mAudioBufferReadyCB(nullptr);
                    break;
                }
            }
        }
        mIsStarted = false;
    };

    if (mIsStarted) return;

    if (mProgramType == RTSP_PROGRAM_FILE) {
        auto readfileThread = std::make_unique<std::thread>(readfile);
        readfileThread->detach();
    } else if (mProgramType == RTSP_PROGRAM_SCREEN) {

    } else if (mProgramType == RTSP_PROGRAM_CAMERA) {
    }

    mIsStarted = true;
}
