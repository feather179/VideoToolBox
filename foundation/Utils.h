#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <vector>

enum MediaCodecType {
    MEDIA_CODEC_TYPE_UNKNOWN,
    MEDIA_CODEC_TYPE_AUDIO,
    MEDIA_CODEC_TYPE_VIDEO,
};

std::string &trim(std::string &str);
// substr between first char b and first char e
std::string substr(std::string &s, const char b, const char e);

const uint8_t *findStartcode(const uint8_t *p, const uint8_t *end);

bool isIPv4(std::string addr);
bool isIPv6(std::string addr);

void parseCsdMPEG4(
    const uint8_t *data, int length, std::string &objectType, int &frequency, int &channels);
void parseCsdAVC(const uint8_t *data,
                 int length,
                 std::vector<uint8_t> &sps,
                 std::vector<uint8_t> &pps);
void parseCsdHEVC(const uint8_t *data,
                  int length,
                  std::vector<uint8_t> &vps,
                  std::vector<uint8_t> &sps,
                  std::vector<uint8_t> &pps,
                  std::vector<uint8_t> &sei);

int64_t rescaleTimeStamp(int64_t timeStamp, int inTimeScale, int outTimeScale);

void msleep(int ms);

#endif
