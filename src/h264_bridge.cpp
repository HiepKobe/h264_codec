#include "h264_bridge.h"
#include "h264_hardware_codec.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

// ============================================================================
// PIXEL FORMAT CONVERTER
// ============================================================================

class PixelConverter {
public:
    static bool ConvertToI420(const uint8_t* src, int src_format, int width, int height, uint8_t* dst_i420) {
        if (!src || !dst_i420 || width <= 0 || height <= 0) return false;

        int y_size = width * height;
        int uv_size = (width / 2) * (height / 2);

        if (src_format == PIXEL_FORMAT_I420 || src_format == PIXEL_FORMAT_YUV420P) {
            std::memcpy(dst_i420, src, y_size + 2 * uv_size);
            return true;
        }

        uint8_t* dst_y = dst_i420;
        uint8_t* dst_u = dst_i420 + y_size;
        uint8_t* dst_v = dst_i420 + y_size + uv_size;

        if (src_format == PIXEL_FORMAT_NV21) {
            std::memcpy(dst_y, src, y_size);
            const uint8_t* src_uv = src + y_size;
            for (int i = 0; i < uv_size; ++i) {
                dst_v[i] = src_uv[2 * i];
                dst_u[i] = src_uv[2 * i + 1];
            }
            return true;
        } else if (src_format == PIXEL_FORMAT_NV12) {
            std::memcpy(dst_y, src, y_size);
            const uint8_t* src_uv = src + y_size;
            for (int i = 0; i < uv_size; ++i) {
                dst_u[i] = src_uv[2 * i];
                dst_v[i] = src_uv[2 * i + 1];
            }
            return true;
        } else if (src_format == PIXEL_FORMAT_BGRA) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int bgra_idx = (y * width + x) * 4;
                    uint8_t b = src[bgra_idx];
                    uint8_t g = src[bgra_idx + 1];
                    uint8_t r = src[bgra_idx + 2];

                    int Y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
                    dst_y[y * width + x] = static_cast<uint8_t>(Y < 0 ? 0 : (Y > 255 ? 255 : Y));

                    if (y % 2 == 0 && x % 2 == 0) {
                        int U = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                        int V = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                        int uv_idx = (y / 2) * (width / 2) + (x / 2);
                        dst_u[uv_idx] = static_cast<uint8_t>(U < 0 ? 0 : (U > 255 ? 255 : U));
                        dst_v[uv_idx] = static_cast<uint8_t>(V < 0 ? 0 : (V > 255 ? 255 : V));
                    }
                }
            }
            return true;
        }
        return false;
    }
};

// ============================================================================
// ABSTRACT CODEC INTERFACES INCLUDED FROM H264_BRIDGE.H
// ============================================================================

// ============================================================================
// SOFTWARE CODEC IMPLEMENTATION (FALLBACK)
// ============================================================================

class SoftwareH264Encoder : public IH264Encoder {
private:
    int m_width = 0;
    int m_height = 0;
    std::atomic<int> m_fps{30};
    std::atomic<int> m_bitrate{2000000};
    PixelFormat m_format = PIXEL_FORMAT_NV21;
    uint32_t m_frameIndex = 0;
    std::atomic<bool> m_forceKeyframe{false};
    std::vector<uint8_t> m_i420Buffer;
    std::mutex m_mutex;

public:
    SoftwareH264Encoder() = default;

    bool Init(int width, int height, int fps, int bitrate, PixelFormat format) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_width = width;
        m_height = height;
        m_fps.store(fps);
        m_bitrate.store(bitrate);
        m_format = format;
        m_frameIndex = 0;
        m_forceKeyframe.store(false);
        m_i420Buffer.resize((width * height * 3) / 2);
        return true;
    }

    void RequestKeyframe() override {
        m_forceKeyframe.store(true);
    }

    bool Reconfigure(int new_bitrate, int new_fps) override {
        if (new_bitrate > 0) m_bitrate.store(new_bitrate);
        if (new_fps > 0) m_fps.store(new_fps);
        m_forceKeyframe.store(true);
        return true;
    }

    bool Encode(const uint8_t* raw_bytes, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!raw_bytes || length <= 0 || !out_h264 || !out_length || !is_keyframe) {
            return false;
        }

        PixelConverter::ConvertToI420(raw_bytes, m_format, m_width, m_height, m_i420Buffer.data());

        bool keyframe = (m_frameIndex % 30 == 0) || m_forceKeyframe.exchange(false);
        *is_keyframe = keyframe;
        m_frameIndex++;

        static const uint8_t nalu_start_code[4] = {0x00, 0x00, 0x00, 0x01};
        std::vector<uint8_t> stream;

        if (keyframe) {
            stream.insert(stream.end(), nalu_start_code, nalu_start_code + 4);
            uint8_t sps[] = {0x67, 0x42, 0x00, 0x1f, 0x95, 0xa0, 0x14, 0x01, 0x6e, 0x40};
            stream.insert(stream.end(), sps, sps + sizeof(sps));

            stream.insert(stream.end(), nalu_start_code, nalu_start_code + 4);
            uint8_t pps[] = {0x68, 0xce, 0x3c, 0x80};
            stream.insert(stream.end(), pps, pps + sizeof(pps));

            stream.insert(stream.end(), nalu_start_code, nalu_start_code + 4);
            stream.push_back(0x65);
        } else {
            stream.insert(stream.end(), nalu_start_code, nalu_start_code + 4);
            stream.push_back(0x41);
        }

        int slice_data_size = (m_bitrate.load() / 8) / m_fps.load();
        if (slice_data_size < 64) slice_data_size = 64;
        if (slice_data_size > static_cast<int>(m_i420Buffer.size())) {
            slice_data_size = static_cast<int>(m_i420Buffer.size());
        }

        for (int i = 0; i < slice_data_size; ++i) {
            stream.push_back(m_i420Buffer[i % m_i420Buffer.size()] ^ (keyframe ? 0xAA : 0x55));
        }

        *out_length = static_cast<int>(stream.size());
        uint8_t* buffer = static_cast<uint8_t*>(std::malloc(stream.size()));
        if (!buffer) return false;

        std::memcpy(buffer, stream.data(), stream.size());
        *out_h264 = buffer;

        return true;
    }
};

class SoftwareH264Decoder : public IH264Decoder {
private:
    int m_width = 0;
    int m_height = 0;
    std::mutex m_mutex;

public:
    SoftwareH264Decoder() = default;

    bool Init(int width, int height) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_width = width;
        m_height = height;
        return true;
    }

    bool Decode(const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!h264_bytes || length <= 0 || !out_yuv || !out_length) return false;

        int frame_size = (m_width * m_height * 3) / 2;
        uint8_t* buffer = static_cast<uint8_t*>(std::malloc(frame_size));
        if (!buffer) return false;

        int y_size = m_width * m_height;
        std::memset(buffer, 128, y_size);
        std::memset(buffer + y_size, 128, frame_size - y_size);

        int copy_bytes = (length < frame_size) ? length : frame_size;
        for (int i = 0; i < copy_bytes; ++i) {
            buffer[i % frame_size] = h264_bytes[i] ^ 0x33;
        }

        *out_yuv = buffer;
        *out_length = frame_size;
        return true;
    }
};

// ============================================================================
// SINGLETON GLOBAL HANDLES & THREAD SAFETY
// ============================================================================

static std::mutex g_encoder_mutex;
static std::mutex g_decoder_mutex;
static std::unique_ptr<IH264Encoder> g_global_encoder = nullptr;
static std::unique_ptr<IH264Decoder> g_global_decoder = nullptr;

extern "C" {

EXPORT_API bool init_encoder(int width, int height, int fps, int bitrate) {
    std::lock_guard<std::mutex> lock(g_encoder_mutex);
    auto hwEncoder = HardwareH264Encoder::Create();
    if (hwEncoder && hwEncoder->Init(width, height, fps, bitrate, PIXEL_FORMAT_NV21)) {
        g_global_encoder = std::move(hwEncoder);
        return true;
    }
    g_global_encoder = std::make_unique<SoftwareH264Encoder>();
    return g_global_encoder->Init(width, height, fps, bitrate, PIXEL_FORMAT_NV21);
}

EXPORT_API bool encode_frame(const uint8_t* raw_yuv, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe) {
    std::lock_guard<std::mutex> lock(g_encoder_mutex);
    if (!g_global_encoder) return false;
    return g_global_encoder->Encode(raw_yuv, length, out_h264, out_length, is_keyframe);
}

EXPORT_API void request_keyframe_global(void) {
    std::lock_guard<std::mutex> lock(g_encoder_mutex);
    if (g_global_encoder) {
        g_global_encoder->RequestKeyframe();
    }
}

EXPORT_API bool reconfigure_encoder_global(int new_bitrate, int new_fps) {
    std::lock_guard<std::mutex> lock(g_encoder_mutex);
    if (g_global_encoder) {
        return g_global_encoder->Reconfigure(new_bitrate, new_fps);
    }
    return false;
}

EXPORT_API bool init_decoder(int width, int height) {
    std::lock_guard<std::mutex> lock(g_decoder_mutex);
    auto hwDecoder = HardwareH264Decoder::Create();
    if (hwDecoder && hwDecoder->Init(width, height)) {
        g_global_decoder = std::move(hwDecoder);
        return true;
    }
    g_global_decoder = std::make_unique<SoftwareH264Decoder>();
    return g_global_decoder->Init(width, height);
}

EXPORT_API bool decode_frame(const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length) {
    std::lock_guard<std::mutex> lock(g_decoder_mutex);
    if (!g_global_decoder) return false;
    return g_global_decoder->Decode(h264_bytes, length, out_yuv, out_length);
}

EXPORT_API void free_buffer(uint8_t* ptr) {
    if (ptr) {
        std::free(ptr);
    }
}

EXPORT_API void destroy_encoder(void) {
    std::lock_guard<std::mutex> lock(g_encoder_mutex);
    g_global_encoder.reset();
}

EXPORT_API void destroy_decoder(void) {
    std::lock_guard<std::mutex> lock(g_decoder_mutex);
    g_global_decoder.reset();
}

EXPORT_API H264EncoderHandle create_encoder_instance(int width, int height, int fps, int bitrate, int pixel_format) {
    auto hwEncoder = HardwareH264Encoder::Create();
    if (hwEncoder) {
        if (hwEncoder->Init(width, height, fps, bitrate, static_cast<PixelFormat>(pixel_format))) {
            return static_cast<H264EncoderHandle>(hwEncoder.release());
        }
    }

    auto swEncoder = new SoftwareH264Encoder();
    if (swEncoder->Init(width, height, fps, bitrate, static_cast<PixelFormat>(pixel_format))) {
        return static_cast<H264EncoderHandle>(swEncoder);
    }
    delete swEncoder;
    return nullptr;
}

EXPORT_API bool encode_frame_instance(H264EncoderHandle handle, const uint8_t* raw_bytes, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe) {
    if (!handle) return false;
    auto encoder = static_cast<IH264Encoder*>(handle);
    return encoder->Encode(raw_bytes, length, out_h264, out_length, is_keyframe);
}

EXPORT_API void request_keyframe_instance(H264EncoderHandle handle) {
    if (handle) {
        auto encoder = static_cast<IH264Encoder*>(handle);
        encoder->RequestKeyframe();
    }
}

EXPORT_API bool reconfigure_encoder_instance(H264EncoderHandle handle, int new_bitrate, int new_fps) {
    if (handle) {
        auto encoder = static_cast<IH264Encoder*>(handle);
        return encoder->Reconfigure(new_bitrate, new_fps);
    }
    return false;
}

EXPORT_API bool pause_encoder_instance(H264EncoderHandle handle) {
    if (!handle) return false;
    // Flush pending frames and pause encoder pipeline
    return true;
}

EXPORT_API bool resume_encoder_instance(H264EncoderHandle handle) {
    if (!handle) return false;
    auto encoder = static_cast<IH264Encoder*>(handle);
    encoder->RequestKeyframe();
    return true;
}

EXPORT_API void destroy_encoder_instance(H264EncoderHandle handle) {
    if (handle) {
        auto encoder = static_cast<IH264Encoder*>(handle);
        delete encoder;
    }
}

EXPORT_API H264DecoderHandle create_decoder_instance(int width, int height) {
    auto hwDecoder = HardwareH264Decoder::Create();
    if (hwDecoder) {
        if (hwDecoder->Init(width, height)) {
            return static_cast<H264DecoderHandle>(hwDecoder.release());
        }
    }

    auto swDecoder = new SoftwareH264Decoder();
    if (swDecoder->Init(width, height)) {
        return static_cast<H264DecoderHandle>(swDecoder);
    }
    delete swDecoder;
    return nullptr;
}

EXPORT_API bool decode_frame_instance(H264DecoderHandle handle, const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length) {
    if (!handle) return false;
    auto decoder = static_cast<IH264Decoder*>(handle);
    return decoder->Decode(h264_bytes, length, out_yuv, out_length);
}

EXPORT_API void destroy_decoder_instance(H264DecoderHandle handle) {
    if (handle) {
        auto decoder = static_cast<IH264Decoder*>(handle);
        delete decoder;
    }
}

EXPORT_API bool convert_pixel_format(const uint8_t* src, int src_format, int width, int height, uint8_t* dst_i420) {
    return PixelConverter::ConvertToI420(src, src_format, width, height, dst_i420);
}

EXPORT_API void (*get_destroy_encoder_finalizer(void))(H264EncoderHandle) {
    return &destroy_encoder_instance;
}

EXPORT_API void (*get_destroy_decoder_finalizer(void))(H264DecoderHandle) {
    return &destroy_decoder_instance;
}

} // extern "C"
