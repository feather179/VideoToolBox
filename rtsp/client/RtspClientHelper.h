#ifndef RTSP_CLIENT_HELPER_H
#define RTSP_CLIENT_HELPER_H

#include <rtsp/client/SdpClientHelper.h>
#include <rtsp/client/RtpClientStream.h>

#include <cstdint>

#include <string>
#include <vector>
#include <memory>

#include <winsock2.h>
#include <ws2tcpip.h>

class RtspClientHelper {
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
        int statusCode;
        int cseq;
        int contentLength;
        int timeout;
        uint16_t clientPort[2];
        uint16_t serverPort[2];
        std::string statusReason;
        std::string contentBase;
        std::string contentType;
        std::string session;
        std::vector<RtspMsgType> options;
    };

    struct RtspSession {
        std::string session;
        std::vector<RtspMsgType> options;
        std::vector<std::shared_ptr<SdpClientBaseStream>> sdpStreams;
    };

    bool mInitDone;
    int mCSeq;
    int mCurrStreamId;
    std::string mUrl;
    std::string mRtspAddr;
    SOCKET mRtspSocket;
    uint16_t mRtspPort;
    RtspSession mRtspSession;
    // RtpClientHelper mRtpHelper;
    std::vector<std::shared_ptr<RtpClientStream>> mRtpStreams;

    bool sendMessage(RtspMsgType msg);
    bool parseLine(std::string &line, RtspMessage &msg);
    bool connectRtspServer();

public:
    static std::string RTSP_AGENET;

    RtspClientHelper();
    RtspClientHelper(const RtspClientHelper &) = delete;
    RtspClientHelper &operator=(const RtspClientHelper &) = delete;
    ~RtspClientHelper();

    bool init(std::string url);
};

#endif
