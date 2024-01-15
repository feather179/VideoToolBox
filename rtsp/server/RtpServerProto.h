#ifndef RTP_SERVER_PROTO_H
#define RTP_SERVER_PROTO_H

#include "foundation/FFBuffer.h"

#include <cstdint>

#include <memory>
#include <string>
#include <vector>

class RtpServerBaseProto {
protected:
    // 12 bytes
    struct RtpHeader {
        uint8_t cc : 4;
        uint8_t ext : 1;
        uint8_t padding : 1;
        uint8_t version : 2;
        uint8_t payload : 7;
        uint8_t marker : 1;
        uint16_t seq;
        uint32_t timestamp;
        uint32_t ssrc;
    };

    // MTU: 1500 bytes
    // IP Header: 20 bytes
    // UDP Header: 8 bytes -> max frame size = 1472 bytes
    // RTP Header: 12 bytes -> max payload size = 1460 bytes
    const static int RTP_HEADER_SIZE = 12;
    const static int RTP_MAX_FRAME_SIZE = 1472;
    const static int RTP_MAX_PAYLOAD_SIZE = 1460;

    int mPayloadType;

    uint8_t *mpBuffer;
    bool mIsFirstPack;
    bool mIsLastPack;
    uint32_t mOffset;
    uint32_t mPayloadSize;

    uint16_t mSeqNum;
    uint32_t mSSRC;
    uint32_t mBaseTimestamp;
    int64_t mPacketTimestamp;

public:
    RtpServerBaseProto();
    RtpServerBaseProto(const RtpServerBaseProto &) = delete;
    RtpServerBaseProto &operator=(const RtpServerBaseProto &) = delete;
    virtual ~RtpServerBaseProto();

    void prepareInternal();
    virtual void parseCsd(const uint8_t *data, int length) = 0;
    virtual void prepare(std::shared_ptr<AVPacketBuffer> packetBuffer) = 0;
    virtual int buildRtpPackage(uint8_t **out) = 0;
    int buildRtcpPakcage();
};

class RtpServerAACProto : public RtpServerBaseProto {
private:
    const uint8_t *mpData;
    int mSize;

public:
    static std::string MIME;

    explicit RtpServerAACProto(int payloadType);
    RtpServerAACProto(const RtpServerAACProto &) = delete;
    RtpServerAACProto &operator=(const RtpServerAACProto &) = delete;
    virtual ~RtpServerAACProto();

    virtual void parseCsd(const uint8_t *data, int length) override;
    virtual void prepare(std::shared_ptr<AVPacketBuffer> packetBuffer) override;
    virtual int buildRtpPackage(uint8_t **out) override;
};

class RtpServerH264Proto : public RtpServerBaseProto {
private:
    enum RtpPayloadType {
        RTP_PAYLOAD_STAP_A = 24,
        RTP_PAYLOAD_STAP_B,
        RTP_PAYLOAD_MTAP16,
        RTP_PAYLOAD_MTAP24,
        RTP_PAYLOAD_FU_A,
        RTP_PAYLOAD_FU_B,
    };

    // 1 byte
    struct RtpPayloadHeader {
        uint8_t type : 5;
        uint8_t nri : 2;
        uint8_t f : 1;
    };

#pragma pack(1)
    // 2 bytes
    struct FUAHeader {
        RtpPayloadHeader indicator;
        uint8_t type : 5;
        uint8_t f : 1;
        uint8_t end : 1;
        uint8_t start : 1;
    };
#pragma pack()

    std::vector<std::pair<int, int>> mNalus; // offset-length
    std::vector<std::pair<int, int>>::const_iterator mCurrNalu;
    const uint8_t *mpData;
    uint8_t mCurrNaluType;
    uint8_t mCurrNRI; // nal_ref_idc

    int mNaluLengthSize; // AVCDecoderConfigurationRecord
    std::vector<uint8_t> mSps;
    std::vector<uint8_t> mPps;

public:
    static std::string MIME;

    explicit RtpServerH264Proto(int payloadType);
    RtpServerH264Proto(const RtpServerH264Proto &) = delete;
    RtpServerH264Proto &operator=(const RtpServerH264Proto &) = delete;
    virtual ~RtpServerH264Proto();

    virtual void parseCsd(const uint8_t *data, int length) override;
    virtual void prepare(std::shared_ptr<AVPacketBuffer> packetBuffer) override;
    virtual int buildRtpPackage(uint8_t **out) override;
};

class RtpServerHEVCProto : public RtpServerBaseProto {
private:
    enum RtpPayloadType {
        RTP_PAYLOAD_APS = 48,
        RTP_PAYLOAD_FU,
    };

    // 2 bytes
    struct RtpPayloadHeader {
        uint16_t tid : 3;
        uint16_t layer : 6;
        uint16_t type : 6;
        uint16_t f : 1;
    };

#pragma pack(1)
    // 3 bytes
    struct FUHeader {
        RtpPayloadHeader indicator;
        uint8_t type : 6;
        uint8_t end : 1;
        uint8_t start : 1;
    };
#pragma pack()

    std::vector<std::pair<int, int>> mNalus; // offset-length
    std::vector<std::pair<int, int>>::const_iterator mCurrNalu;
    const uint8_t *mpData;
    uint8_t mCurrNaluType;
    uint8_t mCurrTid;

    int mNaluLengthSize; // HEVCDecoderConfigurationRecord
    std::vector<uint8_t> mVps;
    std::vector<uint8_t> mSps;
    std::vector<uint8_t> mPps;
    std::vector<uint8_t> mSei;

public:
    static std::string MIME;

    explicit RtpServerHEVCProto(int payloadType);
    RtpServerHEVCProto(const RtpServerHEVCProto &) = delete;
    RtpServerHEVCProto &operator=(const RtpServerHEVCProto &) = delete;
    virtual ~RtpServerHEVCProto();

    virtual void parseCsd(const uint8_t *data, int length) override;
    virtual void prepare(std::shared_ptr<AVPacketBuffer> packetBuffer) override;
    virtual int buildRtpPackage(uint8_t **out) override;
};

#endif
