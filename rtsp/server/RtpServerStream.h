#ifndef RTP_SERVER_STREAM_H
#define RTP_SERVER_STREAM_H

#include "RtpServerProto.h"
#include "foundation/Utils.h"
#include "foundation/FFBuffer.h"

#include <string>
#include <memory>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

class RtpServerStream {
private:
    int mStreamId;
    int mPayloadType;
    bool mIsSkipAdtsHeader;
    MediaCodecType mMediaType;
    std::string mMime;
    std::string mRemoteIpAddr;

    SOCKET mRtpSocket;
    SOCKADDR_IN mRemoteRtpAddr;
    uint16_t mLocalRtpPort;

    SOCKET mRtcpSocket;
    SOCKADDR_IN mRemoteRtcpAddr;
    uint16_t mLocalRtcpPort;

    std::shared_ptr<RtpServerBaseProto> mRtpProto;

public:
    RtpServerStream(int streamId, int payloadType, MediaCodecType mediaType, std::string mime);
    RtpServerStream(const RtpServerStream &) = delete;
    RtpServerStream &operator=(const RtpServerStream &) = delete;
    virtual ~RtpServerStream();

    bool init(std::string ipAddr, uint16_t rtpPort, uint16_t rtcpPort);
    void parseCsd(const uint8_t *data, int length);

    void skipAdtsHeader();
    void sendCsd();
    void sendPacket(std::shared_ptr<AVPacketBuffer> packetBuffer);

    MediaCodecType getMediaType() const { return mMediaType; }
    uint16_t getRtpPort() const { return mLocalRtpPort; }
    uint16_t getRtcpPort() const { return mLocalRtcpPort; }
};

#endif
