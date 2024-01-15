#ifndef BIT_WRITER_H
#define BIT_WRITER_H

#include <cstdint>

class BitWriter {
private:
    uint8_t *mpData;
    size_t mSize;
    uint32_t mReservoir;
    uint32_t mNumBitsLeft;

public:
    BitWriter(uint8_t *data, size_t size);
    BitWriter(const BitWriter &) = delete;
    BitWriter &operator=(const BitWriter &) = delete;
    ~BitWriter();

    void putBits(uint32_t x, size_t n);
    void flush();
};

#endif
