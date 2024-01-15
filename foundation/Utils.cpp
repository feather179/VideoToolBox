#include "Utils.h"
#include "BitReader.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

enum NaluTypeAVC {
    NALU_TYPE_AVC_SPS = 7,
    NALU_TYPE_AVC_PPS = 8,
};

enum NaluTypeHEVC {
    NALU_TYPE_HEVC_VPS = 32,
    NALU_TYPE_HEVC_SPS = 33,
    NALU_TYPE_HEVC_PPS = 34,
    NALU_TYPE_HEVC_SEI_PREFIX = 39,
};

std::string &trim(std::string &str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
    str.erase(
        std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) { return !std::isspace(ch); })
            .base(),
        str.end());

    return str;
}

std::string substr(std::string &s, const char b, const char e) {
    int pos1 = s.find_first_of(b);
    int pos2 = s.find_first_of(e);
    if (pos1 < 0) return "";
    if (pos2 < 0) pos2 = s.size();
    pos1 += 1;

    std::string str = s.substr(pos1, pos2);

    return trim(str);
}

bool isIPv4(std::string addr) {
    SOCKADDR_IN testAddr;
    return InetPton(AF_INET, addr.c_str(), &testAddr.sin_addr) == 1;
}

bool isIPv6(std::string addr) {
    SOCKADDR_IN6 testAddr;
    return InetPton(AF_INET6, addr.c_str(), &testAddr.sin6_addr) == 1;
}

// copy from ffmpeg libavformat/avc.c
static const uint8_t *findStartcodeInternal(const uint8_t *p, const uint8_t *end) {
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1) return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t *)p;
        //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
        //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1) return p;
                if (p[2] == 0 && p[3] == 1) return p + 1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1) return p + 2;
                if (p[4] == 0 && p[5] == 1) return p + 3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1) return p;
    }

    return end + 3;
}

const uint8_t *findStartcode(const uint8_t *p, const uint8_t *end) {
    const uint8_t *out = findStartcodeInternal(p, end);
    if (p < out && out < end && !out[-1]) out--;
    return out;
}

static const char *AUDIO_OBJECT_TYPE[] = {
    "Null",     // 0
    "AAC Main", // 1
    "AAC LC",   // 2
    "AAC SSR",  // 3
};

static int AUDIO_SAMPLE_RATE[] = {
    96000, // 0
    88200, // 1
    64000, // 2
    48000, // 3
    44100, // 4
    32000, // 5
    24000, // 6
    22050, // 7
    16000, // 8
    12000, // 9
    11025, // 10
    8000,  // 11
    7350,  // 12
    0,     // 13
    0,     // 14
    0,     // 15
};

static int AUDIO_CHANNEL_CONFIG[] = {
    0, // 0
    1, // 1
    2, // 2
    3, // 3
    4, // 4
    5, // 5
    6, // 6
    8, // 7
    0, // 8
    0, // 9
    0, // 10
    0, // 11
    0, // 12
    0, // 13
    0, // 14
    0, // 15
};

void parseCsdMPEG4(
    const uint8_t *data, int length, std::string &objectType, int &frequency, int &channels) {

    BitReader br(data, length);
    uint8_t typeIndex = br.getBits(5);
    uint8_t freqIndex = br.getBits(4);
    uint8_t channelIndex = br.getBits(4);

    objectType = AUDIO_OBJECT_TYPE[typeIndex];
    freqIndex = AUDIO_SAMPLE_RATE[freqIndex];
    channels = AUDIO_CHANNEL_CONFIG[channelIndex];
}

void parseCsdAVC(const uint8_t *data,
                 int length,
                 std::vector<uint8_t> &sps,
                 std::vector<uint8_t> &pps) {

    if (length >= 7 && data[0] == 0x01) {
        // AVCDecoderConfigurationRecord
        data += 5;
        length -= 5;
        {
            uint8_t numSps = (*data) & 0x1F;
            data += 1;
            length -= 1;
            for (uint8_t i = 0; i < numSps; ++i) {
                BitReader br(data, length);
                uint16_t spsLength = br.getBits(16);
                data += 2;
                length -= 2;
                sps.clear();
                sps.insert(sps.end(), data, data + spsLength);
                data += spsLength;
                length -= spsLength;
            }
        }
        {
            uint8_t numPps = *data;
            data += 1;
            length -= 1;
            for (uint8_t i = 0; i < numPps; ++i) {
                BitReader br(data, length);
                uint16_t ppsLength = br.getBits(16);
                data += 2;
                length -= 2;
                pps.clear();
                pps.insert(pps.end(), data, data + ppsLength);
                data += ppsLength;
                length -= ppsLength;
            }
        }
        // skip spsExt
    } else {
        // split by {0x00 0x00 0x00 0x01}
        const uint8_t *pStart = data;
        const uint8_t *pEnd = data + length;

        const uint8_t *pNaluStart = findStartcode(pStart, pEnd);
        const uint8_t *pNaluEnd = nullptr;
        while (true) {
            while (pNaluStart && (pNaluStart < pEnd) && !*(pNaluStart++))
                ;
            if (pNaluStart == pEnd) break;
            pNaluEnd = findStartcode(pNaluStart, pEnd);
            BitReader br(pNaluStart, 1);
            br.skipBits(3);
            uint8_t naluType = br.getBits(5);
            switch (naluType) {
                case NALU_TYPE_AVC_SPS: {
                    sps.clear();
                    sps.insert(sps.end(), pNaluStart, pNaluEnd);
                    break;
                }
                case NALU_TYPE_AVC_PPS: {
                    pps.clear();
                    pps.insert(pps.end(), pNaluStart, pNaluEnd);
                    break;
                }
                default:
                    break;
            }
            pNaluStart = pNaluEnd;
        }
    }
}

void parseCsdHEVC(const uint8_t *data,
                  int length,
                  std::vector<uint8_t> &vps,
                  std::vector<uint8_t> &sps,
                  std::vector<uint8_t> &pps,
                  std::vector<uint8_t> &sei) {

    if (length >= 23 && data[0] == 0x01) {
        // HEVCDecoderConfigurationRecord
        data += 22;
        length -= 22;
        uint8_t numArrays = *data;
        data += 1;
        length -= 1;

        for (uint8_t j = 0; j < numArrays; ++j) {
            BitReader br(data, length);
            br.skipBits(2);
            uint8_t naluType = br.getBits(6);
            uint16_t numNalus = br.getBits(16);
            data += 3;
            length -= 3;
            for (uint16_t i = 0; i < numNalus; ++i) {
                BitReader br(data, length);
                uint16_t naluLength = br.getBits(16);
                data += 2;
                length -= 2;
                switch (naluType) {
                    case NALU_TYPE_HEVC_VPS: {
                        vps.clear();
                        vps.insert(vps.end(), data, data + naluLength);
                        break;
                    }
                    case NALU_TYPE_HEVC_SPS: {
                        sps.clear();
                        sps.insert(sps.end(), data, data + naluLength);
                        break;
                    }
                    case NALU_TYPE_HEVC_PPS: {
                        pps.clear();
                        pps.insert(pps.end(), data, data + naluLength);
                        break;
                    }
                    case NALU_TYPE_HEVC_SEI_PREFIX: {
                        sei.clear();
                        sei.insert(sei.end(), data, data + naluLength);
                        break;
                    }
                    default:
                        break;
                }
                data += naluLength;
                length -= naluLength;
            }
        }
    } else {
        // split by {0x00 0x00 0x00 0x01}
        const uint8_t *pStart = data;
        const uint8_t *pEnd = data + length;

        const uint8_t *pNaluStart = findStartcode(pStart, pEnd);
        const uint8_t *pNaluEnd = nullptr;
        while (true) {
            while (pNaluStart && (pNaluStart < pEnd) && !*(pNaluStart++))
                ;
            if (pNaluStart == pEnd) break;
            pNaluEnd = findStartcode(pNaluStart, pEnd);
            BitReader br(pNaluStart, 2);
            br.skipBits(1);
            uint8_t naluType = br.getBits(6);
            switch (naluType) {
                case NALU_TYPE_HEVC_VPS: {
                    vps.clear();
                    vps.insert(vps.end(), pNaluStart, pNaluEnd);
                    break;
                }
                case NALU_TYPE_HEVC_SPS: {
                    sps.clear();
                    sps.insert(sps.end(), pNaluStart, pNaluEnd);
                    break;
                }
                case NALU_TYPE_HEVC_PPS: {
                    pps.clear();
                    pps.insert(pps.end(), pNaluStart, pNaluEnd);
                    break;
                }
                case NALU_TYPE_HEVC_SEI_PREFIX: {
                    sei.clear();
                    sei.insert(sei.end(), pNaluStart, pNaluEnd);
                    break;
                }
                default:
                    break;
            }
            pNaluStart = pNaluEnd;
        }
    }
}

int64_t rescaleTimeStamp(int64_t timeStamp, int inTimeScale, int outTimeScale) {
    if (!inTimeScale || (inTimeScale == outTimeScale)) return timeStamp;

    int64_t t = timeStamp * outTimeScale;
    return (t / inTimeScale);
}

struct TimerContext {
    TimerContext() { timerIsUp = false; }
    bool timerIsUp;
    std::mutex timerMutex;
    std::condition_variable timerCv;
};

static void callback(UINT, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR) {
    TimerContext *pCtx = (TimerContext *)dwUser;
    std::unique_lock<std::mutex> lk(pCtx->timerMutex);
    pCtx->timerIsUp = true;
    pCtx->timerCv.notify_all();
}

void msleep(int ms) {
    timeBeginPeriod(1);
    TimerContext ctx;
    ctx.timerIsUp = false;
    UINT timerId = timeSetEvent(ms, 1, callback, (DWORD_PTR)&ctx, TIME_ONESHOT);
    std::unique_lock<std::mutex> lk(ctx.timerMutex);
    ctx.timerCv.wait(lk, [&ctx]() { return ctx.timerIsUp; });
    timeKillEvent(timerId);
    timeEndPeriod(1);
}
