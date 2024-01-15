#ifndef BIT_READER_H
#define BIT_READER_H

#include <cstdint>

class BitReader {
private:
    const uint8_t *mpData;
    size_t mSize;
    uint32_t mReservoir;
    uint32_t mNumBitsLeft;

    bool fillReservoir();

public:
    BitReader(const uint8_t *data, size_t size);
    BitReader(const BitReader &) = delete;
    BitReader &operator=(const BitReader &) = delete;
    ~BitReader();

    bool getBitsGraceful(size_t n, uint32_t *out);
    uint32_t getBits(size_t n);
    bool skipBits(size_t n);
    size_t numBitsLeft() const;
    const uint8_t *data() const;
};

#endif
