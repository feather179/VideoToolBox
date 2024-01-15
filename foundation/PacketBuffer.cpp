#include "PacketBuffer.h"

PacketBuffer::PacketBuffer(size_t capacity) {
    mRangeOffset = 0;
    mData = new uint8_t[capacity];
    if (mData == nullptr) {
        mCapacity = 0;
        mRangeLength = 0;
    } else {
        mCapacity = capacity;
        mRangeLength = capacity;
    }
}

PacketBuffer::PacketBuffer(void *data, size_t capacity) {
    mData = data;
    mCapacity = capacity;
    mRangeOffset = 0;
    mRangeLength = capacity;
}

PacketBuffer::~PacketBuffer() {
    if (mData != nullptr) {
        delete[] (uint8_t *)mData;
        mData = nullptr;
    }
}

void PacketBuffer::setRange(size_t offset, size_t size) {
    if (mCapacity <= offset) return;
    if (mCapacity - offset < size) return;
    mRangeOffset = offset;
    mRangeLength = size;
}
