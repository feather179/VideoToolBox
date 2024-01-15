#include "FFBuffer.h"

extern "C" {
#include "libavcodec/packet.h"
#include "libavutil/frame.h"
}

AVPacketBuffer::AVPacketBuffer() {
    mpPacket = av_packet_alloc();
}

AVPacketBuffer::~AVPacketBuffer() {
    av_packet_free(&mpPacket);
}

AVPacket *AVPacketBuffer::get() {
    return mpPacket;
}

AVPacket &AVPacketBuffer::operator*() const {
    return *mpPacket;
}

AVPacket *AVPacketBuffer::operator->() {
    return mpPacket;
}

const uint8_t *AVPacketBuffer::data() {
    return mpPacket->data;
}

int AVPacketBuffer::size() const {
    return mpPacket->size;
}

int64_t AVPacketBuffer::dts() const {
    return mpPacket->dts;
}

int AVPacketBuffer::timescale() const {
    return mpPacket->time_base.den;
}

AVFrameBuffer::AVFrameBuffer() {
    mpFrame = av_frame_alloc();
}

AVFrameBuffer::~AVFrameBuffer() {
    av_frame_free(&mpFrame);
}

AVFrame *AVFrameBuffer::get() {
    return mpFrame;
}

AVFrame &AVFrameBuffer::operator*() const {
    return *mpFrame;
}

AVFrame *AVFrameBuffer::operator->() {
    return mpFrame;
}