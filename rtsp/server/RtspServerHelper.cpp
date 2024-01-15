#include "RtspServerHelper.h"

#include "foundation/MD5.h"
#include "foundation/CountingQueue.h"
#include "foundation/FFBuffer.h"
#include "foundation/Log.h"

#include <filesystem>
#include <sstream>
#include <chrono>

static std::string SERVER_NAME = "Andu RTSP server test";

RtspServerHelper::RtspServerHelper() {}

RtspServerHelper::~RtspServerHelper() {
    if (mRtspSocket) closesocket(mRtspSocket);
}

bool RtspServerHelper::init() {
    mRtspSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mRtspSocket == INVALID_SOCKET) {
        LOGE("%s Failed to create rtsp socket, error code:%d\n", __PRETTY_FUNCTION__,
             WSAGetLastError());
        return false;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100 ms
    if (setsockopt(mRtspSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
        LOGE("%s Failed to set rtsp socket recv timeout, error code:%d\n", __PRETTY_FUNCTION__,
             WSAGetLastError());
        closesocket(mRtspSocket);
        return false;
    }

    SOCKADDR_IN serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(0);
    serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(mRtspSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LOGE("%s Failed to bind rtsp server, error code:%d\n", __PRETTY_FUNCTION__,
             WSAGetLastError());
        closesocket(mRtspSocket);
        return false;
    }

    {
        SOCKADDR_IN testAddr;
        int len = sizeof(testAddr);
        if (getsockname(mRtspSocket, (SOCKADDR *)&testAddr, &len) == SOCKET_ERROR) {
            LOGE("%s Failed to get rtsp socket name, error code:%d\n", __PRETTY_FUNCTION__,
                 WSAGetLastError());
            closesocket(mRtspSocket);
            return false;
        }
        mRtspPort = ntohs(testAddr.sin_port);
        // char localIPAddr[256] = { 0 };
        // inet_ntop(AF_INET, &testAddr.sin_addr, localIPAddr, sizeof(localIPAddr));
        LOGD("%s Rtsp server: listening on port:%hu\n", __PRETTY_FUNCTION__, mRtspPort);
    }

    if (listen(mRtspSocket, 3) == SOCKET_ERROR) {
        LOGE("%s Failed to listen rtsp socket, error code:%d\n", __PRETTY_FUNCTION__,
             WSAGetLastError());
        closesocket(mRtspSocket);
        return false;
    }

    mListenThread = std::make_unique<std::thread>(&RtspServerHelper::listenThread, this);

    return true;
}

void RtspServerHelper::listenThread() {
    while (true) {
        SOCKADDR_IN clientAddr;
        int len = sizeof(clientAddr);

        SOCKET clientSocket = accept(mRtspSocket, (SOCKADDR *)&clientAddr, &len);
        if (clientSocket == INVALID_SOCKET) {
            LOGE("%s accept invalid socket\n", __PRETTY_FUNCTION__);
            break;
        }

        LOGD("%s connected from %s:%hu\n", inet_ntoa(clientAddr.sin_addr),
             ntohs(clientAddr.sin_port));

        std::thread clientThread(&RtspServerHelper::clientHandler, this, clientSocket);
        clientThread.detach();
    }
}

void RtspServerHelper::clientHandler(SOCKET clientSocket) {
    char sendbuf[1024] = {0};
    char recvbuf[1024] = {0};

    std::string ipAddr;
    uint16_t port = 0;

    {
        SOCKADDR_IN testAddr;
        int len = sizeof(testAddr);
        if (getsockname(clientSocket, (SOCKADDR *)&testAddr, &len) == SOCKET_ERROR) {
            LOGE("%s Failed to get rtsp socket name, error code:%d\n", __PRETTY_FUNCTION__,
                 WSAGetLastError());
            return;
        }
        char localIPAddr[256] = {0};
        inet_ntop(AF_INET, &testAddr.sin_addr, localIPAddr, sizeof(localIPAddr));
        ipAddr = localIPAddr;
        port = ntohs(testAddr.sin_port);
    }

    auto parseMessage = [this](const char *buf, RtspMessage &msg) -> bool {
        std::stringstream ss(buf);
        std::string line;
        while (std::getline(ss, line)) {
            if (!parseLine(line, msg)) {
                LOGE("%s Failed to parse line:%s\n", __PRETTY_FUNCTION__, line.c_str());
                return false;
            }
        }
        return true;
    };

    std::shared_ptr<RtspProgram> rtspProgram;
    std::shared_ptr<RtspSession> rtspSession;

    std::unique_ptr<std::thread> sendThread;

    bool teardown = false;
    while (true) {
        if (teardown) {
            LOGD("%s Receive teardown message!\n", __PRETTY_FUNCTION__);
            break;
        }

        std::memset(recvbuf, 0, sizeof(recvbuf));
        int recvLen = recv(clientSocket, recvbuf, sizeof(recvbuf) - 1, 0);
        if (recvLen == SOCKET_ERROR) {
            LOGE("%s Failed to receive from client socket, error code:%d\n", __PRETTY_FUNCTION__,
                 WSAGetLastError());
            break;
        }
        if (recvLen == 0) {
            LOGD("%s Client socket closed\n", __PRETTY_FUNCTION__);
            break;
        }

        RtspMessage msg;
        if (!parseMessage(recvbuf, msg)) {
            LOGE("%s Failed to parse rtsp msg:%s\n", __PRETTY_FUNCTION__, recvbuf);
            break;
        }

        int i = 0;
        std::memset(sendbuf, 0, sizeof(sendbuf));

        switch (msg.msgId) {
            case RTSP_MSG_OPTIONS: {
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "RTSP/1.0 200 OK\r\n");
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", msg.cseq);
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Server: %s\r\n",
                                   SERVER_NAME.c_str());
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i,
                                   "Public: "
                                   "DESCRIBE,ANNOUNCE,SETUP,PLAY,RECORD,PAUSE,GET_PARAMETER,SET_"
                                   "PARAMETER,TEARDOWN\r\n");
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
                break;
            }
            case RTSP_MSG_DESCRIBE: {
                rtspProgram = getProgram(msg.programName);
                if (!rtspProgram) {
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i,
                                       "RTSP/1.0 404 Not Found\r\n");
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", msg.cseq);
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Server: %s\r\n",
                                       SERVER_NAME.c_str());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
                } else {
                    rtspSession = std::make_shared<RtspSession>();
                    std::timespec ts;
                    [[maybe_unused]] auto v = std::timespec_get(&ts, TIME_UTC);
                    rtspSession->session = md5Sum((const uint8_t *)&ts, sizeof(ts));
                    rtspSession->program = rtspProgram;

                    std::string sdpStr = rtspProgram->getSdpString(ipAddr, port);
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "RTSP/1.0 200 OK\r\n");
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", msg.cseq);
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Server: %s\r\n",
                                       SERVER_NAME.c_str());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i,
                                       "Content-Base: rtsp://%s:%hu/%s\r\n", ipAddr.c_str(), port,
                                       msg.programName.c_str());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Content-Type: %s\r\n",
                                       msg.acceptType.c_str());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Content-Length: %lld\r\n",
                                       sdpStr.size());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "%s", sdpStr.c_str());
                }
                break;
            }
            case RTSP_MSG_SETUP: {
                if (rtspSession && rtspProgram &&
                    (msg.session.empty() || msg.session == rtspSession->session)) {
                    int programStreamId = msg.programStreamId;
                    int payloadType = rtspProgram->getPayloadType(programStreamId);
                    MediaCodecType mediaType = rtspProgram->getMediaType(programStreamId);
                    std::string mime = rtspProgram->getMime(programStreamId);
                    std::vector<uint8_t> csd;
                    rtspProgram->getCsd(programStreamId, csd);
                    auto rtpStream = std::make_shared<RtpServerStream>(programStreamId, payloadType,
                                                                       mediaType, mime);
                    if (!rtpStream->init(ipAddr, msg.clientPort[0], msg.clientPort[1])) break;
                    rtpStream->parseCsd(csd.data(), csd.size());
                    rtspSession->streams.emplace_back(rtpStream);

                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "RTSP/1.0 200 OK\r\n");
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", msg.cseq);
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Server: %s\r\n",
                                       SERVER_NAME.c_str());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Session: %s\r\n",
                                       rtspSession->session.c_str());
                    i += std::snprintf(
                        sendbuf + i, sizeof(sendbuf) - i,
                        "Transport: %s;%s;client_port=%hu-%hu;server_port=%hu-%hu\r\n",
                        msg.protocol.c_str(), msg.cast.c_str(), msg.clientPort[0],
                        msg.clientPort[1], rtpStream->getRtpPort(), rtpStream->getRtcpPort());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
                }
                break;
            }
            case RTSP_MSG_PLAY: {
                if (rtspSession && rtspProgram && (msg.session == rtspSession->session)) {
                    addSession(rtspSession);

                    sendThread = std::make_unique<std::thread>(&RtspServerHelper::sendThread, this,
                                                               rtspSession);
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "RTSP/1.0 200 OK\r\n");
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", msg.cseq);
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Server: %s\r\n",
                                       SERVER_NAME.c_str());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Session: %s\r\n",
                                       msg.session.c_str());
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
                }
                break;
            }
            case RTSP_MSG_GET_PARAMETER: {
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "RTSP/1.0 200 OK\r\n");
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", msg.cseq);
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Server: %s\r\n",
                                   SERVER_NAME.c_str());
                if (!msg.contentType.empty()) {
                    i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Content-Type: %s\r\n",
                                       msg.contentType.c_str());
                }
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Session: %s\r\n",
                                   msg.session.c_str());
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
                break;
            }
            case RTSP_MSG_TEARDOWN: {
                teardown = true;

                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "RTSP/1.0 200 OK\r\n");
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "CSeq: %d\r\n", msg.cseq);
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Server: %s\r\n",
                                   SERVER_NAME.c_str());
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "Session: %s\r\n",
                                   msg.session.c_str());
                i += std::snprintf(sendbuf + i, sizeof(sendbuf) - i, "\r\n");
                break;
            }
            default:
                break;
        }

        if (std::strlen(sendbuf) > 0) {
            if (send(clientSocket, sendbuf, strlen(sendbuf), 0) == SOCKET_ERROR) {
                LOGE("%s Failed to send rtsp message\n", __PRETTY_FUNCTION__);
                break;
            }
        }
    }

    closesocket(clientSocket);

    if (sendThread && sendThread->joinable()) sendThread->join();
}

bool RtspServerHelper::parseLine(std::string &line, RtspMessage &msg) {
    if (line == "\r" || line == "\n" || line == "\r\n") return true;

    char str[1024] = {0};

    if (line.starts_with("OPTIONS")) {
        msg.msgId = RTSP_MSG_OPTIONS;
    } else if (line.starts_with("DESCRIBE")) {
        msg.msgId = RTSP_MSG_DESCRIBE;
        if (std::sscanf(line.c_str(), "DESCRIBE rtsp://%*[^/]/%[^ ] ", str) != 1) return false;
        msg.programName = str;
    } else if (line.starts_with("ANNOUNCE")) {
        msg.msgId = RTSP_MSG_ANNOUNCE;
    } else if (line.starts_with("SETUP")) {
        msg.msgId = RTSP_MSG_SETUP;

        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, ' ')) {
            if (token.starts_with("rtsp")) {
                int pos = token.find_last_of('/');
                pos += 1;
                if (pos < 0) return false;
                if (std::sscanf(token.substr(pos, token.size() - pos).c_str(), "trackID=%d",
                                &msg.programStreamId) != 1)
                    return false;
                else
                    break;
            }
        }
    } else if (line.starts_with("PLAY")) {
        msg.msgId = RTSP_MSG_PLAY;
    } else if (line.starts_with("PAUSE")) {
        msg.msgId = RTSP_MSG_PAUSE;
    } else if (line.starts_with("TEARDOWN")) {
        msg.msgId = RTSP_MSG_TEARDOWN;
    } else if (line.starts_with("GET_PARAMETER")) {
        msg.msgId = RTSP_MSG_GET_PARAMETER;
    } else if (line.starts_with("SET_PARAMETER")) {
        msg.msgId = RTSP_MSG_SET_PARAMETER;
    } else if (line.starts_with("REDIRECT")) {
        msg.msgId = RTSP_MSG_REDIRECT;
    } else if (line.starts_with("RECORD")) {
        msg.msgId = RTSP_MSG_RECORD;
    } else if (line.starts_with("CSeq")) {
        if (std::sscanf(line.c_str(), "CSeq: %d\r", &msg.cseq) != 1) return false;
    } else if (line.starts_with("User-Agent")) {
        msg.userAgent = substr(line, ' ', '\r');
    } else if (line.starts_with("Accept")) {
        msg.acceptType = substr(line, ' ', '\r');
    } else if (line.starts_with("Transport")) {
        std::string transport = substr(line, ' ', '\r');
        std::stringstream ss(transport);
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
            } else if (token == "RTP/AVP" || token == "RTP/AVP/UDP" || token == "RTP/AVP/TCP") {
                msg.protocol = token;
            } else if (token == "unicast" || token == "multicast") {
                msg.cast = token;
            }
        }
    } else if (line.starts_with("Session")) {
        std::string session = substr(line, ' ', '\r');
        std::stringstream ss(session);
        std::string token;
        while (std::getline(ss, token, ';')) {
            trim(token);
            if (token.starts_with("timeout")) {
                if (std::sscanf(token.c_str(), "timeout=%d", &msg.timeout) != 1) return false;
            } else {
                msg.session = token;
            }
        }
    } else if (line.starts_with("Range")) {
    } else if (line.starts_with("Content-type")) {
        // Content-type: text/parameters
        msg.contentType = substr(line, ' ', '\r');
    } else {
        return false;
    }

    return true;
}

void RtspServerHelper::sendThread(std::shared_ptr<RtspSession> session) {
    using BufferQueue = CountingQueue<std::shared_ptr<AVPacketBuffer>>;

    std::unique_ptr<std::thread> videoSendThread;
    std::unique_ptr<std::thread> audioSendThread;

    std::shared_ptr<BufferQueue> videoBufferQueue;
    std::shared_ptr<BufferQueue> audioBufferQueue;

    std::shared_ptr<RtpServerStream> rtpVideoStream;
    std::shared_ptr<RtpServerStream> rtpAudioStream;

    for (auto &stream : session->streams) {
        if (stream->getMediaType() == MEDIA_CODEC_TYPE_VIDEO) {
            rtpVideoStream = stream;
            videoBufferQueue = std::make_shared<BufferQueue>(10);
        } else if (stream->getMediaType() == MEDIA_CODEC_TYPE_AUDIO) {
            rtpAudioStream = stream;
            audioBufferQueue = std::make_shared<BufferQueue>(10);
        }
    }

    auto baseTimePoint = std::chrono::high_resolution_clock::now();

    auto sender = [&, this](MediaCodecType mediaType) {
        std::shared_ptr<RtpServerStream> rtpStream;
        std::shared_ptr<BufferQueue> bufferQueue;

        if (mediaType == MEDIA_CODEC_TYPE_VIDEO) {
            rtpStream = rtpVideoStream;
            bufferQueue = videoBufferQueue;
        } else if (mediaType == MEDIA_CODEC_TYPE_AUDIO) {
            rtpStream = rtpAudioStream;
            bufferQueue = audioBufferQueue;
        }

        if (rtpStream) {
            int index = 0;
            for (;;) {
                std::shared_ptr<AVPacketBuffer> packetBuffer;
                if (!bufferQueue->pop(packetBuffer)) break;
                if (packetBuffer) {
                    int64_t timestamp =
                        rescaleTimeStamp(packetBuffer->dts(), packetBuffer->timescale(), 1000);
                    auto now = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> duration = now - baseTimePoint;
                    int64_t delta = (int64_t)(timestamp - duration.count());
                    if (delta >= 5) msleep(delta - 5);
                    rtpStream->sendPacket(packetBuffer);
                } else
                    break;
            }
        }
    };

    if (rtpVideoStream) {
        videoSendThread = std::make_unique<std::thread>(sender, MEDIA_CODEC_TYPE_VIDEO);
    }
    if (rtpAudioStream) {
        audioSendThread = std::make_unique<std::thread>(sender, MEDIA_CODEC_TYPE_AUDIO);
    }

    auto rtspProgram = session->program;
    rtspProgram->setVideoBufferReadyCB([&](std::shared_ptr<AVPacketBuffer> packetBuffer) {
        videoBufferQueue->push(packetBuffer);
    });
    rtspProgram->setAudioBufferReadyCB([&](std::shared_ptr<AVPacketBuffer> packetBuffer) {
        audioBufferQueue->push(packetBuffer);
    });
    rtspProgram->start();

    if (videoSendThread && videoSendThread->joinable()) videoSendThread->join();
    if (audioSendThread && audioSendThread->joinable()) audioSendThread->join();
}

void RtspServerHelper::addSession(std::shared_ptr<RtspSession> session) {
    std::lock_guard<std::mutex> lock(mSessionMutex);
    mRtspSessions.emplace_back(session);
}

void RtspServerHelper::addProgram(std::shared_ptr<RtspProgram> program) {
    std::lock_guard<std::mutex> lock(mProgramMutex);
    mRtspPrograms.emplace_back(program);
}

std::shared_ptr<RtspProgram> RtspServerHelper::getProgram(std::string name) {
    std::lock_guard<std::mutex> lock(mProgramMutex);
    auto iter = std::find_if(mRtspPrograms.begin(), mRtspPrograms.end(),
                             [name](auto item) { return item->getProgramName() == name; });
    if (iter == mRtspPrograms.end()) return nullptr;
    return *iter;
}

void RtspServerHelper::addProgramFile(const std::string programName, const std::string filePath) {
    if (!getProgram(programName)) return;
    if (!std::filesystem::exists(filePath)) return;

    auto program =
        std::make_shared<RtspProgram>(RtspProgram::RTSP_PROGRAM_FILE, programName, filePath);
    program->init();

    LOGD("%s", program->getSdpString("127.0.0.1", 1234).c_str());

    addProgram(program);
}

void RtspServerHelper::addProgramScreen(const std::string programName) {
    if (!getProgram(programName)) return;

    auto program = std::make_shared<RtspProgram>(RtspProgram::RTSP_PROGRAM_SCREEN, programName);
    program->init();

    LOGD("%s", program->getSdpString("127.0.0.1", 1234).c_str());

    addProgram(program);
}

void RtspServerHelper::addProgramCamera(const std::string programName) {
    if (!getProgram(programName)) return;

    auto program = std::make_shared<RtspProgram>(RtspProgram::RTSP_PROGRAM_CAMERA, programName);
    program->init();

    LOGD("%s", program->getSdpString("127.0.0.1", 1234).c_str());

    addProgram(program);
}