#include "ScreenRecorder.h"
#include "foundation/Log.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavdevice/avdevice.h"
}

static void sdlAudioCallback(void *userdata, uint8_t *stream, int len) {
    ScreenRecorder *pRecoder = (ScreenRecorder *)userdata;

    pRecoder->queueAudioBuffer(stream, len);
}

ScreenRecorder::ScreenRecorder(int width, int height, int offsetX, int offsetY)
    : mWidth(width), mHeight(height), mOffsetX(offsetX), mOffsetY(offsetY) {

    mAv2SDLAudioFormatMap[AV_SAMPLE_FMT_S16] = AUDIO_S16SYS;
    mAv2SDLAudioFormatMap[AV_SAMPLE_FMT_S32] = AUDIO_S32SYS;
    mAv2SDLAudioFormatMap[AV_SAMPLE_FMT_FLT] = AUDIO_F32SYS;

    mSDL2AvAudioFormatMap[AUDIO_S16SYS] = AV_SAMPLE_FMT_S16;
    mSDL2AvAudioFormatMap[AUDIO_S32SYS] = AV_SAMPLE_FMT_S32;
    mSDL2AvAudioFormatMap[AUDIO_F32SYS] = AV_SAMPLE_FMT_FLT;

    mVideoFrameBufferQueue =
        std::make_unique<CountingQueue<std::shared_ptr<AVFrameBuffer>>>(10);
    mVideoPacketBufferQueue =
        std::make_unique<CountingQueue<std::shared_ptr<AVPacketBuffer>>>(10);
    mVideoNextPts = 0;

    mAudioFrameBufferQueue =
        std::make_unique<CountingQueue<std::shared_ptr<AVFrameBuffer>>>(10);
    mAudioPacketBufferQueue =
        std::make_unique<CountingQueue<std::shared_ptr<AVPacketBuffer>>>(10);
    mAudioNextPts = 0;

    mAudioFreq    = 48000;
    mAudioLayout  = AV_CHANNEL_LAYOUT_STEREO;
    mAudioFormat  = AV_SAMPLE_FMT_S16;
    mAudioSamples = 1024;
    mAudioDevID   = 0;
}

ScreenRecorder::ScreenRecorder(int width, int height) : ScreenRecorder(width, height, 0, 0) {
    av_log_set_level(AV_LOG_DEBUG);
}

ScreenRecorder::~ScreenRecorder() {
    // stop();
}

void ScreenRecorder::init() {
    int ret = 0;

    if (mAudioDevID == 0) {

        int           flags      = SDL_INIT_AUDIO;
        SDL_AudioSpec wantedSpec = {0}, spec = {0};

        if (SDL_Init(flags)) {
            printf("Failed to init SDL\n");
            return;
        }

        int             freq     = mAudioFreq;
        SDL_AudioFormat format   = mAv2SDLAudioFormatMap[mAudioFormat];
        int             channels = mAudioLayout.nb_channels;
        int             samples  = mAudioSamples;

        wantedSpec.freq     = freq;
        wantedSpec.format   = format;
        wantedSpec.channels = channels;
        wantedSpec.silence  = 0;
        wantedSpec.samples  = samples;
        wantedSpec.callback = sdlAudioCallback;
        wantedSpec.userdata = (void *)this;

        mAudioDevID = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &wantedSpec, &spec,
                                          SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
                                              SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
        if (mAudioDevID == 0) {
            printf("Failed to open SDL Audio Device\n");
            return;
        }

        // update audio parameters
        mAudioFreq   = spec.freq;
        mAudioFormat = mSDL2AvAudioFormatMap[spec.format];
        if (spec.channels == 1) {
            mAudioLayout = AV_CHANNEL_LAYOUT_MONO;
        } else {
            mAudioLayout = AV_CHANNEL_LAYOUT_STEREO;
        }
        mAudioSamples = spec.samples;
    }

    mAudioInitDone                 = false;
    const AVCodec *pAudioEncoder   = avcodec_find_encoder_by_name("aac");
    mpAudioEncoderCtx              = avcodec_alloc_context3(pAudioEncoder);
    mpAudioEncoderCtx->bit_rate    = 128000;
    mpAudioEncoderCtx->sample_rate = mAudioFreq;
    mpAudioEncoderCtx->sample_fmt  = pAudioEncoder->sample_fmts[0];
    mpAudioEncoderCtx->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
    mpAudioEncoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_open2(mpAudioEncoderCtx, pAudioEncoder, nullptr);
    if (ret < 0) {
        LOGD("failed to open audio encoder\n");
        return;
    }
    mAudioInitDone = true;

    mVideoInitDone = false;
    // AVPixelFormat pixelFormat = AV_PIX_FMT_BGRA;
    AVPixelFormat  pixelFormat             = AV_PIX_FMT_NV12;
    const AVCodec *pVideoEncoder           = avcodec_find_encoder_by_name("hevc_nvenc");
    mpVideoEncoderCtx                      = avcodec_alloc_context3(pVideoEncoder);
    mpVideoEncoderCtx->width               = mWidth;
    mpVideoEncoderCtx->height              = mHeight;
    mpVideoEncoderCtx->pix_fmt             = pixelFormat;
    mpVideoEncoderCtx->bit_rate            = 4000000;
    mpVideoEncoderCtx->bit_rate_tolerance  = 4000000;
    mpVideoEncoderCtx->sample_aspect_ratio = {0, 1};
    mpVideoEncoderCtx->framerate           = {30, 1};
    mpVideoEncoderCtx->gop_size            = 30;
    mpVideoEncoderCtx->max_b_frames        = 0;
    mpVideoEncoderCtx->profile             = FF_PROFILE_HEVC_MAIN;
    mpVideoEncoderCtx->level               = 50;
    mpVideoEncoderCtx->time_base           = {1, 30};
    mpVideoEncoderCtx->color_range         = AVCOL_RANGE_MPEG;
    mpVideoEncoderCtx->colorspace          = AVCOL_SPC_BT709;
    mpVideoEncoderCtx->color_primaries     = AVCOL_PRI_BT709;
    mpVideoEncoderCtx->color_trc           = AVCOL_TRC_BT709;
    // mpVideoEncoderCtx->codec_tag = MKTAG('h', 'v', 'c', '1');
    // mpVideoEncoderCtx->field_order = AV_FIELD_PROGRESSIVE;
    mpVideoEncoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_open2(mpVideoEncoderCtx, pVideoEncoder, nullptr);
    if (ret < 0) {
        LOGD("failed to open video encoder\n");
        return;
    }
    mVideoInitDone = true;
}

void ScreenRecorder::start() {

    SDL_PauseAudioDevice(mAudioDevID, 0);

    mVideoRecordThreadExit = false;
    mVideoRecordThread =
        std::make_unique<std::thread>(&ScreenRecorder::videoRecordThread, this);

    mVideoEncodeThreadExit = false;
    mVideoEncodeThread =
        std::make_unique<std::thread>(&ScreenRecorder::videoEncodeThread, this);

    mAudioEncodeThreadExit = false;
    mAudioEncodeThread =
        std::make_unique<std::thread>(&ScreenRecorder::audioEncodeThread, this);

    mMuxThreadExit = false;
    mMuxThread     = std::make_unique<std::thread>(&ScreenRecorder::muxThread, this);
}

void ScreenRecorder::stop() {
    if (mAudioDevID > 0) {
        SDL_CloseAudioDevice(mAudioDevID);
        mAudioDevID = 0;
        SDL_Quit();
    }

    mVideoRecordThreadExit = true;
    mVideoEncodeThreadExit = true;
    mAudioEncodeThreadExit = true;
    mMuxThreadExit         = true;

    mVideoFrameBufferQueue->abort();
    mVideoPacketBufferQueue->abort();
    mAudioFrameBufferQueue->abort();
    mAudioPacketBufferQueue->abort();

    if (mVideoRecordThread) {
        mVideoRecordThread->join();
    }
    if (mVideoEncodeThread) {
        mVideoEncodeThread->join();
    }
    if (mAudioEncodeThread) {
        mAudioEncodeThread->join();
    }
    if (mMuxThread) {
        mMuxThread->join();
    }
}

void ScreenRecorder::queueVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame) {
    mVideoFrameBufferQueue->push(pFrame);
}

void ScreenRecorder::queueAudioBuffer(std::shared_ptr<AVFrameBuffer> pFrame) {
    mAudioFrameBufferQueue->push(pFrame);
}

void ScreenRecorder::queueAudioBuffer(uint8_t *buffer, int len) {
    if (!buffer) return;

    std::shared_ptr<AVFrameBuffer> pFrame = std::make_shared<AVFrameBuffer>();
    pFrame->get()->format                 = mAudioFormat;
    pFrame->get()->ch_layout              = mAudioLayout;
    pFrame->get()->nb_samples             = mAudioSamples;
    pFrame->get()->sample_rate            = mAudioFreq;
    av_frame_get_buffer(pFrame->get(), 1);
    memcpy(pFrame->get()->data[0], buffer, len);
    mAudioFrameBufferQueue->push(pFrame);
}

int ScreenRecorder::getVideoTimescale() {
    if (!mpVideoEncoderCtx) return 0;

    return mpVideoEncoderCtx->time_base.den;
}

std::string ScreenRecorder::getVideoMime() {
    if (!mpVideoEncoderCtx) return "";

    if (mpVideoEncoderCtx->codec_id == AV_CODEC_ID_H264)
        return "H264";
    else if (mpVideoEncoderCtx->codec_id == AV_CODEC_ID_HEVC)
        return "HEVC";

    return "";
}

void ScreenRecorder::getVideoCsdData(std::vector<uint8_t> &csd) {
    if (mpVideoEncoderCtx && mpVideoEncoderCtx->extradata_size > 0) {
        csd.insert(csd.end(), mpVideoEncoderCtx->extradata,
                   mpVideoEncoderCtx->extradata + mpVideoEncoderCtx->extradata_size);
    }
}

int ScreenRecorder::getAudioTimescale() {
    if (!mpAudioEncoderCtx) return 0;

    return mpAudioEncoderCtx->time_base.den;
}

std::string ScreenRecorder::getAudioMime() {
    if (!mpAudioEncoderCtx) return "";

    if (mpAudioEncoderCtx->codec_id == AV_CODEC_ID_AAC) return "AAC";

    return "";
}

void ScreenRecorder::getAudioCsdData(std::vector<uint8_t> &csd) {
    if (mpAudioEncoderCtx && mpAudioEncoderCtx->extradata_size > 0) {
        csd.insert(csd.end(), mpAudioEncoderCtx->extradata,
                   mpAudioEncoderCtx->extradata + mpAudioEncoderCtx->extradata_size);
    }
}

void ScreenRecorder::videoRecordThread() {
    int ret = 0;

#if 0
    const AVFilter *pSrcFilter = avfilter_get_by_name("ddagrab");
    const AVFilter *pHWDownloadFilter = avfilter_get_by_name("hwdownload");
    const AVFilter *pSinkFilter = avfilter_get_by_name("buffersink");

    AVFilterContext *pSrcFilterCtx = nullptr;
    AVFilterContext *pHWDownloadFilterCtx = nullptr;
    AVFilterContext *pSinkFilterCtx = nullptr;

    AVFilterGraph *pFilterGraph = avfilter_graph_alloc();
    if (!pFilterGraph) {
        LOGD("failed to create filter graph\n");
        return;
    }

    ret = avfilter_graph_create_filter(&pSrcFilterCtx, pSrcFilter, "in", "framerate=30", nullptr, pFilterGraph);
    if (ret < 0) {
        LOGD("failed to create src filter\n");
        return;
    }
    ret = av_opt_set_image_size(pSrcFilterCtx, "video_size", mWidth, mHeight, AV_OPT_SEARCH_CHILDREN);
    ret = av_opt_set_int(pSrcFilterCtx, "offset_x", mOffsetX, AV_OPT_SEARCH_CHILDREN);
    ret = av_opt_set_int(pSrcFilterCtx, "offset_y", mOffsetY, AV_OPT_SEARCH_CHILDREN);

    ret = avfilter_graph_create_filter(&pHWDownloadFilterCtx, pHWDownloadFilter, "hwdownload", nullptr, nullptr, pFilterGraph);
    if (ret < 0) {
        LOGD("failed to create hwdownload filter\n");
        return;
    }
    
    ret = avfilter_graph_create_filter(&pSinkFilterCtx, pSinkFilter, "out", nullptr, nullptr, pFilterGraph);
    if (ret < 0) {
        LOGD("failed to create sink filter\n");
        return;
    }
    enum AVPixelFormat pixFmts[] = { AV_PIX_FMT_BGRA, AV_PIX_FMT_NONE };
    ret = av_opt_set_int_list(pSinkFilterCtx, "pix_fmts", pixFmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    ret = avfilter_link(pSrcFilterCtx, 0, pHWDownloadFilterCtx, 0);
    if (ret < 0) {
        LOGD("failed to link src -> hwdownload\n");
        return;
    }
    ret = avfilter_link(pHWDownloadFilterCtx, 0, pSinkFilterCtx, 0);
    if (ret < 0) {
        LOGD("failed to link hwdownload -> sink\n");
        return;
    }

    ret = avfilter_graph_config(pFilterGraph, nullptr);
    if (ret < 0) {
        LOGD("failed to config graph\n");
        return;
    }

    int frameIndex = 0;
    while (true) {
        if (mVideoRecordThreadExit)
            break;

        std::shared_ptr<AVFrameBuffer> pFrame = std::make_shared<AVFrameBuffer>();
        ret = av_buffersink_get_frame(pSinkFilterCtx, pFrame->get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            LOGD("failed to get frame\n");
            break;
        }
        LOGD("get frame#%d width:%d height:%d format:%d\n",
            ++frameIndex, pFrame->get()->width, pFrame->get()->height, pFrame->get()->format);
        queueVideoBuffer(pFrame);
    }
#endif

    const AVCodec     *pDecoder = nullptr;
    AVCodecParameters *pParam;
    AVCodecContext    *pDecContext   = nullptr;
    AVStream          *mpVideoStream = nullptr;
    AVDictionary      *pOptions      = nullptr;
    char               str[256];

    avdevice_register_all();
    const AVInputFormat *pInputFmt = av_find_input_format("gdigrab");
    AVFormatContext     *pFmtCtx   = avformat_alloc_context();
    snprintf(str, sizeof(str), "%d", 30);
    av_dict_set(&pOptions, "framerate", str, 0);
    snprintf(str, sizeof(str), "%d", mOffsetX);
    av_dict_set(&pOptions, "offset_x", str, 0);
    snprintf(str, sizeof(str), "%d", mOffsetY);
    av_dict_set(&pOptions, "offset_y", str, 0);
    snprintf(str, sizeof(str), "%dx%d", mWidth, mHeight);
    av_dict_set(&pOptions, "video_size", str, 0);
    ret = avformat_open_input(&pFmtCtx, "desktop", pInputFmt, &pOptions);
    if (ret < 0) {
        LOGD("Failed to open %s\n", pInputFmt->name);
        avformat_free_context(pFmtCtx);
        return;
    }

    // LOGD("%d\n", pFmtCtx->nb_streams);
    pParam      = pFmtCtx->streams[0]->codecpar;
    pDecoder    = avcodec_find_decoder(pParam->codec_id);
    pDecContext = avcodec_alloc_context3(pDecoder);
    avcodec_parameters_to_context(pDecContext, pParam);
    avcodec_open2(pDecContext, pDecoder, nullptr);

#if 0
    FILE *pFile = fopen("D:\\Desktop\\ffmpeg-test\\ffmpeg-vs\\BMP\\output.yuv", "rb");
    while (true) {
        std::shared_ptr<AVFrameBuffer> pFrame = std::make_shared<AVFrameBuffer>();
        pFrame->get()->width = 1920;
        pFrame->get()->height = 1080;
        pFrame->get()->format = AV_PIX_FMT_NV12;
        av_frame_get_buffer(pFrame->get(), 1);
        int lumaSize = 1920 * 1080;
        int chromaSize = 1920 * 1080 / 2;
        if (fread(pFrame->get()->data[0], 1, lumaSize, pFile) != lumaSize)
            break;
        if (fread(pFrame->get()->data[1], 1, chromaSize, pFile) != chromaSize)
            break;
        queueVideoBuffer(pFrame);
    }
    fclose(pFile);

#else
    // int frameIndex = 0;
    // char filename[256];
    // snprintf(filename, sizeof(filename), "BMP/picture_%d.bmp", frameIndex++);
    // FILE *pFile = fopen(filename, "wb+");

    auto receiveFrame = [this, &ret, &pDecContext]() {
        while (true) {
            std::shared_ptr<AVFrameBuffer> pFrame = std::make_shared<AVFrameBuffer>();
            ret = avcodec_receive_frame(pDecContext, pFrame->get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;

            // fwrite(pFrame->get()->data[0], 1, pFrame->get()->linesize[0] *
            // pFrame->get()->height, pFile);
            LOGD("get frame width:%d height:%d format:%d\n", pFrame->get()->width,
                 pFrame->get()->height, pFrame->get()->format);
            queueVideoBuffer(pFrame);
        }
    };

    while (true) {
        if (mVideoRecordThreadExit) break;
        std::shared_ptr<AVPacketBuffer> pPacket = std::make_shared<AVPacketBuffer>();
        ret                                     = av_read_frame(pFmtCtx, pPacket->get());
        if (ret < 0) {
            LOGD("%s failed to read frame\n", pInputFmt->name);
        }

        ret = avcodec_send_packet(pDecContext, pPacket->get());
        receiveFrame();
    }

    avcodec_send_packet(pDecContext, nullptr);
    receiveFrame();
#endif

    avcodec_close(pDecContext);
    avcodec_free_context(&pDecContext);

    avformat_free_context(pFmtCtx);
}

void ScreenRecorder::videoEncodeThread() {
    int ret = 0;

    SwsContext *pSwsCtx =
        sws_getContext(mWidth, mHeight, AV_PIX_FMT_BGRA, mWidth, mHeight, AV_PIX_FMT_NV12,
                       SWS_BILINEAR, nullptr, nullptr, nullptr);
    sws_setColorspaceDetails(pSwsCtx, sws_getCoefficients(SWS_CS_ITU709), 1,
                             sws_getCoefficients(SWS_CS_ITU709), 0, 0, 1 << 16, 1 << 16);
    std::shared_ptr<AVFrameBuffer> pConvertFrame = std::make_shared<AVFrameBuffer>();
    pConvertFrame->get()->format                 = AV_PIX_FMT_NV12;
    pConvertFrame->get()->width                  = mWidth;
    pConvertFrame->get()->height                 = mHeight;
    av_frame_get_buffer(pConvertFrame->get(), 1);

    auto receivePacket = [this, &ret]() {
        while (true) {
            std::shared_ptr<AVPacketBuffer> pPacket = std::make_shared<AVPacketBuffer>();
            ret = avcodec_receive_packet(mpVideoEncoderCtx, pPacket->get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                LOGD("failed to receive video packet\n");
                break;
            }
            LOGD("get video packet, size=%d, dts=%d pts=%d\n", pPacket->get()->size,
                 pPacket->get()->dts, pPacket->get()->pts);
            mVideoPacketBufferQueue->push(pPacket);
        }
    };

    while (true) {
        if (mVideoEncodeThreadExit) break;

        std::shared_ptr<AVFrameBuffer> pFrame = nullptr;
        if (!mVideoFrameBufferQueue->pop(pFrame)) {
            // failed to pop buffer
            break;
        }
        sws_scale(pSwsCtx, (const uint8_t *const *)pFrame->get()->data,
                  pFrame->get()->linesize, 0, mHeight, pConvertFrame->get()->data,
                  pConvertFrame->get()->linesize);

        pConvertFrame->get()->pts = mVideoNextPts;
        mVideoNextPts += 1;
        ret = avcodec_send_frame(mpVideoEncoderCtx, pConvertFrame->get());
        if (ret < 0) {
            LOGD("failed to send video frame\n");
            break;
        }
        receivePacket();
    }

    // flush
    avcodec_send_frame(mpVideoEncoderCtx, nullptr);
    receivePacket();

    avcodec_close(mpVideoEncoderCtx);
    avcodec_free_context(&mpVideoEncoderCtx);
    sws_freeContext(pSwsCtx);
}

void ScreenRecorder::audioEncodeThread() {
    int ret = 0;

    AVChannelLayout inLayout  = mAudioLayout;
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    // swresample
    SwrContext *pSwrCtx = swr_alloc();
    av_opt_set_int(pSwrCtx, "in_sample_rate", mAudioFreq, 0);
    av_opt_set_chlayout(pSwrCtx, "in_chlayout", &inLayout, 0);
    av_opt_set_sample_fmt(pSwrCtx, "in_sample_fmt", mAudioFormat, 0);

    av_opt_set_int(pSwrCtx, "out_sample_rate", mAudioFreq, 0);
    av_opt_set_chlayout(pSwrCtx, "out_chlayout", &outLayout, 0);
    av_opt_set_sample_fmt(pSwrCtx, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
    ret = swr_init(pSwrCtx);
    if (ret < 0) {
        LOGD("failed to init SwrContext\n");
        return;
    }
    std::shared_ptr<AVFrameBuffer> pConvertFrame = std::make_shared<AVFrameBuffer>();
    pConvertFrame->get()->format                 = AV_SAMPLE_FMT_FLTP;
    pConvertFrame->get()->ch_layout              = mAudioLayout;
    pConvertFrame->get()->nb_samples             = mAudioSamples;
    av_frame_get_buffer(pConvertFrame->get(), 1);

    auto receivePacket = [this, &ret]() {
        while (true) {
            std::shared_ptr<AVPacketBuffer> pPacket = std::make_shared<AVPacketBuffer>();
            ret = avcodec_receive_packet(mpAudioEncoderCtx, pPacket->get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                LOGD("failed to receive audio packet\n");
                break;
            }
            LOGD("get audio packet, size=%d, dts=%d pts=%d\n", pPacket->get()->size,
                 pPacket->get()->dts, pPacket->get()->pts);
            mAudioPacketBufferQueue->push(pPacket);
        }
    };

    while (true) {
        if (mAudioEncodeThreadExit) break;

        std::shared_ptr<AVFrameBuffer> pFrame = nullptr;
        if (!mAudioFrameBufferQueue->pop(pFrame)) {
            // failed to pop buffer
            break;
        }

        swr_convert(pSwrCtx, pConvertFrame->get()->data, mAudioSamples,
                    (const uint8_t **)pFrame->get()->data, mAudioSamples);

        pConvertFrame->get()->pts = mAudioNextPts;
        mAudioNextPts += mAudioSamples;
        ret = avcodec_send_frame(mpAudioEncoderCtx, pConvertFrame->get());
        if (ret < 0) {
            LOGD("failed to send audio frame\n");
            break;
        }
        receivePacket();
    }

    avcodec_send_frame(mpAudioEncoderCtx, nullptr);
    receivePacket();

    swr_free(&pSwrCtx);
    avcodec_close(mpAudioEncoderCtx);
    avcodec_free_context(&mpAudioEncoderCtx);
}

void ScreenRecorder::muxThread() {
    int ret = 0;

    const AVOutputFormat *pOutputFormat = nullptr;
    AVFormatContext      *pFormatCtx    = nullptr;
    AVStream             *pVideoStream  = nullptr;
    AVStream             *pAudioStream  = nullptr;

    while (!mVideoInitDone || !mAudioInitDone)
        ;

    avformat_alloc_output_context2(&pFormatCtx, nullptr, "mp4", nullptr);
    pOutputFormat = pFormatCtx->oformat;

    pVideoStream = avformat_new_stream(pFormatCtx, nullptr);
    ret          = avcodec_parameters_from_context(pVideoStream->codecpar, mpVideoEncoderCtx);
    if (ret < 0) {
        LOGD("failed to copy video encoder parameters\n");
        return;
    }

    pAudioStream = avformat_new_stream(pFormatCtx, nullptr);
    ret          = avcodec_parameters_from_context(pAudioStream->codecpar, mpAudioEncoderCtx);
    if (ret < 0) {
        LOGD("failed to copy audio encoder parameters\n");
        return;
    }

    ret = avio_open(&pFormatCtx->pb, "screen_record.mp4", AVIO_FLAG_WRITE);
    if (ret < 0) {
        LOGD("failed to open output file\n");
        return;
    }

    ret = avformat_write_header(pFormatCtx, nullptr);
    if (ret < 0) {
        LOGD("failed to write header\n");
        return;
    }

    // av_packet_rescale_ts(pPkt, mpVideoEncoderCtx->time_base, pVideoStream->time_base);
    // av_interleaved_write_frame(pFormatCtx, pPkt);

    std::shared_ptr<AVPacketBuffer> pVideoPacket = nullptr;
    std::shared_ptr<AVPacketBuffer> pAudioPacket = nullptr;
    while (true) {
        if (mMuxThreadExit) break;

        if (!pVideoPacket) {
            if (!mVideoPacketBufferQueue->pop(pVideoPacket)) {
                // failed to pop buffer
                break;
            }
        }

        if (!pAudioPacket) {
            if (!mAudioPacketBufferQueue->pop(pAudioPacket)) {
                // failed to pop buffer
                break;
            }
        }

        if (av_compare_ts(pVideoPacket->get()->dts, mpVideoEncoderCtx->time_base,
                          pAudioPacket->get()->dts, mpAudioEncoderCtx->time_base) <= 0) {
            av_packet_rescale_ts(pVideoPacket->get(), mpVideoEncoderCtx->time_base,
                                 pVideoStream->time_base);
            pVideoPacket->get()->stream_index = pVideoStream->index;
            av_interleaved_write_frame(pFormatCtx, pVideoPacket->get());
            pVideoPacket = nullptr;
        } else {
            av_packet_rescale_ts(pAudioPacket->get(), mpAudioEncoderCtx->time_base,
                                 pAudioStream->time_base);
            pAudioPacket->get()->stream_index = pAudioStream->index;
            av_interleaved_write_frame(pFormatCtx, pAudioPacket->get());
            pAudioPacket = nullptr;
        }
    }

    av_write_trailer(pFormatCtx);

    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);
}
