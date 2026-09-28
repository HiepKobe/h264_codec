#ifndef PIXEL_CONVERTER_LIBYUV_H
#define PIXEL_CONVERTER_LIBYUV_H

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <mutex>
#include <memory>

#include "h264_bridge.h"

// Angle rotations
typedef enum {
    ROTATION_0 = 0,
    ROTATION_90 = 90,
    ROTATION_180 = 180,
    ROTATION_270 = 270
} VideoRotation;

// Zero-allocation reusable buffer pool for 60fps render loops
class ReusableBufferPool {
private:
    std::vector<uint8_t> m_buffer;
    size_t m_capacity = 0;
    std::mutex m_mutex;

public:
    ReusableBufferPool() = default;

    uint8_t* GetBuffer(size_t requiredSize) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_capacity < requiredSize) {
            m_buffer.resize(requiredSize);
            m_capacity = requiredSize;
        }
        return m_buffer.data();
    }

    size_t GetCapacity() const {
        return m_capacity;
    }
};

class LibYuvConverter {
private:
    ReusableBufferPool m_i420Pool;
    ReusableBufferPool m_rotatedPool;
    ReusableBufferPool m_outputPool;

public:
    LibYuvConverter() = default;

    /**
     * Convert CameraImage Planes (handling Stride/Padding) to contiguous I420 Planar,
     * and optionally apply rotation (0, 90, 180, 270 degrees).
     * Zero-allocation in 60fps rendering loop.
     */
    bool ConvertPlanesToI420(
        const uint8_t* src_y, int y_stride,
        const uint8_t* src_uv, int uv_stride,
        int width, int height,
        PixelFormat format,
        VideoRotation rotation,
        uint8_t** dst_i420,
        int* dst_length,
        int* out_width,
        int* out_height
    );
};

#ifdef __cplusplus
extern "C" {
#endif

EXPORT_API bool process_camera_frame_libyuv(
    const uint8_t* src_y, int y_stride,
    const uint8_t* src_uv, int uv_stride,
    int width, int height,
    int format,
    int rotation_degrees,
    uint8_t** out_i420,
    int* out_length,
    int* out_width,
    int* out_height
);

#ifdef __cplusplus
}
#endif

#endif // PIXEL_CONVERTER_LIBYUV_H
