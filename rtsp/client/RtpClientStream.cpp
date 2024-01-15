#include "RtpClientStream.h"
#include "foundation/BitReader.h"
#include "foundation/BitWriter.h"
#include "foundation/Log.h"

void RtpClientBaseProto::processRtcpPackage(const uint8_t *data, int length) {}

const std::string RtpClientMPEG4Proto::MIME = "mpeg4-generic";

void RtpClientMPEG4Proto::processRtpPackage(const uint8_t *data, int length) {
    std::shared_ptr<SdpClientMPEG4Stream> stream =
        std::dynamic_pointer_cast<SdpClientMPEG4Stream>(mSdpStream);
    int sizeLength = stream->getSizeLength();
    int headerLength = sizeof(RtpHeader);

    data += headerLength;
    length -= headerLength;

    BitReader br(data, length);
    int auHeaderLength = br.getBits(16);
    int auHeaderCount = auHeaderLength / 16;
    std::vector<int> auSizes;
    for (int i = 0; i < auHeaderCount; ++i) {
        int size = br.getBits(sizeLength);
        auSizes.emplace_back(size);
        br.skipBits(16 - sizeLength);
    }

    data += 2;
    length -= 2;
    data += auHeaderCount * 2;
    length -= auHeaderCount * 2;

    for (int i = 0; i < auHeaderCount; ++i) {
        mData.insert(mData.end(), data, data + auSizes[i]);
        data += auSizes[i];
        length -= auSizes[i];
        if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
        mData.clear();
    }
}

const std::string RtpClientLATMProto::MIME = "MP4A-LATM";

void RtpClientLATMProto::processRtpPackage(const uint8_t *data, int length) {
    int size = 0;
    int headerLength = sizeof(RtpHeader);

    data += headerLength;
    length -= headerLength;

    while (*data == 0xFF && length > 0) {
        size += *data;
        ++data;
        --length;
    }
    size += *data;
    ++data;
    --length;
    mData.insert(mData.end(), data, data + size);
    if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
    mData.clear();
}

const std::string RtpClientH264Proto::MIME = "H264;AVC";

RtpClientH264Proto::RtpClientH264Proto() : mSentCSD(false) {}

RtpClientH264Proto::~RtpClientH264Proto() {}

void RtpClientH264Proto::processRtpPackage(const uint8_t *data, int length) {
    const static uint8_t START_CODE[] = {0x00, 0x00, 0x00, 0x01};

    if (!mSentCSD) [[unlikely]] {
        std::shared_ptr<SdpClientH264Stream> stream =
            std::dynamic_pointer_cast<SdpClientH264Stream>(mSdpStream);
        std::vector<uint8_t> csd;
        stream->getCSD(csd);
        mData.insert(mData.end(), csd.begin(), csd.end());
        if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
        mData.clear();
        mSentCSD = true;
    }

    int headerLength = sizeof(RtpHeader);

    data += headerLength;
    length -= headerLength;

    BitReader br(data, length);
    // payload header
    br.skipBits(1); // F bit
    uint8_t nri = br.getBits(2);
    uint8_t type = br.getBits(5);

    if (type >= 1 && type < RTP_PAYLOAD_STAP_A) { // single NAL unit packet
        mData.insert(mData.end(), START_CODE, START_CODE + sizeof(START_CODE));
        mData.insert(mData.end(), data, data + length);
        if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
        mData.clear();
    } else {
        switch (type) {
            case RTP_PAYLOAD_STAP_A: {
                data += sizeof(RtpPayloadHeader);
                length -= sizeof(RtpPayloadHeader);
                while (length > 0) {
                    BitReader br(data, length);
                    int nalSize = br.getBits(16);
                    data += 2;
                    length -= 2;
                    mData.insert(mData.end(), START_CODE, START_CODE + sizeof(START_CODE));
                    mData.insert(mData.end(), data, data + nalSize);
                    data += nalSize;
                    length -= nalSize;
                }
                if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
                mData.clear();
                break;
            }
            case RTP_PAYLOAD_FU_A: {
                uint8_t start = br.getBits(1);
                uint8_t end = br.getBits(1);
                br.skipBits(1); // F bit
                uint8_t naluType = br.getBits(5);
                uint8_t naluHeader[1] = {0};
                {
                    BitWriter bw(naluHeader, sizeof(naluHeader));
                    bw.putBits(0, 1);
                    bw.putBits(nri, 2);
                    bw.putBits(naluType, 5);
                    bw.flush();
                }
                data += sizeof(FUAHeader);
                length -= sizeof(FUAHeader);

                if (start) {
                    mData.insert(mData.end(), START_CODE, START_CODE + sizeof(START_CODE));
                    mData.insert(mData.end(), naluHeader, naluHeader + sizeof(naluHeader));
                }
                mData.insert(mData.end(), data, data + length);
                if (end) {
                    if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
                    mData.clear();
                }
                break;
            }
            default:
                break;
        }
    }
}

const std::string RtpClientHEVCProto::MIME = "H265;HEVC";

RtpClientHEVCProto::RtpClientHEVCProto() : mSentCSD(false) {}

RtpClientHEVCProto::~RtpClientHEVCProto() {}

void RtpClientHEVCProto::processRtpPackage(const uint8_t *data, int length) {
    const static uint8_t START_CODE[] = {0x00, 0x00, 0x00, 0x01};

    if (!mSentCSD) [[unlikely]] {
        std::shared_ptr<SdpClientHEVCStream> stream =
            std::dynamic_pointer_cast<SdpClientHEVCStream>(mSdpStream);
        std::vector<uint8_t> csd;
        stream->getCSD(csd);
        mData.insert(mData.end(), csd.begin(), csd.end());
        if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
        mData.clear();
        mSentCSD = true;
    }

    int headerLength = sizeof(RtpHeader);

    data += headerLength;
    length -= headerLength;

    BitReader br(data, length);
    // payload header
    br.skipBits(1); // F bit
    uint8_t type = br.getBits(6);
    br.skipBits(6); // layer
    uint8_t tid = br.getBits(3);

    if (type >= 0 && type < RTP_PAYLOAD_APS) {
        mData.insert(mData.end(), START_CODE, START_CODE + sizeof(START_CODE));
        mData.insert(mData.end(), data, data + length);
        if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
        mData.clear();
    } else {
        switch (type) {
            case RTP_PAYLOAD_APS: {
                data += sizeof(RtpPayloadHeader);
                length -= sizeof(RtpPayloadHeader);
                while (length > 0) {
                    BitReader br(data, length);
                    int nalSize = br.getBits(16);
                    data += 2;
                    length -= 2;
                    mData.insert(mData.end(), START_CODE, START_CODE + sizeof(START_CODE));
                    mData.insert(mData.end(), data, data + nalSize);
                    data += nalSize;
                    length -= nalSize;
                }
                if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
                mData.clear();
                break;
            }
            case RTP_PAYLOAD_FU: {
                uint8_t start = br.getBits(1);
                uint8_t end = br.getBits(1);
                uint8_t naluType = br.getBits(6);
                uint8_t naluHeader[2] = {0};
                {
                    BitWriter bw(naluHeader, sizeof(naluHeader));
                    bw.putBits(0, 1);
                    bw.putBits(naluType, 6);
                    bw.putBits(0, 6);
                    bw.putBits(tid, 3);
                    bw.flush();
                }
                data += sizeof(FUHeader);
                length -= sizeof(FUHeader);

                if (start) {
                    mData.insert(mData.end(), START_CODE, START_CODE + sizeof(START_CODE));
                    mData.insert(mData.end(), naluHeader, naluHeader + sizeof(naluHeader));
                }
                mData.insert(mData.end(), data, data + length);
                if (end) {
                    if (mBufferReadyCB) mBufferReadyCB(mData.data(), mData.size());
                    mData.clear();
                }
                break;
            }
            default:
                break;
        }
    }
}

RtpClientStream::RtpClientStream(
    int streamId, int pt, std::string mime, std::string serverAddr, std::string protocol)
    : mStreamId(streamId), mPayloadType(pt), mMime(mime), mServerAddr(serverAddr),
      mProtocol(protocol), mRtpSocket(0), mRtpPort(0), mRtcpSocket(0), mRtcpPort(0) {}

RtpClientStream::~RtpClientStream() {
    if (mReceiveThread && mReceiveThread->joinable()) mReceiveThread->join();
}

void RtpClientStream::createRtpSocket() {
    std::vector<SOCKET> sockets;

    while (true) {
        SOCKET rtpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SOCKADDR_IN rtpListenAddr;
        rtpListenAddr.sin_family = AF_INET;
        rtpListenAddr.sin_port = htons(0);
        rtpListenAddr.sin_addr.S_un.S_addr = INADDR_ANY;
        if (bind(rtpSocket, (SOCKADDR *)&rtpListenAddr, sizeof(rtpListenAddr)) == SOCKET_ERROR) {
            LOGE("createRtpSocket Failed to bind rtp socket, error code:%d\n", WSAGetLastError());
            sockets.emplace_back(rtpSocket);
            break;
        }
        SOCKADDR testAddr;
        int len = sizeof(testAddr);
        if (getsockname(rtpSocket, (SOCKADDR *)&testAddr, &len) == SOCKET_ERROR) {
            LOGE("createRtpSocket Failed to get socket name, error code:%d\n", WSAGetLastError());
            sockets.emplace_back(rtpSocket);
            break;
        }
        uint16_t port = ntohs(((SOCKADDR_IN *)(&testAddr))->sin_port);
        if (port & 0x1) {
            sockets.emplace_back(rtpSocket);
            continue;
        }

        SOCKET rtcpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SOCKADDR_IN rtcpListenAddr;
        rtcpListenAddr.sin_family = AF_INET;
        rtcpListenAddr.sin_port = htons(port + 1);
        rtcpListenAddr.sin_addr.S_un.S_addr = INADDR_ANY;
        if (bind(rtcpSocket, (SOCKADDR *)&rtcpListenAddr, sizeof(rtcpListenAddr)) == SOCKET_ERROR) {
            LOGE("createRtpSocket Failed to bind rtcp socket, error code:%d\n", WSAGetLastError());
            sockets.emplace_back(rtpSocket);
            sockets.emplace_back(rtcpSocket);
            continue;
        }

        mRtpSocket = rtpSocket;
        mRtpPort = port;
        mRtcpSocket = rtcpSocket;
        mRtcpPort = port + 1;
        break;
    }

    for (auto &sock : sockets) closesocket(sock);
}

bool RtpClientStream::init() {
    if (RtpClientMPEG4Proto::MIME.find(mMime) != std::string::npos) {
        mRtpProto = std::make_shared<RtpClientMPEG4Proto>();
    } else if (RtpClientLATMProto::MIME.find(mMime) != std::string::npos) {
        mRtpProto = std::make_shared<RtpClientLATMProto>();
    } else if (RtpClientH264Proto::MIME.find(mMime) != std::string::npos) {
        mRtpProto = std::make_shared<RtpClientH264Proto>();
    } else if (RtpClientHEVCProto::MIME.find(mMime) != std::string::npos) {
        mRtpProto = std::make_shared<RtpClientHEVCProto>();
    }

    if (!mRtpProto) return false;

    if (mProtocol == "RTP/AVP" || mProtocol == "RTP/AVP/UDP") {
        createRtpSocket();
    } else if (mProtocol == "RTP/AVP/TCP") {
    }

    mReceiveThread = std::make_unique<std::thread>(&RtpClientStream::receiveThread, this);

    return true;
}

void RtpClientStream::setSdpStream(std::shared_ptr<SdpClientBaseStream> stream) {
    if (mRtpProto) mRtpProto->setSdpStream(stream);
}

void RtpClientStream::setBufferReadyCB(std::function<void(const uint8_t *, int)> callback) {
    if (mRtpProto) mRtpProto->setBufferReadyCB(callback);
}

void RtpClientStream::receiveThread() {
    int maxSocket = mRtpSocket > mRtcpSocket ? mRtpSocket : mRtcpSocket;
    SOCKADDR_IN senderAddr;
    int senderLen = sizeof(SOCKADDR);
    int recvBufLength = 1024 * 2;
    char *pRecvBuf = new char[recvBufLength];

    FD_SET fds;
    while (true) {
        FD_ZERO(&fds);
        FD_SET(mRtpSocket, &fds);
        FD_SET(mRtcpSocket, &fds);

        if (select(maxSocket + 1, &fds, nullptr, nullptr, nullptr) == SOCKET_ERROR) {
            LOGE("receiveThread Failed to select rtp socket, error code:%d\n", WSAGetLastError());
            break;
        }

        if (FD_ISSET(mRtpSocket, &fds)) {
            int recvLen = recvfrom(mRtpSocket, pRecvBuf, recvBufLength, 0, (SOCKADDR *)&senderAddr,
                                   &senderLen);
            if (recvLen == SOCKET_ERROR || recvLen == 0) {
                LOGE("receiveThread failed to receive from rtp socket, error code:%d\n",
                     WSAGetLastError());
                break;
            }
            if (mRtpProto) mRtpProto->processRtpPackage((const uint8_t *)pRecvBuf, recvLen);
        }

        if (FD_ISSET(mRtcpSocket, &fds)) {
            int recvLen = recvfrom(mRtcpSocket, pRecvBuf, recvBufLength, 0, (SOCKADDR *)&senderAddr,
                                   &senderLen);
            if (recvLen == SOCKET_ERROR || recvLen == 0) {
                LOGE("receiveThread failed to receive from rtcp socket, error code:%d\n",
                     WSAGetLastError());
                break;
            }
            if (mRtpProto) mRtpProto->processRtcpPackage((const uint8_t *)pRecvBuf, recvLen);
        }
    }

    delete[] pRecvBuf;
}
