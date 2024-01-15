#ifndef RTP_CLIENT_STREAM_H
#define RTP_CLIENT_STREAM_H

#include "SdpClientHelper.h"

#include <vector>
#include <thread>
#include <memory>
#include <functional>
#include <string>

#include <winsock2.h>

class RtpClientBaseProto {
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
        uint16_t timestamp;
        uint32_t ssrc;
    };

    std::vector<uint8_t> mData;
    std::shared_ptr<SdpClientBaseStream> mSdpStream;
    std::function<void(const uint8_t *, int)> mBufferReadyCB;

public:
    RtpClientBaseProto() = default;
    RtpClientBaseProto(const RtpClientBaseProto &) = delete;
    RtpClientBaseProto &operator=(const RtpClientBaseProto &) = delete;
    virtual ~RtpClientBaseProto() = default;
    virtual void processRtpPackage(const uint8_t *data, int length) = 0;
    void processRtcpPackage(const uint8_t *data, int length);

    void setSdpStream(std::shared_ptr<SdpClientBaseStream> stream) { mSdpStream = stream; }
    void setBufferReadyCB(std::function<void(const uint8_t *, int)> callback) {
        mBufferReadyCB = callback;
    }
};

class RtpClientMPEG4Proto : public RtpClientBaseProto {
public:
    const static std::string MIME;

    RtpClientMPEG4Proto() = default;
    virtual ~RtpClientMPEG4Proto() = default;
    virtual void processRtpPackage(const uint8_t *data, int length) override;
};

class RtpClientLATMProto : public RtpClientBaseProto {
public:
    const static std::string MIME;

    RtpClientLATMProto() = default;
    virtual ~RtpClientLATMProto() = default;
    virtual void processRtpPackage(const uint8_t *data, int length) override;
};

class RtpClientH264Proto : public RtpClientBaseProto {
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
    bool mSentCSD;

public:
    const static std::string MIME;

    RtpClientH264Proto();
    virtual ~RtpClientH264Proto();
    virtual void processRtpPackage(const uint8_t *data, int length) override;
};

class RtpClientHEVCProto : public RtpClientBaseProto {
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

    bool mSentCSD;

public:
    const static std::string MIME;

    RtpClientHEVCProto();
    virtual ~RtpClientHEVCProto();
    virtual void processRtpPackage(const uint8_t *data, int length) override;
};

class RtpClientStream {
private:
    int mStreamId;
    int mPayloadType;
    std::string mMime;
    std::string mServerAddr;
    std::string mProtocol;

    SOCKET mRtpSocket;
    uint16_t mRtpPort;
    SOCKET mRtcpSocket;
    uint16_t mRtcpPort;

    std::shared_ptr<RtpClientBaseProto> mRtpProto;

    std::unique_ptr<std::thread> mReceiveThread;
    void receiveThread();
    void createRtpSocket();

public:
    RtpClientStream(
        int streamId, int pt, std::string mime, std::string serverAddr, std::string protocol);
    RtpClientStream(const RtpClientStream &) = delete;
    RtpClientStream &operator=(const RtpClientStream &) = delete;
    ~RtpClientStream();

    bool init();
    void setSdpStream(std::shared_ptr<SdpClientBaseStream> stream);
    void setBufferReadyCB(std::function<void(const uint8_t *, int)> callback);

    int getStreamId() const { return mStreamId; }
    uint16_t getRtpPort() const { return mRtpPort; }
    uint16_t getRtcpPort() const { return mRtcpPort; }
};

// class RtpClientHelper {
// private:
//	int mNextStreamId;
//	std::vector<std::shared_ptr<RtpClientStream> > mStreams;
//
// public:
//	//int addStream();
//	//void setSdpStream(int streamId) { mStreams[streamId]->setSdpStream(); }
//	//uint16_t getRtpPort(int streamId) { return mStreams[streamId]->getRtpPort(); }
//	//uint16_t getRtcpPort(int streamId) { return mStreams[streamId]->getRtcpPort(); }
// };

#endif
