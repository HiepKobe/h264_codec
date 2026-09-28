#ifndef H264_BRIDGE_H
#define H264_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(__WIN32__)
    #if defined(H264_BRIDGE_EXPORTS)
        #define EXPORT_API __declspec(dllexport)
    #else
        #define EXPORT_API __declspec(dllimport)
    #endif
#else
    #define EXPORT_API __attribute__((visibility("default")))
#endif

// Pixel formats for raw frame inputs from Camera / Video sources
typedef enum {
    PIXEL_FORMAT_I420 = 0,   // Standard YUV420P Planar (Y, U, V)
    PIXEL_FORMAT_NV21 = 1,   // Android Camera default (Y, VU interleaved)
    PIXEL_FORMAT_NV12 = 2,   // iOS / Camera2 default (Y, UV interleaved)
    PIXEL_FORMAT_YUV420P = 3,// Same as I420
    PIXEL_FORMAT_BGRA = 4    // Raw 32-bit BGRA image
} PixelFormat;

// Opaque handles for multi-instance support
typedef void* H264EncoderHandle;
typedef void* H264DecoderHandle;

#ifdef __cplusplus

// Abstract Codec Interfaces
class IH264Encoder {
public:
    virtual ~IH264Encoder() = default;
    virtual bool Init(int width, int height, int fps, int bitrate, PixelFormat format) = 0;
    virtual bool Encode(const uint8_t* raw_bytes, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe) = 0;
    virtual void RequestKeyframe() = 0;
    virtual bool Reconfigure(int new_bitrate, int new_fps) = 0;
};

class IH264Decoder {
public:
    virtual ~IH264Decoder() = default;
    virtual bool Init(int width, int height) = 0;
    virtual bool Decode(const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length) = 0;
};

extern "C" {
#endif

// ============================================================================
// SINGLETON / GLOBAL C-API
// ============================================================================

EXPORT_API bool init_encoder(int width, int height, int fps, int bitrate);

EXPORT_API bool encode_frame(const uint8_t* raw_yuv, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe);

EXPORT_API bool init_decoder(int width, int height);

EXPORT_API bool decode_frame(const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length);

EXPORT_API void free_buffer(uint8_t* ptr);

EXPORT_API void destroy_encoder(void);

EXPORT_API void destroy_decoder(void);

EXPORT_API void request_keyframe_global(void);

EXPORT_API bool reconfigure_encoder_global(int new_bitrate, int new_fps);

// ============================================================================
// MULTI-INSTANCE API (Realtime Low Latency / WebRTC grade)
// ============================================================================

EXPORT_API H264EncoderHandle create_encoder_instance(int width, int height, int fps, int bitrate, int pixel_format);

EXPORT_API bool encode_frame_instance(H264EncoderHandle handle, const uint8_t* raw_bytes, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe);

EXPORT_API void request_keyframe_instance(H264EncoderHandle handle);

EXPORT_API bool reconfigure_encoder_instance(H264EncoderHandle handle, int new_bitrate, int new_fps);

EXPORT_API bool pause_encoder_instance(H264EncoderHandle handle);

EXPORT_API bool resume_encoder_instance(H264EncoderHandle handle);

EXPORT_API void destroy_encoder_instance(H264EncoderHandle handle);

EXPORT_API H264DecoderHandle create_decoder_instance(int width, int height);

EXPORT_API bool decode_frame_instance(H264DecoderHandle handle, const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length);

EXPORT_API void destroy_decoder_instance(H264DecoderHandle handle);

// Pixel conversion utility exposed to C-API
EXPORT_API bool convert_pixel_format(const uint8_t* src, int src_format, int width, int height, uint8_t* dst_i420);

// Native Finalizer Function Pointer exports for Dart NativeFinalizer
EXPORT_API void (*get_destroy_encoder_finalizer(void))(H264EncoderHandle);

EXPORT_API void (*get_destroy_decoder_finalizer(void))(H264DecoderHandle);

#ifdef __cplusplus
}
#endif

#endif // H264_BRIDGE_H
