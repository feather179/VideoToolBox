#include "RtpServerStream.h"
#include "foundation/Log.h"

RtpServerStream::RtpServerStream(int streamId,
                                 int payloadType,
                                 MediaCodecType mediaType,
                                 std::string mime)
    : mStreamId(streamId), mPayloadType(payloadType), mIsSkipAdtsHeader(false),
      mMediaType(mediaType), mMime(mime) {

    mRtpSocket = 0;
    mRemoteRtpAddr = {0};
    mLocalRtpPort = 0;

    mRtcpSocket = 0;
    mRemoteRtcpAddr = {0};
    mLocalRtcpPort = 0;
}

RtpServerStream::~RtpServerStream() {}

bool RtpServerStream::init(std::string ipAddr, uint16_t rtpPort, uint16_t rtcpPort) {
    bool initDone = false;

    mRemoteIpAddr = ipAddr;

    mRemoteRtpAddr.sin_family = AF_INET;
    mRemoteRtpAddr.sin_port = htons(rtpPort);
    inet_pton(AF_INET, mRemoteIpAddr.c_str(), &mRemoteRtpAddr.sin_addr);

    mRemoteRtcpAddr.sin_family = AF_INET;
    mRemoteRtcpAddr.sin_port = htons(rtcpPort);
    inet_pton(AF_INET, mRemoteIpAddr.c_str(), &mRemoteRtcpAddr.sin_addr);

    std::vector<SOCKET> socketList;

    while (true) {
        SOCKET rtpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SOCKADDR_IN rtpSendAddr;
        rtpSendAddr.sin_family = AF_INET;
        rtpSendAddr.sin_port = htons(0);
        rtpSendAddr.sin_addr.S_un.S_addr = INADDR_ANY;
        if (bind(rtpSocket, (SOCKADDR *)&rtpSendAddr, sizeof(rtpSendAddr)) == SOCKET_ERROR) {
            LOGE("%s Failed to bind rtp socket, error code:%d\n", __PRETTY_FUNCTION__,
                 WSAGetLastError());
            socketList.emplace_back(rtpSocket);
            initDone = false;
            break;
        }

        SOCKADDR_IN testAddr;
        int len = sizeof(testAddr);
        if (getsockname(rtpSocket, (SOCKADDR *)&testAddr, &len) == SOCKET_ERROR) {
            LOGE("%s Failed to get rtp socket name, error code:%d\n", __PRETTY_FUNCTION__,
                 WSAGetLastError());
            socketList.emplace_back(rtpSocket);
            initDone = false;
            break;
        }

        uint16_t port = ntohs(testAddr.sin_port);
        if (port & 0x1) {
            socketList.emplace_back(rtpSocket);
            continue;
        }

        SOCKET rtcpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SOCKADDR_IN rtcpSendAddr;
        rtcpSendAddr.sin_family = AF_INET;
        rtcpSendAddr.sin_port = htons(port + 1);
        rtcpSendAddr.sin_addr.S_un.S_addr = INADDR_ANY;
        if (bind(rtcpSocket, (SOCKADDR *)&rtcpSendAddr, sizeof(rtcpSendAddr)) == SOCKET_ERROR) {
            LOGE("%s Failed to bind rtcp socket, error code:%d\n", __PRETTY_FUNCTION__,
                 WSAGetLastError());
            socketList.emplace_back(rtpSocket);
            socketList.emplace_back(rtcpSocket);
            initDone = false;
            break;
        }

        mRtpSocket = rtpSocket;
        mRtcpSocket = rtcpSocket;
        mLocalRtpPort = port;
        mLocalRtcpPort = port + 1;
        initDone = true;
        break;
    }

    for (auto &sock : socketList) {
        closesocket(sock);
    }

    if (RtpServerAACProto::MIME.find(mMime) != std::string::npos) {
        mRtpProto = std::make_shared<RtpServerAACProto>(mPayloadType);
    } else if (RtpServerH264Proto::MIME.find(mMime) != std::string::npos) {
        mRtpProto = std::make_shared<RtpServerH264Proto>(mPayloadType);
    } else if (RtpServerHEVCProto::MIME.find(mMime) != std::string::npos) {
        mRtpProto = std::make_shared<RtpServerHEVCProto>(mPayloadType);
    }

    if (!mRtpProto) initDone = false;

    return initDone;
}

void RtpServerStream::parseCsd(const uint8_t *data, int length) {
    if (mRtpProto && data && length > 0) {
        mRtpProto->parseCsd(data, length);
    }
}

void RtpServerStream::skipAdtsHeader() {
    if (RtpServerAACProto::MIME.find(mMime) != std::string::npos) mIsSkipAdtsHeader = true;
}

void RtpServerStream::sendCsd() {}

void RtpServerStream::sendPacket(std::shared_ptr<AVPacketBuffer> packetBuffer) {
    // for AAC, ADTS header size = 7
    // if (mIsSkipAdtsHeader) {
    //	data += 7;
    //	length -= 7;
    //}
    mRtpProto->prepare(packetBuffer);

    int packetSize = 0;
    uint8_t *pData = nullptr;
    while (true) {
        packetSize = mRtpProto->buildRtpPackage(&pData);
        if (packetSize == 0) break;
        sendto(mRtpSocket, (const char *)pData, packetSize, 0, (SOCKADDR *)&mRemoteRtpAddr,
               sizeof(mRemoteRtpAddr));
    }
}
