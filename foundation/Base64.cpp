#include "Base64.h"

// copy from AOSP frameworks/av/media/module/foundation/base64.cpp

bool decodeBase64(const char *s, size_t *inOutBufSize, uint8_t *out) {
    size_t n = std::strlen(s);

    if ((n % 4) != 0) return false;

    size_t padding = 0;
    if (n >= 1 && s[n - 1] == '=') {
        padding = 1;

        if (n >= 2 && s[n - 2] == '=') {
            padding = 2;

            if (n >= 3 && s[n - 3] == '=') {
                padding = 3;
            }
        }
    }

    // We divide first to avoid overflow. It's OK to do this because we already made sure that n % 4
    // == 0.
    size_t outLen = (n / 4) * 3 - padding;

    if (out == nullptr || *inOutBufSize < outLen) return false;

    size_t j = 0;
    uint32_t accum = 0;
    for (size_t i = 0; i < n; ++i) {
        char c = s[i];
        unsigned int value;
        if (c >= 'A' && c <= 'Z') {
            value = c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            value = 26 + c - 'a';
        } else if (c >= '0' && c <= '9') {
            value = 52 + c - '0';
        } else if (c == '+' || c == '-') {
            value = 62;
        } else if (c == '/' || c == '_') {
            value = 63;
        } else if (c != '=') {
            return false;
        } else {
            if (i < n - padding) {
                return false;
            }
            value = 0;
        }

        accum = (accum << 6) | value;

        if (((i + 1) % 4) == 0) {
            if (j < outLen) {
                out[j++] = (accum >> 16);
            }
            if (j < outLen) {
                out[j++] = (accum >> 8) & 0xFF;
            }
            if (j < outLen) {
                out[j++] = accum & 0xFF;
            }

            accum = 0;
        }
    }

    *inOutBufSize = j;
    return true;
}

static char encode6Bit(unsigned int x) {
    if (x <= 25) {
        return 'A' + x;
    } else if (x <= 51) {
        return 'a' + x - 26;
    } else if (x <= 61) {
        return '0' + x - 52;
    } else if (x == 62) {
        return '+';
    } else {
        return '/';
    }
}

void encodeBase64(const uint8_t *data, size_t size, char *out) {
    int j = 0;
    size_t i;
    for (i = 0; i < (size / 3) * 3; i += 3) {
        uint8_t x1 = data[i];
        uint8_t x2 = data[i + 1];
        uint8_t x3 = data[i + 2];

        out[j++] = encode6Bit(x1 >> 2);
        out[j++] = encode6Bit((x1 << 4 | x2 >> 4) & 0x3F);
        out[j++] = encode6Bit((x2 << 2 | x3 >> 6) & 0x3F);
        out[j++] = encode6Bit(x3 & 0x3F);
    }

    switch (size % 3) {
        case 0:
            break;
        case 2: {
            uint8_t x1 = data[i];
            uint8_t x2 = data[i + 1];
            out[j++] = encode6Bit(x1 >> 2);
            out[j++] = encode6Bit((x1 << 4 | x2 >> 4) & 0x3F);
            out[j++] = encode6Bit((x2 << 2) & 0x3F);
            out[j++] = '=';
            break;
        }
        default: {
            uint8_t x1 = data[i];
            out[j++] = encode6Bit(x1 >> 2);
            out[j++] = encode6Bit((x1 << 4) & 0x3F);
            out[j++] = '=';
            out[j++] = '=';
            break;
        }
    }
}
