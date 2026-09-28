#ifndef H264_HARDWARE_CODEC_H
#define H264_HARDWARE_CODEC_H

#include "h264_bridge.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>

// Abstract Base Class for Hardware Encoders
class HardwareH264Encoder : public IH264Encoder {
public:
    virtual ~HardwareH264Encoder() = default;
    static std::unique_ptr<HardwareH264Encoder> Create();
};

// Abstract Base Class for Hardware Decoders
class HardwareH264Decoder : public IH264Decoder {
public:
    virtual ~HardwareH264Decoder() = default;
    static std::unique_ptr<HardwareH264Decoder> Create();
};

#endif // H264_HARDWARE_CODEC_H
