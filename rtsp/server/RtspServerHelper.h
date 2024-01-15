#ifndef RTSP_SERVER_HELPER_H
#define RTSP_SERVER_HELPER_H

#include "rtsp/server/RtspProgram.h"
#include "rtsp/server/RtpServerStream.h"

#include <cstdint>

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>

#include <winsock2.h>

class RtspServerHelper {
private:
    enum RtspMsgType {
        RTSP_MSG_OPTIONS,
        RTSP_MSG_DESCRIBE,
        RTSP_MSG_ANNOUNCE,
        RTSP_MSG_SETUP,
        RTSP_MSG_PLAY,
        RTSP_MSG_PAUSE,
        RTSP_MSG_TEARDOWN,
        RTSP_MSG_GET_PARAMETER,
        RTSP_MSG_SET_PARAMETER,
        RTSP_MSG_REDIRECT,
        RTSP_MSG_RECORD,
    };

    struct RtspMessage {
        RtspMsgType msgId;
        int cseq;
        int timeout;
        int programStreamId;
        uint16_t clientPort[2];
        uint16_t serverPort[2];
        std::string programName;
        std::string acceptType;
        std::string contentType;
        std::string session;
        std::string userAgent;
        std::string protocol;
        std::string cast; // unicast/multicast/broadcast
    };

    struct RtspSession {
        std::string session;
        std::shared_ptr<RtspProgram> program;
        std::vector<std::shared_ptr<RtpServerStream>> streams;
    };

    SOCKET mRtspSocket;
    uint16_t mRtspPort;

    std::mutex mProgramMutex;
    std::vector<std::shared_ptr<RtspProgram>> mRtspPrograms;

    std::mutex mSessionMutex;
    std::vector<std::shared_ptr<RtspSession>> mRtspSessions;

    void addSession(std::shared_ptr<RtspSession> session);

    bool parseLine(std::string &line, RtspMessage &msg);

    void addProgram(std::shared_ptr<RtspProgram> program);
    std::shared_ptr<RtspProgram> getProgram(std::string name);

    std::unique_ptr<std::thread> mListenThread;
    void listenThread();

    void clientHandler(SOCKET clientSocket);
    void sendThread(std::shared_ptr<RtspSession> session);

public:
    RtspServerHelper();
    RtspServerHelper(const RtspServerHelper &) = delete;
    RtspServerHelper &operator=(const RtspServerHelper &) = delete;
    RtspServerHelper(RtspServerHelper &&) = delete;
    virtual ~RtspServerHelper();

    bool init();

    void addProgramFile(const std::string programName, const std::string filePath);
    void addProgramScreen(const std::string programName);
    void addProgramCamera(const std::string programName);
};

#endif
