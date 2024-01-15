#include "RtpServerProto.h"
#include "foundation/BitWriter.h"
#include "foundation/BitReader.h"
#include "foundation/Utils.h"

#include <cstring>

#include <random>

#include <winsock2.h>

RtpServerBaseProto::RtpServerBaseProto()
    : mPayloadType(0), mIsFirstPack(true), mIsLastPack(false), mOffset(0), mPayloadSize(0),
      mPacketTimestamp(0) {
    mpBuffer = new uint8_t[RTP_MAX_FRAME_SIZE];

    std::mt19937 engine(std::random_device{}());

    mSeqNum = engine() % (USHRT_MAX);
    mSSRC = engine();
    mBaseTimestamp = engine();
}

RtpServerBaseProto::~RtpServerBaseProto() {
    delete[] mpBuffer;
}

void RtpServerBaseProto::prepareInternal() {
    mIsFirstPack = true;
    mIsLastPack = false;
    mOffset = 0;
}

int RtpServerBaseProto::buildRtcpPakcage() {
    return 0;
}

std::string RtpServerAACProto::MIME = "AAC";

RtpServerAACProto::RtpServerAACProto(int payloadType) : mpData(nullptr), mSize(0) {
    mPayloadType = payloadType;
}

RtpServerAACProto::~RtpServerAACProto() {}

void RtpServerAACProto::parseCsd(const uint8_t *data, int length) {}

void RtpServerAACProto::prepare(std::shared_ptr<AVPacketBuffer> packetBuffer) {
    const uint8_t *data = packetBuffer->data();
    int size = packetBuffer->size();

    mpData = packetBuffer->data();
    mSize = packetBuffer->size();
    mPacketTimestamp = packetBuffer->dts();

    prepareInternal();
}

int RtpServerAACProto::buildRtpPackage(uint8_t **out) {
    int bufferedSize = 0;
    uint8_t marker = 0;

    RtpHeader *header = (RtpHeader *)mpBuffer;

    if (mSize > 0) {
        // ADTS package
        BitWriter bw(mpBuffer + RTP_HEADER_SIZE, 4); // single package
        bw.putBits(sizeof(uint16_t) * 8, 16);        // AU-headers-length in bits
        bw.putBits(mSize, 13);                       // AU-header
        bw.putBits(0, 3);
        bw.flush();
        bufferedSize += 4;

        std::memcpy(mpBuffer + RTP_HEADER_SIZE + bufferedSize, mpData, mSize);
        bufferedSize += mSize;
        mSize = 0;
        marker = 1;
    }

    *out = mpBuffer;
    if (bufferedSize) {
        header->version = 2;
        header->padding = 0;
        header->ext = 0;
        header->cc = 0;
        header->marker = marker;
        header->payload = mPayloadType;
        header->seq = htons(++mSeqNum);
        header->timestamp = htonl((uint32_t)(mBaseTimestamp + mPacketTimestamp));
        header->ssrc = htonl(mSSRC);
        return bufferedSize + RTP_HEADER_SIZE;
    }

    return bufferedSize;
}

std::string RtpServerH264Proto::MIME = "H264;AVC";

RtpServerH264Proto::RtpServerH264Proto(int payloadType)
    : mpData(nullptr), mCurrNaluType(0), mCurrNRI(0), mNaluLengthSize(0) {

    mPayloadType = payloadType;
}

RtpServerH264Proto::~RtpServerH264Proto() {}

void RtpServerH264Proto::parseCsd(const uint8_t *data, int length) {
    parseCsdAVC(data, length, mSps, mPps);

    if (length >= 7 && data[0] == 0x01) {
        // AVCDecoderConfigurationRecord
        BitReader br(data, length);
        br.skipBits(4 * 8);
        br.skipBits(6);
        mNaluLengthSize = br.getBits(2) + 1;
    }
}

void RtpServerH264Proto::prepare(std::shared_ptr<AVPacketBuffer> packetBuffer) {
    mNalus.clear();

    int size = packetBuffer->size();
    const uint8_t *data = mpData = packetBuffer->data();
    mPacketTimestamp = packetBuffer->dts();

    if (mNaluLengthSize > 0) {
        int offset = 0;
        while (offset < size) {
            BitReader br(data, size);
            int naluSize = br.getBits(mNaluLengthSize * 8);
            mNalus.emplace_back(std::make_pair(offset + mNaluLengthSize, naluSize));
            data += (mNaluLengthSize + naluSize);
            offset += (mNaluLengthSize + naluSize);
        }
    } else {
        // split NALUs
        const uint8_t *pStart = data;
        const uint8_t *pEnd = data + size;

        const uint8_t *pNaluStart = findStartcode(pStart, pEnd);
        const uint8_t *pNaluEnd = nullptr;
        while (true) {
            while (pNaluStart && (pNaluStart < pEnd) && !*(pNaluStart++))
                ;
            if (pNaluStart == pEnd) break;
            pNaluEnd = findStartcode(pNaluStart, pEnd);
            mNalus.emplace_back(
                std::make_pair((int)(pNaluStart - pStart), (int)(pNaluEnd - pNaluStart)));
            pNaluStart = pNaluEnd;
        }
    }

    mCurrNalu = mNalus.cbegin();

    prepareInternal();
}

int RtpServerH264Proto::buildRtpPackage(uint8_t **out) {
    int bufferedSize = 0;
    int bufferedNalus = 0;
    int headerSize = 0;
    int payloadSize = 0;
    uint8_t marker = 0;

    RtpHeader *header = (RtpHeader *)mpBuffer;

    for (; mCurrNalu != mNalus.cend(); ++mCurrNalu) {
        mCurrNaluType = ((const RtpPayloadHeader *)(mpData + mCurrNalu->first))->type;
        mCurrNRI = ((const RtpPayloadHeader *)(mpData + mCurrNalu->first))->nri;
        payloadSize = mCurrNalu->second;
        if (payloadSize <= RTP_MAX_PAYLOAD_SIZE) {
            // try STAP-A
            if (bufferedSize == 0)
                headerSize = sizeof(RtpPayloadHeader);
            else
                headerSize = 0;

            if (bufferedSize + headerSize + 2 + payloadSize <= RTP_MAX_PAYLOAD_SIZE) {
                // build in current package
                if (headerSize) {
                    RtpPayloadHeader *payloadHeader =
                        (RtpPayloadHeader *)(mpBuffer + RTP_HEADER_SIZE);
                    payloadHeader->f = 0;
                    payloadHeader->nri = mCurrNRI;
                    payloadHeader->type = RTP_PAYLOAD_STAP_A;
                    bufferedSize += sizeof(RtpPayloadHeader);
                }
                *(uint16_t *)(mpBuffer + RTP_HEADER_SIZE + bufferedSize) = htons(payloadSize);
                bufferedSize += 2;
                std::memcpy(mpBuffer + RTP_HEADER_SIZE + bufferedSize, mpData + mCurrNalu->first,
                            payloadSize);
                bufferedSize += payloadSize;

                bufferedNalus += 1;
            } else {
                // build in next package
                break;
            }
        } else {
            if (bufferedSize > 0) break;

            // FU-A
            FUAHeader *fuHeader = (FUAHeader *)(mpBuffer + RTP_HEADER_SIZE);
            fuHeader->indicator.f = 0;
            fuHeader->indicator.nri = mCurrNRI;
            fuHeader->indicator.type = RTP_PAYLOAD_FU_A;
            fuHeader->f = 0;
            fuHeader->type = mCurrNaluType;
            if (mIsFirstPack) {
                fuHeader->start = 1;
                mOffset = mCurrNalu->first + sizeof(RtpPayloadHeader);
                mPayloadSize = payloadSize - sizeof(RtpPayloadHeader);
                mIsFirstPack = false;
            } else
                fuHeader->start = 0;
            fuHeader->end = 0;
            bufferedSize += sizeof(FUAHeader);

            if (bufferedSize + mPayloadSize <= RTP_MAX_PAYLOAD_SIZE) {
                std::memcpy(mpBuffer + RTP_HEADER_SIZE + bufferedSize, mpData + mOffset,
                            mPayloadSize);
                bufferedSize += mPayloadSize;
                marker = 1;
                fuHeader->end = 1;
            } else {
                int copySize = RTP_MAX_PAYLOAD_SIZE - bufferedSize;
                std::memcpy(mpBuffer + RTP_HEADER_SIZE + bufferedSize, mpData + mOffset, copySize);
                bufferedSize += copySize;
                mOffset += copySize;
                mPayloadSize -= copySize;
                break;
            }
        }
    }

    // need change from STAP-A to Single NAL unit packet
    if (bufferedNalus == 1) {
        int naluSize = ntohs(*(uint16_t *)(mpBuffer + RTP_HEADER_SIZE + sizeof(RtpPayloadHeader)));
        std::memmove(mpBuffer + RTP_HEADER_SIZE,
                     mpBuffer + RTP_HEADER_SIZE + sizeof(RtpPayloadHeader) + 2, naluSize);
        bufferedSize = naluSize;
    }

    *out = mpBuffer;
    if (bufferedSize) {
        header->version = 2;
        header->padding = 0;
        header->ext = 0;
        header->cc = 0;
        header->marker = marker;
        header->payload = mPayloadType;
        header->seq = htons(++mSeqNum);
        header->timestamp = htonl((uint32_t)(mBaseTimestamp + mPacketTimestamp));
        header->ssrc = htonl(mSSRC);
        return bufferedSize + RTP_HEADER_SIZE;
    }

    return bufferedSize;
}

std::string RtpServerHEVCProto::MIME = "H265;HEVC";

RtpServerHEVCProto::RtpServerHEVCProto(int payloadType)
    : mpData(nullptr), mCurrNaluType(0), mCurrTid(0), mNaluLengthSize(0) {

    mPayloadType = payloadType;
}

RtpServerHEVCProto::~RtpServerHEVCProto() {}

void RtpServerHEVCProto::parseCsd(const uint8_t *data, int length) {
    parseCsdHEVC(data, length, mVps, mSps, mPps, mSei);

    if (length >= 23 && data[0] == 0x01) {
        // HEVCDecoderConfigurationRecord
        BitReader br(data, length);
        br.skipBits(21 * 8);
        br.skipBits(6);
        mNaluLengthSize = br.getBits(2) + 1;
    }
}

void RtpServerHEVCProto::prepare(std::shared_ptr<AVPacketBuffer> packetBuffer) {
    mNalus.clear();

    int size = packetBuffer->size();
    const uint8_t *data = mpData = packetBuffer->data();
    mPacketTimestamp = packetBuffer->dts();

    if (mNaluLengthSize > 0) {
        int offset = 0;
        while (offset < size) {
            BitReader br(data, size);
            int naluSize = br.getBits(mNaluLengthSize * 8);
            mNalus.emplace_back(std::make_pair(offset + mNaluLengthSize, naluSize));
            data += (mNaluLengthSize + naluSize);
            offset += (mNaluLengthSize + naluSize);
        }
    } else {
        // split NALUs
        const uint8_t *pStart = data;
        const uint8_t *pEnd = data + size;

        const uint8_t *pNaluStart = findStartcode(pStart, pEnd);
        const uint8_t *pNaluEnd = nullptr;
        while (true) {
            while (pNaluStart && (pNaluStart < pEnd) && !*(pNaluStart++))
                ;
            if (pNaluStart == pEnd) break;
            pNaluEnd = findStartcode(pNaluStart, pEnd);
            mNalus.emplace_back(
                std::make_pair((int)(pNaluStart - pStart), (int)(pNaluEnd - pNaluStart)));
            pNaluStart = pNaluEnd;
        }
    }

    mCurrNalu = mNalus.cbegin();

    prepareInternal();
}

int RtpServerHEVCProto::buildRtpPackage(uint8_t **out) {
    int bufferedSize = 0;
    int bufferedNalus = 0;
    int headerSize = 0;
    int payloadSize = 0;
    uint8_t marker = 0;

    RtpHeader *header = (RtpHeader *)mpBuffer;

    for (; mCurrNalu != mNalus.cend(); ++mCurrNalu) {
        BitReader br(mpData + mCurrNalu->first, sizeof(RtpPayloadHeader));
        br.skipBits(1);
        mCurrNaluType = br.getBits(6);
        br.skipBits(6);
        mCurrTid = br.getBits(3);
        payloadSize = mCurrNalu->second;

        if (payloadSize <= RTP_MAX_PAYLOAD_SIZE) {
            // try APs
            if (bufferedSize == 0)
                headerSize = sizeof(RtpPayloadHeader);
            else
                headerSize = 0;

            if (bufferedSize + headerSize + 2 + payloadSize <= RTP_MAX_PAYLOAD_SIZE) {
                // build in current package
                if (headerSize) {
                    BitWriter bw(mpBuffer + RTP_HEADER_SIZE, sizeof(RtpPayloadHeader));
                    bw.putBits(0, 1);               // F bit
                    bw.putBits(RTP_PAYLOAD_APS, 6); // type
                    bw.putBits(0, 6);               // layer ID
                    bw.putBits(mCurrTid, 3);        // tid
                    bw.flush();
                    bufferedSize += sizeof(RtpPayloadHeader);
                }
                *(uint16_t *)(mpBuffer + RTP_HEADER_SIZE + bufferedSize) = htons(payloadSize);
                bufferedSize += 2;
                std::memcpy(mpBuffer + RTP_HEADER_SIZE + bufferedSize, mpData + mCurrNalu->first,
                            payloadSize);
                bufferedSize += payloadSize;

                bufferedNalus += 1;
            } else {
                // build in next package
                break;
            }
        } else {
            if (bufferedSize > 0) break;

            // FU
            FUHeader *fuHeader = (FUHeader *)(mpBuffer + RTP_HEADER_SIZE);
            BitWriter bw(mpBuffer + RTP_HEADER_SIZE, sizeof(FUHeader::indicator));
            bw.putBits(0, 1);              // F bit
            bw.putBits(RTP_PAYLOAD_FU, 6); // type
            bw.putBits(0, 6);              // layer ID
            bw.putBits(mCurrTid, 3);       // tid
            bw.flush();
            fuHeader->type = mCurrNaluType;
            if (mIsFirstPack) {
                fuHeader->start = 1;
                mOffset = mCurrNalu->first + sizeof(RtpPayloadHeader);
                mPayloadSize = payloadSize - sizeof(RtpPayloadHeader);
                mIsFirstPack = false;
            } else
                fuHeader->start = 0;
            fuHeader->end = 0;

            bufferedSize += sizeof(FUHeader);

            if (bufferedSize + mPayloadSize <= RTP_MAX_PAYLOAD_SIZE) {
                std::memcpy(mpBuffer + RTP_HEADER_SIZE + bufferedSize, mpData + mOffset,
                            mPayloadSize);
                bufferedSize += mPayloadSize;
                marker = 1;
                fuHeader->end = 1;
            } else {
                int copySize = RTP_MAX_PAYLOAD_SIZE - bufferedSize;
                std::memcpy(mpBuffer + RTP_HEADER_SIZE + bufferedSize, mpData + mOffset, copySize);
                bufferedSize += copySize;
                mOffset += copySize;
                mPayloadSize -= copySize;
                break;
            }
        }
    }

    // need change from APs to Single NAL unit packet
    if (bufferedNalus == 1) {
        int naluSize = ntohs(*(uint16_t *)(mpBuffer + RTP_HEADER_SIZE + sizeof(RtpPayloadHeader)));
        std::memmove(mpBuffer + RTP_HEADER_SIZE,
                     mpBuffer + RTP_HEADER_SIZE + sizeof(RtpPayloadHeader) + 2, naluSize);
        bufferedSize = naluSize;
    }

    *out = mpBuffer;
    if (bufferedSize) {
        header->version = 2;
        header->padding = 0;
        header->ext = 0;
        header->cc = 0;
        header->marker = marker;
        header->payload = mPayloadType;
        header->seq = htons(++mSeqNum);
        header->timestamp = htonl((uint32_t)(mBaseTimestamp + mPacketTimestamp));
        header->ssrc = htonl(mSSRC);
        return bufferedSize + RTP_HEADER_SIZE;
    }

    return bufferedSize;
}