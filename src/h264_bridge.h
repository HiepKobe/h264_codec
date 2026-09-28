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

#ifdef __cplusplus
extern "C" {
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

// ============================================================================
// SINGLETON / GLOBAL C-API (as requested by specs)
// ============================================================================

/**
 * Initialize global H.264 Encoder.
 * @return true if initialized successfully.
 */
EXPORT_API bool init_encoder(int width, int height, int fps, int bitrate);

/**
 * Encode raw pixel frame to H.264 NAL Units.
 * @param raw_yuv Pointer to input raw pixel bytes
 * @param length Length of raw_yuv byte array
 * @param out_h264 Pointer to output pointer allocated by native memory (must be freed via free_buffer)
 * @param out_length Pointer to int storing length of encoded H.264 buffer
 * @param is_keyframe Pointer to bool indicating whether encoded packet is I-Frame
 * @return true on success, false on error
 */
EXPORT_API bool encode_frame(const uint8_t* raw_yuv, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe);

/**
 * Initialize global H.264 Decoder.
 * @return true if initialized successfully.
 */
EXPORT_API bool init_decoder(int width, int height);

/**
 * Decode H.264 packet to raw YUV I420 frame.
 * @param h264_bytes Pointer to H.264 NAL units
 * @param length Length of H.264 byte array
 * @param out_yuv Pointer to output pointer allocated by native memory (must be freed via free_buffer)
 * @param out_length Pointer to int storing length of decoded YUV buffer
 * @return true on success, false on error
 */
EXPORT_API bool decode_frame(const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length);

/**
 * Free buffer allocated by native C++ layer (e.g. out_h264 or out_yuv).
 */
EXPORT_API void free_buffer(uint8_t* ptr);

/**
 * Destroy global encoder and free resources.
 */
EXPORT_API void destroy_encoder(void);

/**
 * Destroy global decoder and free resources.
 */
EXPORT_API void destroy_decoder(void);

// ============================================================================
// MULTI-INSTANCE API
// ============================================================================

EXPORT_API H264EncoderHandle create_encoder_instance(int width, int height, int fps, int bitrate, int pixel_format);
EXPORT_API bool encode_frame_instance(H264EncoderHandle handle, const uint8_t* raw_bytes, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe);
EXPORT_API void destroy_encoder_instance(H264EncoderHandle handle);

EXPORT_API H264DecoderHandle create_decoder_instance(int width, int height);
EXPORT_API bool decode_frame_instance(H264DecoderHandle handle, const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length);
EXPORT_API void destroy_decoder_instance(H264DecoderHandle handle);

// Pixel conversion utility exposed to C-API
EXPORT_API bool convert_pixel_format(const uint8_t* src, int src_format, int width, int height, uint8_t* dst_i420);

#ifdef __cplusplus
}
#endif

#endif // H264_BRIDGE_H
