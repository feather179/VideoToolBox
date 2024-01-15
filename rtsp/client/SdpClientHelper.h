#ifndef SDP_CLIENT_HELPER_H
#define SDP_CLIENT_HELPER_H

#include <string>
#include <memory>
#include <vector>

class SdpClientBaseStream {
    friend class SdpClientHelper;

private:
    std::shared_ptr<SdpClientBaseStream> mStream;
    std::vector<std::string> mAttrs;

    void setStreamId(int streamId) { mStreamId = streamId; }
    void setMediaName(std::string name) { mMediaName = name; }
    void setProtocol(std::string protocol) { mProtocol = protocol; }
    void setPayloadType(int pt) { mPayloadType = pt; }
    void addAttribute(std::string attr);
    bool init();

protected:
    int mStreamId;
    std::string mMediaName;
    std::string mProtocol;
    std::string mControlUrl;
    int mPayloadType;
    int mClockRate;

public:
    SdpClientBaseStream();
    SdpClientBaseStream(const SdpClientBaseStream &) = delete;
    SdpClientBaseStream &operator=(const SdpClientBaseStream &) = delete;
    virtual ~SdpClientBaseStream();

    virtual void parseControl(std::string control);
    virtual void parseRtpmap(std::string rtpmap) {}
    virtual void parseFmtp(std::string fmtp) {}

    int getStreamId() const { return mStreamId; }
    std::string getMediaName() const { return mMediaName; }
    std::string getProtocol() const { return mProtocol; }
    std::string getControlUrl() const { return mControlUrl; }
    int getPayloadType() const { return mPayloadType; }
    virtual std::string getMime() const { return ""; }
};

class SdpClientMPEG4Stream : public SdpClientBaseStream {
private:
    int mChannels;
    std::string mMode; // e.g. AAC-hbr
    int mSizeLength;

public:
    const static std::string MIME;

    SdpClientMPEG4Stream();
    SdpClientMPEG4Stream(const SdpClientMPEG4Stream &) = delete;
    SdpClientMPEG4Stream &operator=(const SdpClientMPEG4Stream &) = delete;
    virtual ~SdpClientMPEG4Stream();

    virtual void parseRtpmap(std::string rtpmap) override;
    virtual void parseFmtp(std::string fmtp) override;
    virtual std::string getMime() const override { return MIME; }
    int getSizeLength() { return mSizeLength; }
};

class SdpClientLATMStream : public SdpClientBaseStream {
public:
    const static std::string MIME;

    SdpClientLATMStream();
    SdpClientLATMStream(const SdpClientLATMStream &) = delete;
    SdpClientLATMStream &operator=(const SdpClientLATMStream &) = delete;
    virtual ~SdpClientLATMStream();

    virtual void parseRtpmap(std::string rtpmap) override;
    virtual void parseFmtp(std::string fmtp) override;
    virtual std::string getMime() const override { return MIME; }
};

class SdpClientH264Stream : public SdpClientBaseStream {
private:
    std::vector<uint8_t> mSps;
    std::vector<uint8_t> mPps;
    uint8_t mProfileIdc;
    uint8_t mProfileIop;
    uint8_t mLevelIdc;
    int mPacketizationMode;

public:
    const static std::string MIME;

    SdpClientH264Stream();
    SdpClientH264Stream(const SdpClientH264Stream &) = delete;
    SdpClientH264Stream &operator=(const SdpClientH264Stream &) = delete;
    virtual ~SdpClientH264Stream();

    virtual void parseRtpmap(std::string rtpmap) override;
    virtual void parseFmtp(std::string fmtp) override;
    virtual std::string getMime() const override { return MIME; }
    int getCSD(std::vector<uint8_t> &csd);
};

class SdpClientHEVCStream : public SdpClientBaseStream {
private:
    std::vector<uint8_t> mSps;
    std::vector<uint8_t> mPps;
    std::vector<uint8_t> mVps;

public:
    const static std::string MIME;

    SdpClientHEVCStream();
    SdpClientHEVCStream(const SdpClientHEVCStream &) = delete;
    SdpClientHEVCStream &operator=(const SdpClientHEVCStream &) = delete;
    virtual ~SdpClientHEVCStream();

    virtual void parseRtpmap(std::string rtpmap) override;
    virtual void parseFmtp(std::string fmtp) override;
    virtual std::string getMime() const override { return MIME; }
    int getCSD(std::vector<uint8_t> &csd);
};

class SdpClientHelper {
private:
    int mVersion;
    int mSessId;
    int mSessVersion;
    int mStartTime;
    int mStopTime;
    int mNextStreamId;
    std::string mSessName;
    std::string mUserName;
    std::string mNetType;
    std::string mAddrType;
    std::string mAddress;
    std::vector<std::shared_ptr<SdpClientBaseStream>> mStreams;

public:
    SdpClientHelper();
    SdpClientHelper(const SdpClientHelper &) = delete;
    SdpClientHelper &operator=(const SdpClientHelper &) = delete;
    ~SdpClientHelper();

    bool parseSdp(std::string sdpStr);
    void copySdpStreams(std::vector<std::shared_ptr<SdpClientBaseStream>> &streams);
};

#endif
