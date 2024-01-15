#ifndef FFBUFFER_H
#define FFBUFFER_H

#include <cstdint>

struct AVPacket;
struct AVFrame;

class AVPacketBuffer {
private:
    AVPacket *mpPacket;

public:
    AVPacketBuffer();
    ~AVPacketBuffer();

    AVPacket *get();
    AVPacket &operator*() const;
    AVPacket *operator->();
    const uint8_t *data();
    int size() const;
    int64_t dts() const;
    int timescale() const;
};

class AVFrameBuffer {
private:
    AVFrame *mpFrame;

public:
    AVFrameBuffer();
    ~AVFrameBuffer();

    AVFrame *get();
    AVFrame &operator*() const;
    AVFrame *operator->();
};

#endif
