#ifndef SDP_SERVER_HELPER_H
#define SDP_SERVER_HELPER_H

#include "foundation/Utils.h"

#include <cstdint>

#include <string>
#include <vector>
#include <memory>

class SdpServerBaseStream {
protected:
    int mStreamId = 0;
    int mPayloadType = 0;
    int mTimescale = 0;
    MediaCodecType mMediaType = MEDIA_CODEC_TYPE_UNKNOWN;
    std::string mEncodingName; // mpeg4-generic / MP4A-LATM / H264, etc
    std::string mProgramName;

public:
    SdpServerBaseStream() = default;
    SdpServerBaseStream(const SdpServerBaseStream &) = delete;
    SdpServerBaseStream &operator=(const SdpServerBaseStream &) = delete;
    virtual ~SdpServerBaseStream() = default;

    virtual void parseCsd(const uint8_t *data, int length) = 0;

    int getStreamId() const { return mStreamId; }
    int getPayloadType() const { return mPayloadType; }
    int getTimescale() const { return mTimescale; }
    std::string getMediaType() const {
        if (mMediaType == MEDIA_CODEC_TYPE_AUDIO)
            return "audio";
        else if (mMediaType == MEDIA_CODEC_TYPE_VIDEO)
            return "video";
        return "";
    }
    std::string getEncodingName() const { return mEncodingName; }
    std::string getProgramName() const { return mProgramName; }
};

class SdpServerMPEG4Stream : public SdpServerBaseStream {
private:
    int mFrequency = 0;
    int mChannels = 0;
    std::string mConfigStr;
    std::string mObjectType;

public:
    static std::string MIME;

    SdpServerMPEG4Stream(int streamId, int payloadType, std::string programName);
    SdpServerMPEG4Stream(const SdpServerMPEG4Stream &) = delete;
    SdpServerMPEG4Stream &operator=(const SdpServerMPEG4Stream &) = delete;
    virtual ~SdpServerMPEG4Stream();

    virtual void parseCsd(const uint8_t *data, int length) override;

    int getFrequency() const { return mFrequency; }
    int getChannels() const { return mChannels; }
    std::string getConfigString() const { return mConfigStr; }
};

class SdpServerH264Stream : public SdpServerBaseStream {
private:
    std::vector<uint8_t> mSps;
    std::vector<uint8_t> mPps;

public:
    static std::string MIME;

    SdpServerH264Stream(int streamId, int payloadType, std::string programName);
    SdpServerH264Stream(const SdpServerH264Stream &) = delete;
    SdpServerH264Stream &operator=(const SdpServerH264Stream &) = delete;
    virtual ~SdpServerH264Stream();

    virtual void parseCsd(const uint8_t *data, int length) override;

    std::string getProfileLevelIdString();
    std::string getSpsString();
    std::string getPpsString();
};

class SdpServerHEVCStream : public SdpServerBaseStream {
private:
    std::vector<uint8_t> mVps;
    std::vector<uint8_t> mSps;
    std::vector<uint8_t> mPps;
    std::vector<uint8_t> mSei;

public:
    static std::string MIME;

    SdpServerHEVCStream(int streamId, int payloadType, std::string programName);
    SdpServerHEVCStream(const SdpServerHEVCStream &) = delete;
    SdpServerHEVCStream &operator=(const SdpServerHEVCStream &) = delete;
    virtual ~SdpServerHEVCStream();

    virtual void parseCsd(const uint8_t *data, int length) override;

    std::string getVpsString();
    std::string getSpsString();
    std::string getPpsString();
};

class SdpServerHelper {
private:
    std::vector<std::shared_ptr<SdpServerBaseStream>> mStreams;

public:
    SdpServerHelper();
    SdpServerHelper(const SdpServerHelper &) = delete;
    SdpServerHelper &operator=(const SdpServerHelper &) = delete;
    virtual ~SdpServerHelper();

    void addStream(int streamId, int payloadType, std::string mime, std::string programName);
    void parseCsd(int streamId, const uint8_t *data, int length);
    int getTimescale(int streamId);
    std::string toString(std::string localIPAddr, uint16_t localPort);
};

#endif
