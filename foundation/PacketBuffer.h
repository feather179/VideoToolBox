#ifndef PACKET_BUFFER_H
#define PACKET_BUFFER_H

#include <cstdint>

class PacketBuffer {
public:
    explicit PacketBuffer(size_t capacity);
    PacketBuffer(void *data, size_t capacity);
    PacketBuffer(const PacketBuffer &) = delete;
    PacketBuffer &operator=(const PacketBuffer &) = delete;
    virtual ~PacketBuffer();

    uint8_t *base() { return (uint8_t *)mData; }
    uint8_t *data() { return (uint8_t *)mData + mRangeOffset; }
    size_t capacity() const { return mCapacity; }
    size_t size() const { return mRangeLength; }
    size_t offset() const { return mRangeOffset; }

    void setRange(size_t offset, size_t size);

private:
    void *mData;
    size_t mCapacity;
    size_t mRangeOffset;
    size_t mRangeLength;
};

#endif // PACKETBUFFER_H
