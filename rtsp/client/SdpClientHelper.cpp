#include "SdpClientHelper.h"
#include "foundation/Base64.h"
#include "foundation/Log.h"

#include <sstream>

SdpClientBaseStream::SdpClientBaseStream() : mStreamId(0), mPayloadType(0), mClockRate(0) {}

SdpClientBaseStream::~SdpClientBaseStream() {}

void SdpClientBaseStream::addAttribute(std::string attr) {
    mAttrs.emplace_back(attr);
}

bool SdpClientBaseStream::init() {
    char mime[32] = {'\0'};
    bool findMime = false;
    for (auto &attr : mAttrs) {
        if (attr.starts_with("rtpmap:")) {
            if (std::sscanf(attr.c_str(), "rtpmap%*[^ ] %[^/]/", mime)) {
                findMime = true;
                break;
            }
        }
    }

    if (!findMime) return false;

    std::string mimeStr = mime;
    if (SdpClientMPEG4Stream::MIME.find(mimeStr) != std::string::npos) {
        mStream = std::make_shared<SdpClientMPEG4Stream>();
    } else if (SdpClientLATMStream::MIME.find(mimeStr) != std::string::npos) {
        mStream = std::make_shared<SdpClientLATMStream>();
    } else if (SdpClientH264Stream::MIME.find(mimeStr) != std::string::npos) {
        mStream = std::make_shared<SdpClientH264Stream>();
    } else if (SdpClientHEVCStream::MIME.find(mimeStr) != std::string::npos) {
        mStream = std::make_shared<SdpClientHEVCStream>();
    }

    if (!mStream) return false;

    mStream->setStreamId(mStreamId);
    mStream->setMediaName(mMediaName);
    mStream->setProtocol(mProtocol);
    mStream->setPayloadType(mPayloadType);

    for (auto &attr : mAttrs) {
        if (attr.starts_with("control:")) {
            mStream->parseControl(attr);
        } else if (attr.starts_with("rtpmap:")) {
            mStream->parseRtpmap(attr);
        } else if (attr.starts_with("fmtp:")) {
            mStream->parseFmtp(attr);
        }
    }

    return true;
}

void SdpClientBaseStream::parseControl(std::string control) {
    char url[256] = {'\0'};
    if (std::sscanf(control.c_str(), "control:%s", url) != 1) return;
    mControlUrl = url;
}

const std::string SdpClientMPEG4Stream::MIME = "mpeg4-generic";

SdpClientMPEG4Stream::SdpClientMPEG4Stream() : mChannels(0), mSizeLength(0) {}

SdpClientMPEG4Stream::~SdpClientMPEG4Stream() {}

void SdpClientMPEG4Stream::parseRtpmap(std::string rtpmap) {
    int pt, clockRate, channels;
    if (std::sscanf(rtpmap.c_str(), "rtpmap:%d %*[^/]/%d/%d", &pt, &clockRate, &channels) != 3) {
        LOGE("SdpClientMPEG4Stream failed to parse rtpmap:%s\n", rtpmap.c_str());
        return;
    }

    if (pt != mPayloadType) {
        LOGE("SdpClientMPEG4Stream rtpmap invalidate payload type:%d\n", pt);
        return;
    }
    mClockRate = clockRate;
    mChannels = channels;
}

void SdpClientMPEG4Stream::parseFmtp(std::string fmtp) {
    int pos = fmtp.find_first_of(' ');
    if (pos < 0) return;
    pos += 1;

    std::stringstream ss(fmtp.substr(pos, fmtp.size() - pos));
    std::string token;
    while (std::getline(ss, token, ' ')) {
        if (token.starts_with("config")) {

        } else if (token.starts_with("indexdeltalength")) {

        } else if (token.starts_with("indexlength")) {

        } else if (token.starts_with("mode")) {
            char mode[32] = {'\0'};
            if (std::sscanf(token.c_str(), "mode=%s", mode) != 1) return;
            mMode = mode;
        } else if (token.starts_with("profile-level-id")) {

        } else if (token.starts_with("sizelength")) {
            if (std::sscanf(token.c_str(), "sizelength=%d", &mSizeLength) != 1) return;
        } else if (token.starts_with("streamtype")) {
        }
    }
}

const std::string SdpClientLATMStream::MIME = "MP4A-LATM";

SdpClientLATMStream::SdpClientLATMStream() {}

SdpClientLATMStream::~SdpClientLATMStream() {}

void SdpClientLATMStream::parseRtpmap(std::string rtpmap) {}

void SdpClientLATMStream::parseFmtp(std::string fmtp) {}

const std::string SdpClientH264Stream::MIME = "H264;AVC";

SdpClientH264Stream::SdpClientH264Stream()
    : mProfileIdc(0), mProfileIop(0), mLevelIdc(0), mPacketizationMode(0) {}

SdpClientH264Stream::~SdpClientH264Stream() {}

void SdpClientH264Stream::parseRtpmap(std::string rtpmap) {
    int pt, clockRate;
    if (std::sscanf(rtpmap.c_str(), "rtpmap:%d %*[^/]/%d", &pt, &clockRate) != 2) {
        LOGE("SdpClientH264Stream failed to parse rtpmap:%s\n", rtpmap.c_str());
        return;
    }

    if (pt != mPayloadType) {
        LOGE("SdpClientH264Stream rtpmap invalidate payload type:%d\n", pt);
        return;
    }
    mClockRate = clockRate;
}

void SdpClientH264Stream::parseFmtp(std::string fmtp) {
    int pos = fmtp.find_first_of(' ');
    if (pos < 0) return;
    pos += 1;

    std::stringstream ss(fmtp.substr(pos, fmtp.size() - pos));
    std::string token;
    while (std::getline(ss, token, ' ')) {
        if (token.starts_with("packetization-mode")) {
            int mode;
            if (std::sscanf(token.c_str(), "packetization-mode=%d", &mode) != 1) return;
            mPacketizationMode = mode;
        } else if (token.starts_with("profile-level-id")) {
            int profileIdc, profileIop, levelIdc;
            if (std::sscanf(token.c_str(), "profile-level-id=%2d%2d%2d", &profileIdc, &profileIop,
                            &levelIdc) != 3)
                return;
            mProfileIdc = profileIdc;
            mProfileIop = profileIop;
            mLevelIdc = levelIdc;
        } else if (token.starts_with("sprop-parameter-sets")) {
            char param[256];
            if (std::sscanf(token.c_str(), "sprop-parameter-sets=%s", param) != 1) return;
            size_t spsSize = 0, ppsSize = 0, ppsOffset = 0;
            char *p = param;
            while (*p++ != ';') ++spsSize;
            param[spsSize] = '\0';
            ppsOffset = spsSize + 1;
            p = param + ppsOffset;
            while (*p++ != '\0') ++ppsSize;

            uint8_t csd[256];
            size_t csdSize = sizeof(csd);
            decodeBase64(param, &csdSize, csd);
            mSps.insert(mSps.end(), csd, csd + csdSize);

            csdSize = sizeof(csd);
            decodeBase64(param + ppsOffset, &csdSize, csd);
            mPps.insert(mPps.end(), csd, csd + csdSize);
        }
    }
}

int SdpClientH264Stream::getCSD(std::vector<uint8_t> &csd) {
    uint8_t startCode[] = {0x00, 0x00, 0x00, 0x01};

    csd.insert(csd.end(), startCode, startCode + sizeof(startCode));
    csd.insert(csd.end(), mSps.begin(), mSps.end());
    csd.insert(csd.end(), startCode, startCode + sizeof(startCode));
    csd.insert(csd.end(), mPps.begin(), mPps.end());

    return csd.size();
}

const std::string SdpClientHEVCStream::MIME = "H265;HEVC";

SdpClientHEVCStream::SdpClientHEVCStream() {}

SdpClientHEVCStream::~SdpClientHEVCStream() {}

void SdpClientHEVCStream::parseRtpmap(std::string rtpmap) {
    int pt, clockRate;
    if (std::sscanf(rtpmap.c_str(), "rtpmap:%d %*[^/]/%d", &pt, &clockRate) != 2) {
        LOGE("SdpClientHEVCStream failed to parse rtpmap:%s\n", rtpmap.c_str());
        return;
    }

    if (pt != mPayloadType) {
        LOGE("SdpClientHEVCStream rtpmap invalidate payload type:%d\n", pt);
        return;
    }
    mClockRate = clockRate;
}

void SdpClientHEVCStream::parseFmtp(std::string fmtp) {
    int pos = fmtp.find_first_of(' ');
    if (pos < 0) return;
    pos += 1;

    std::stringstream ss(fmtp.substr(pos, fmtp.size() - pos));
    std::string token;
    char param[256];
    uint8_t csd[256];
    size_t csdSize = sizeof(csd);
    while (std::getline(ss, token, ' ')) {
        if (token.ends_with(';')) token = token.substr(0, token.size() - 1);

        if (token.starts_with("sprop-sps")) {
            if (std::sscanf(token.c_str(), "sprop-sps=%s", param) != 1) return;
            csdSize = sizeof(csd);
            decodeBase64(param, &csdSize, csd);
            mSps.insert(mSps.end(), csd, csd + csdSize);
        } else if (token.starts_with("sprop-pps")) {
            if (std::sscanf(token.c_str(), "sprop-pps=%s", param) != 1) return;
            csdSize = sizeof(csd);
            decodeBase64(param, &csdSize, csd);
            mPps.insert(mPps.end(), csd, csd + csdSize);
        } else if (token.starts_with("sprop-vps")) {
            if (std::sscanf(token.c_str(), "sprop-vps=%s", param) != 1) return;
            csdSize = sizeof(csd);
            decodeBase64(param, &csdSize, csd);
            mVps.insert(mVps.end(), csd, csd + csdSize);
        }
    }
}

int SdpClientHEVCStream::getCSD(std::vector<uint8_t> &csd) {
    uint8_t startCode[] = {0x00, 0x00, 0x00, 0x01};

    csd.insert(csd.end(), startCode, startCode + sizeof(startCode));
    csd.insert(csd.end(), mSps.begin(), mSps.end());
    csd.insert(csd.end(), startCode, startCode + sizeof(startCode));
    csd.insert(csd.end(), mPps.begin(), mPps.end());
    csd.insert(csd.end(), startCode, startCode + sizeof(startCode));
    csd.insert(csd.end(), mVps.begin(), mVps.end());

    return csd.size();
}

SdpClientHelper::SdpClientHelper()
    : mVersion(0), mSessId(0), mSessVersion(0), mStartTime(0), mStopTime(0), mNextStreamId(0) {}

SdpClientHelper::~SdpClientHelper() {}

bool SdpClientHelper::parseSdp(std::string sdpStr) {
    std::stringstream ss(sdpStr);
    std::string token;
    while (std::getline(ss, token)) {
        switch (token[0]) {
            case 'a': {
                mStreams.back()->addAttribute(token.substr(2, token.size() - 3)); // a=...\r
                break;
            }
            case 'c': {
                break;
            }
            case 'm': {
                char name[32] = {'\0'};
                char protocol[32] = {'\0'};
                int port, pt;
                if (std::sscanf(token.c_str(), "m=%s %d %s %d\r", name, &port, protocol, &pt) != 4)
                    return false;
                auto stream = std::make_shared<SdpClientBaseStream>();
                stream->setStreamId(mNextStreamId++);
                stream->setMediaName(name);
                stream->setProtocol(protocol);
                stream->setPayloadType(pt);
                mStreams.emplace_back(stream);
                break;
            }
            case 'o': {
                int id, version;
                char name[128] = {'\0'};
                char nettype[32] = {'\0'};
                char addrtype[32] = {'\0'};
                char address[64] = {'\0'};
                if (std::sscanf(token.c_str(), "o=%s %d %d %s %s %s\r", name, &id, &version,
                                nettype, addrtype, address) != 6)
                    return false;
                mUserName = name;
                mSessId = id;
                mSessVersion = version;
                mNetType = nettype;
                mAddrType = addrtype;
                mAddress = address;
                break;
            }
            case 's': {
                mSessName = token.substr(2, token.size() - 3); // s=...\r
                break;
            }
            case 't': {
                int start, stop;
                if (std::sscanf(token.c_str(), "t=%d %d\r", &start, &stop) != 2) return false;
                mStartTime = start;
                mStopTime = stop;
                break;
            }
            case 'v': {
                if (std::sscanf(token.c_str(), "v=%d\r", &mVersion) != 1) return false;
                break;
            }
            default:
                break;
        }
    }

    for (auto &stream : mStreams) stream->init();

    return true;
}

void SdpClientHelper::copySdpStreams(std::vector<std::shared_ptr<SdpClientBaseStream>> &streams) {
    for (auto &stream : mStreams) streams.emplace_back(stream->mStream);
}
