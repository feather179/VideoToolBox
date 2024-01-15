#include "SdpServerHelper.h"

#include "foundation/Base64.h"

std::string SdpServerMPEG4Stream::MIME = "AAC";

SdpServerMPEG4Stream::SdpServerMPEG4Stream(int streamId, int payloadType, std::string programName) {
    mStreamId = streamId;
    mPayloadType = payloadType;
    mTimescale = 0;
    mMediaType = MEDIA_CODEC_TYPE_AUDIO;
    mEncodingName = "mpeg4-generic";
    mProgramName = programName;
}

SdpServerMPEG4Stream::~SdpServerMPEG4Stream() {}

void SdpServerMPEG4Stream::parseCsd(const uint8_t *data, int length) {
    // AudioSpecificConfig
    char str[256] = {0};
    int i = 0;
    for (int j = 0; j < length; ++j) {
        i += std::snprintf(str + i, sizeof(str) - i, "%02x", data[j]);
    }
    mConfigStr = str;

    parseCsdMPEG4(data, length, mObjectType, mFrequency, mChannels);
    mTimescale = mFrequency;
}

std::string SdpServerH264Stream::MIME = "AVC;H264";

SdpServerH264Stream::SdpServerH264Stream(int streamId, int payloadType, std::string programName) {
    mStreamId = streamId;
    mPayloadType = payloadType;
    mTimescale = 90000;
    mMediaType = MEDIA_CODEC_TYPE_VIDEO;
    mEncodingName = "H264";
    mProgramName = programName;
}

SdpServerH264Stream::~SdpServerH264Stream() {}

void SdpServerH264Stream::parseCsd(const uint8_t *data, int length) {
    parseCsdAVC(data, length, mSps, mPps);
}

std::string SdpServerH264Stream::getProfileLevelIdString() {
    // first 3 bytes of SPS, skip SPS header: SPS[0]
    char str[16] = {0};
    if (mSps.size() >= 4) {
        std::sprintf(str, "%02x%02x%02x", mSps[1], mSps[2], mSps[3]);
    }
    return std::string(str);
}

std::string SdpServerH264Stream::getSpsString() {
    char str[1024] = {0};
    encodeBase64(mSps.data(), mSps.size(), str);
    return std::string(str);
}

std::string SdpServerH264Stream::getPpsString() {
    char str[1024] = {0};
    encodeBase64(mPps.data(), mPps.size(), str);
    return std::string(str);
}

std::string SdpServerHEVCStream::MIME = "HEVC;H265";

SdpServerHEVCStream::SdpServerHEVCStream(int streamId, int payloadType, std::string programName) {
    mStreamId = streamId;
    mPayloadType = payloadType;
    mTimescale = 90000;
    mMediaType = MEDIA_CODEC_TYPE_VIDEO;
    mEncodingName = "H265";
    mProgramName = programName;
}

SdpServerHEVCStream::~SdpServerHEVCStream() {}

void SdpServerHEVCStream::parseCsd(const uint8_t *data, int length) {
    parseCsdHEVC(data, length, mVps, mSps, mPps, mSei);
}

std::string SdpServerHEVCStream::getVpsString() {
    char str[1024] = {0};
    encodeBase64(mVps.data(), mVps.size(), str);
    return std::string(str);
}

std::string SdpServerHEVCStream::getSpsString() {
    char str[1024] = {0};
    encodeBase64(mSps.data(), mSps.size(), str);
    return std::string(str);
}

std::string SdpServerHEVCStream::getPpsString() {
    char str[1024] = {0};
    encodeBase64(mPps.data(), mPps.size(), str);
    return std::string(str);
}

SdpServerHelper::SdpServerHelper() {}

SdpServerHelper::~SdpServerHelper() {}

void SdpServerHelper::addStream(int streamId,
                                int payloadType,
                                std::string mime,
                                std::string programName) {
    std::shared_ptr<SdpServerBaseStream> stream;

    if (SdpServerMPEG4Stream::MIME.find(mime) != std::string::npos) {
        stream = std::make_shared<SdpServerMPEG4Stream>(streamId, payloadType, programName);
    } else if (SdpServerH264Stream::MIME.find(mime) != std::string::npos) {
        stream = std::make_shared<SdpServerH264Stream>(streamId, payloadType, programName);
    } else if (SdpServerHEVCStream::MIME.find(mime) != std::string::npos) {
        stream = std::make_shared<SdpServerHEVCStream>(streamId, payloadType, programName);
    }

    if (stream) mStreams.emplace_back(stream);
}

void SdpServerHelper::parseCsd(int streamId, const uint8_t *data, int length) {
    auto iter = std::find_if(mStreams.begin(), mStreams.end(),
                             [streamId](auto item) { return item->getStreamId() == streamId; });

    if (iter == mStreams.end()) return;
    auto stream = *iter;
    stream->parseCsd(data, length);
}

int SdpServerHelper::getTimescale(int streamId) {
    auto iter = std::find_if(mStreams.begin(), mStreams.end(),
                             [streamId](auto item) { return item->getStreamId() == streamId; });

    if (iter == mStreams.end()) return 0;
    auto stream = *iter;
    return stream->getTimescale();
}

std::string SdpServerHelper::toString(std::string localIPAddr, uint16_t localPort) {
    char sdpStr[2048] = {0};

    std::string addrType;
    if (isIPv4(localIPAddr))
        addrType = "IP4";
    else if (isIPv6(localIPAddr))
        addrType = "IP6";
    else
        return "";

    int i = 0;
    i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "v=0\r\n");
    i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "o=- 0 0 IN %s %s\r\n", addrType.c_str(),
                       localIPAddr.c_str());
    i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "s=No Name\r\n");
    i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "c=IN %s %s\r\n", addrType.c_str(),
                       localIPAddr.c_str());
    i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "t=0 0\r\n");

    for (auto &iter : mStreams) {
        i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "m=%s 0 RTP/AVP %d\r\n",
                           iter->getMediaType().c_str(), iter->getPayloadType());
        i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i,
                           "a=control:rtsp://%s:%hu/%s/trackID=%d\r\n", localIPAddr.c_str(),
                           localPort, iter->getProgramName().c_str(), iter->getStreamId());

        if (iter->getEncodingName() == "mpeg4-generic") {
            auto stream = std::dynamic_pointer_cast<SdpServerMPEG4Stream>(iter);
            i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "a=rtpmap:%d %s/%d/%d\r\n",
                               stream->getPayloadType(), stream->getEncodingName().c_str(),
                               stream->getFrequency(), stream->getChannels());
            i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i,
                               "a=fmtp:%d "
                               "config=%s;profile-level-id=1;streamtype=5;mode=AAC-hbr;sizelength="
                               "13;indexlength=3;indexdeltalength=3\r\n",
                               stream->getPayloadType(), stream->getConfigString().c_str());
        } else if (iter->getEncodingName() == "H264") {
            auto stream = std::dynamic_pointer_cast<SdpServerH264Stream>(iter);
            i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "a=rtpmap:%d %s/%d\r\n",
                               stream->getPayloadType(), stream->getEncodingName().c_str(),
                               stream->getTimescale());
            i += std::snprintf(
                sdpStr + i, sizeof(sdpStr) - i,
                "a=fmtp:%d packetization-mode=1;profile-level-id=%s;sprop-parameter-sets=%s,%s\r\n",
                stream->getPayloadType(), stream->getProfileLevelIdString().c_str(),
                stream->getSpsString().c_str(), stream->getPpsString().c_str());
        } else if (iter->getEncodingName() == "H265") {
            auto stream = std::dynamic_pointer_cast<SdpServerHEVCStream>(iter);
            i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "a=rtpmap:%d %s/%d\r\n",
                               stream->getPayloadType(), stream->getEncodingName().c_str(),
                               stream->getTimescale());
            i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i,
                               "a=fmtp:%d sprop-vps=%s;sprop-sps=%s;sprop-pps=%s\r\n",
                               stream->getPayloadType(), stream->getVpsString().c_str(),
                               stream->getSpsString().c_str(), stream->getPpsString().c_str());
        }
    }

    i += std::snprintf(sdpStr + i, sizeof(sdpStr) - i, "\r\n");

    return std::string(sdpStr);
}