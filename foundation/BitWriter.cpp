#include "BitWriter.h"

BitWriter::BitWriter(uint8_t *data, size_t size)
    : mpData(data), mSize(size), mReservoir(0), mNumBitsLeft(0) {}

BitWriter::~BitWriter() {
    flush();
}

void BitWriter::putBits(uint32_t x, size_t n) {
    while (n > 0) {
        size_t m = n;
        if (m > mNumBitsLeft) m = mNumBitsLeft;

        mReservoir = (mReservoir << m) | ((x >> (n - m)) & (~(0xFFFFFFFF << m)));
        mNumBitsLeft -= m;
        n -= m;

        if (mNumBitsLeft == 0) flush();
    }
}

void BitWriter::flush() {
    while (mNumBitsLeft <= 24 && mSize > 0) {
        *mpData = (uint8_t)(mReservoir >> (24 - mNumBitsLeft));
        ++mpData;
        --mSize;
        mNumBitsLeft += 8;
    }
    if (mNumBitsLeft < 32 && mSize > 0) {
        size_t m = 32 - mNumBitsLeft;
        *mpData = (uint8_t)(mReservoir << (8 - m));
        ++mpData;
        --mSize;
        mNumBitsLeft += m;
    }
}