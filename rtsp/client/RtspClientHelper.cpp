#include "RtspClientHelper.h"
#include "foundation/Utils.h"
#include "foundation/Log.h"

#include <sstream>
#include <algorithm>

std::string RtspClientHelper::RTSP_AGENET = "Andu RTSP Test Agent";

RtspClientHelper::RtspClientHelper()
    : mInitDone(false), mCSeq(0), mCurrStreamId(0), mRtspSocket(0), mRtspPort(0) {}

RtspClientHelper::~RtspClientHelper() {}

bool RtspClientHelper::sendMessage(RtspMsgType msgId) {
    if (!mInitDone) {
        LOGE("%s can not send message before init\n", __PRETTY_FUNCTION__);
        return false;
    }

    char sendbuf[1024] = {0};
    char recvbuf[1024] = {0};
    int i = 0;

    switch (msgId) {
        case RtspClientHelper::RTSP_MSG_OPTIONS: {
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "OPTIONS %s RTSP/1.0\r\n",
                               mUrl.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", ++mCSeq);
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "User-Agent: %s\r\n",
                               RTSP_AGENET.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
            break;
        }
        case RtspClientHelper::RTSP_MSG_DESCRIBE: {
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "DESCRIBE %s RTSP/1.0\r\n",
                               mUrl.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", ++mCSeq);
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "User-Agent: %s\r\n",
                               RTSP_AGENET.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Accept: application/sdp\r\n");
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
            break;
        }
        case RtspClientHelper::RTSP_MSG_ANNOUNCE: {
            break;
        }
        case RtspClientHelper::RTSP_MSG_SETUP: {
            auto iter1 =
                std::find_if(mRtspSession.sdpStreams.begin(), mRtspSession.sdpStreams.end(),
                             [this](auto item) { return item->getStreamId() == mCurrStreamId; });
            auto sdpStream = *iter1;
            auto iter2 = std::find_if(mRtpStreams.begin(), mRtpStreams.end(), [this](auto item) {
                return item->getStreamId() == mCurrStreamId;
            });
            auto rtpStream = *iter2;
            uint16_t rtpPort = rtpStream->getRtpPort();
            uint16_t rtcpPort = rtpStream->getRtcpPort();

            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "SETUP %s RTSP/1.0\r\n",
                               mUrl.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", ++mCSeq);
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "User-Agent: %s\r\n",
                               RTSP_AGENET.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i,
                               "Transport: %s;unicast;client_port=%hu-%hu\r\n",
                               sdpStream->getProtocol().c_str(), rtpPort, rtcpPort);
            if (!mRtspSession.session.empty()) {
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Session: %s\r\n",
                                   mRtspSession.session.c_str());
            }
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");

            break;
        }
        case RtspClientHelper::RTSP_MSG_PLAY: {
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "PLAY %s RTSP/1.0\r\n",
                               mUrl.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", ++mCSeq);
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "User-Agent: %s\r\n",
                               RTSP_AGENET.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Session: %s\r\n",
                               mRtspSession.session.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Range: npt=%.3f-\r\n", 0.0f);
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
            break;
        }
        case RtspClientHelper::RTSP_MSG_PAUSE: {
            break;
        }
        case RtspClientHelper::RTSP_MSG_TEARDOWN: {
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "TEARDOWN %s RTSP/1.0\r\n",
                               mUrl.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", ++mCSeq);
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "User-Agent: %s\r\n",
                               RTSP_AGENET.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Session: %s\r\n",
                               mRtspSession.session.c_str());
            i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
            break;
        }
        case RtspClientHelper::RTSP_MSG_GET_PARAMETER: {
            break;
        }
        case RtspClientHelper::RTSP_MSG_SET_PARAMETER: {
            break;
        }
        case RtspClientHelper::RTSP_MSG_REDIRECT: {
            break;
        }
        case RtspClientHelper::RTSP_MSG_RECORD: {
            break;
        }
        default:
            break;
    }

    if (std::strlen(sendbuf) > 0) {
        if (send(mRtspSocket, sendbuf, strlen(sendbuf), 0) == SOCKET_ERROR) {
            LOGE("%s failed to send rtsp packet\n", __PRETTY_FUNCTION__);
            return false;
        }
    } else {
        return false;
    }

    int recvLen = recv(mRtspSocket, recvbuf, sizeof(recvbuf) - 1, 0);
    if (recvLen == SOCKET_ERROR) {
        LOGE("%s failed to receive rtsp packet\n", __PRETTY_FUNCTION__);
        return false;
    }
    if (recvLen == 0) {
        LOGE("%s rtsp socket closed\n", __PRETTY_FUNCTION__);
        return false;
    }

    RtspMessage msg;
    msg.msgId = msgId;

    std::stringstream ss(recvbuf);
    std::string line;
    while (std::getline(ss, line)) {
        if (line == "\r" || line == "\n" || line == "\r\n") break;
        if (!parseLine(line, msg)) {
            LOGE("%s failed to parse line:%s\n", __PRETTY_FUNCTION__, line.c_str());
            return false;
        }
    }

    if (msg.statusCode != 200) {
        LOGE("%s error status:%d %s\n", __PRETTY_FUNCTION__, msg.statusCode,
             msg.statusReason.c_str());
        return false;
    }

    switch (msgId) {
        case RtspClientHelper::RTSP_MSG_OPTIONS:
            mRtspSession.options.insert(mRtspSession.options.end(), msg.options.begin(),
                                        msg.options.end());
            break;
        case RtspClientHelper::RTSP_MSG_DESCRIBE: {
            if (msg.contentType == "application/sdp") {
                auto sdpHelper = std::make_unique<SdpClientHelper>();
                std::string sdpStr = recvbuf + recvLen - msg.contentLength;
                if (!sdpHelper->parseSdp(sdpStr)) {
                    LOGE("%s failed to parse sdp: %s\n", __PRETTY_FUNCTION__, sdpStr.c_str());
                    return false;
                }
                sdpHelper->copySdpStreams(mRtspSession.sdpStreams);
            }
            break;
        }
        case RtspClientHelper::RTSP_MSG_SETUP:
            mRtspSession.session = msg.session;
            break;
        default:
            break;
    }

    return true;
}

bool RtspClientHelper::parseLine(std::string &line, RtspMessage &msg) {
    if (line == "\r" || line == "\n" || line == "\r\n") return true;

    char str[1024] = {0};
    if (line.starts_with("RTSP")) {
        // status line
        if (std::sscanf(line.c_str(), "RTSP%*[^ ] %d %[^\r]\r", &msg.statusCode, str) != 2)
            return false;
        msg.statusReason = str;
    } else if (line.starts_with("CSeq")) {
        if (std::sscanf(line.c_str(), "CSeq: %d\r", &msg.cseq) != 1) return false;
    } else if (line.starts_with("Public")) {
        int pos1 = line.find_first_of(' ');
        int pos2 = line.find_first_of('\r');
        if (pos1 < 0) return false;
        pos1 += 1;
        if (pos2 < 0) pos2 = line.size();
        std::stringstream ss(line.substr(pos1, pos2 - pos1));
        std::string token;
        while (std::getline(ss, token, ' ')) {
            if (token.starts_with("DESCRIBE")) {
                msg.options.emplace_back(RTSP_MSG_DESCRIBE);
            } else if (token.starts_with("ANNOUNCE")) {
                msg.options.emplace_back(RTSP_MSG_ANNOUNCE);
            } else if (token.starts_with("SETUP")) {
                msg.options.emplace_back(RTSP_MSG_SETUP);
            } else if (token.starts_with("PLAY")) {
                msg.options.emplace_back(RTSP_MSG_PLAY);
            } else if (token.starts_with("RECORD")) {
                msg.options.emplace_back(RTSP_MSG_RECORD);
            } else if (token.starts_with("PAUSE")) {
                msg.options.emplace_back(RTSP_MSG_PAUSE);
            } else if (token.starts_with("GET_PARAMETER")) {
                msg.options.emplace_back(RTSP_MSG_GET_PARAMETER);
            } else if (token.starts_with("SET_PARAMETER")) {
                msg.options.emplace_back(RTSP_MSG_SET_PARAMETER);
            } else if (token.starts_with("REDIRECT")) {
                msg.options.emplace_back(RTSP_MSG_REDIRECT);
            } else if (token.starts_with("TEARDOWN")) {
                msg.options.emplace_back(RTSP_MSG_TEARDOWN);
            }
        }
    } else if (line.starts_with("Server")) {

    } else if (line.starts_with("Content-Base")) {
        int pos1 = line.find_first_of(' ');
        int pos2 = line.find_first_of('\r');
        if (pos1 < 0) return false;
        pos1 += 1;
        if (pos2 < 0) pos2 = line.size();
        msg.contentBase = line.substr(pos1, pos2 - pos1);
    } else if (line.starts_with("Content-Length")) {
        if (std::sscanf(line.c_str(), "Content-Length: %d\r", &msg.contentLength) != 1)
            return false;
    } else if (line.starts_with("Content-Type")) {
        int pos1 = line.find_first_of(' ');
        int pos2 = line.find_first_of('\r');
        if (pos1 < 0) return false;
        pos1 += 1;
        if (pos2 < 0) pos2 = line.size();
        msg.contentType = line.substr(pos1, pos2 - pos1);
    } else if (line.starts_with("Session")) {
        int pos1 = line.find_first_of(' ');
        int pos2 = line.find_first_of('\r');
        if (pos1 < 0) return false;
        pos1 += 1;
        if (pos2 < 0) pos2 = line.size();

        std::stringstream ss(line.substr(pos1, pos2 - pos1));
        std::string token;
        while (std::getline(ss, token, ';')) {
            trim(token);
            if (token.starts_with("timeout")) {
                if (std::sscanf(token.c_str(), "timeout=%d", &msg.timeout) != 1) return false;
            } else {
                msg.session = token;
            }
        }
    } else if (line.starts_with("Transport")) {
        int pos1 = line.find_first_of(' ');
        int pos2 = line.find_first_of('\r');
        if (pos1 < 0) return false;
        pos1 += 1;
        if (pos2 < 0) pos2 = line.size();

        std::stringstream ss(line.substr(pos1, pos2 - pos1));
        std::string token;
        while (std::getline(ss, token, ';')) {
            trim(token);
            if (token.starts_with("client_port")) {
                if (std::sscanf(token.c_str(), "client_port=%hu-%hu", &msg.clientPort[0],
                                &msg.clientPort[1]) != 2)
                    return false;
            } else if (token.starts_with("server_port")) {
                if (std::sscanf(token.c_str(), "server_port=%hu-%hu", &msg.serverPort[0],
                                &msg.serverPort[1]) != 2)
                    return false;
            } else if (token == "RTP/AVP" || token == "RTP/SVP/UDP") {

            } else if (token == "RTP/AVP/TCP") {

            } else if (token == "unicast" || token == "multicast") {
            }
        }
    } else {
        return false;
    }

    return true;
}

bool RtspClientHelper::connectRtspServer() {
    if (!sendMessage(RTSP_MSG_OPTIONS)) return false;
    if (!sendMessage(RTSP_MSG_DESCRIBE)) return false;
    for (auto &sdpStream : mRtspSession.sdpStreams) {
        mCurrStreamId = sdpStream->getStreamId();
        int payloadType = sdpStream->getPayloadType();
        std::string mime = sdpStream->getMime();
        std::string protocol = sdpStream->getProtocol();
        auto rtpStream = std::make_shared<RtpClientStream>(mCurrStreamId, payloadType, mime,
                                                           mRtspAddr, protocol);
        if (!rtpStream->init()) return false;
        rtpStream->setSdpStream(sdpStream);
        if (!sendMessage(RTSP_MSG_SETUP)) return false;
    }

    if (!sendMessage(RTSP_MSG_PLAY)) return false;

    return true;
}

bool RtspClientHelper::init(std::string url) {
    mInitDone = false;
    mUrl = url;
    int len = mUrl.size();
    if (len > 0 && mUrl[len - 1] == '/') {
        mUrl[len - 1] = '\0';
    }
    char addr[512] = {'\0'};
    if (std::sscanf(mUrl.c_str(), "%*[^:]://%[^:]:%hd", addr, &mRtspPort) != 2) {
        LOGE("%s Failed to init RTSP, invalid url:%s\n", __PRETTY_FUNCTION__, mUrl.c_str());
        return false;
    }
    mRtspAddr = addr;
    mRtspSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ;
    if (mRtspSocket == INVALID_SOCKET) {
        LOGE("%s Failed to create RTSP socket, error code:%d\n", __PRETTY_FUNCTION__,
             WSAGetLastError());
        return false;
    }

    int timeout = 1000; // ms
    if (setsockopt(mRtspSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        LOGE("%s Failed to set RTSP socket recv timeout, error code:%d\n", __PRETTY_FUNCTION__,
             WSAGetLastError());
        return false;
    }

    SOCKADDR_IN serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(mRtspPort);
    inet_pton(AF_INET, mRtspAddr.c_str(), &serverAddr.sin_addr);
    if (connect(mRtspSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LOGE("%s Failed to connect to RTSP server, error code:%d\n", __PRETTY_FUNCTION__,
             WSAGetLastError());
        return false;
    }

    mInitDone = true;

    if (!connectRtspServer()) return false;

    return true;
}
