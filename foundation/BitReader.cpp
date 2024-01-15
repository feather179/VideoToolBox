#include "BitReader.h"

BitReader::BitReader(const uint8_t *data, size_t size)
    : mpData(data), mSize(size), mReservoir(0), mNumBitsLeft(0) {}

BitReader::~BitReader() {}

bool BitReader::fillReservoir() {
    if (mSize == 0) return false;

    mReservoir = 0;
    int i = 0;
    for (; i < 4 && mSize > 0; ++i) {
        mReservoir = (mReservoir << 8) | *mpData;
        ++mpData;
        --mSize;
    }
    mNumBitsLeft = 8 * i;
    mReservoir <<= (32 - 8 * i);
    return true;
}

bool BitReader::getBitsGraceful(size_t n, uint32_t *out) {
    if (n > 32) return false;

    uint32_t ret = 0;
    while (n > 0) {
        if (mNumBitsLeft == 0) {
            if (!fillReservoir()) return false;
        }

        size_t m = n;
        if (m > mNumBitsLeft) m = mNumBitsLeft;
        ret = (ret << m) | (mReservoir >> (32 - m));
        mReservoir <<= m;
        n -= m;
        mNumBitsLeft -= m;
    }

    *out = ret;
    return true;
}

uint32_t BitReader::getBits(size_t n) {
    uint32_t ret = 0;
    getBitsGraceful(n, &ret);
    return ret;
}

bool BitReader::skipBits(size_t n) {
    uint32_t ret;
    while (n > 32) {
        if (!getBitsGraceful(32, &ret)) return false;
        n -= 32;
    }

    if (n > 0) return getBitsGraceful(n, &ret);
    return true;
}

size_t BitReader::numBitsLeft() const {
    return mSize * 8 + mNumBitsLeft;
}

const uint8_t *BitReader::data() const {
    return mpData - (mNumBitsLeft + 7) / 8;
}
