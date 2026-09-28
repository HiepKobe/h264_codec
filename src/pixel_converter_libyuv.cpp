#include "pixel_converter_libyuv.h"
#include <cstring>
#include <cstdlib>

#if __has_include(<libyuv.h>)
#include <libyuv.h>
#define HAS_LIBYUV 1
#elif __has_include("libyuv/libyuv.h")
#include "libyuv/libyuv.h"
#define HAS_LIBYUV 1
#elif __has_include("libyuv.h")
#include "libyuv.h"
#define HAS_LIBYUV 1
#else
#define HAS_LIBYUV 0
#endif

// ============================================================================
// LIBYUV HIGH-PERFORMANCE SIMD OPTIMIZED CONVERTER
// ============================================================================

bool LibYuvConverter::ConvertPlanesToI420(
    const uint8_t* src_y, int y_stride,
    const uint8_t* src_uv, int uv_stride,
    int width, int height,
    PixelFormat format,
    VideoRotation rotation,
    uint8_t** dst_i420,
    int* dst_length,
    int* out_width,
    int* out_height
) {
    if (!src_y || !dst_i420 || !dst_length || width <= 0 || height <= 0) {
        return false;
    }

    int half_width = width / 2;
    int half_height = height / 2;
    int y_size = width * height;
    int uv_size = half_width * half_height;
    int total_i420_size = y_size + 2 * uv_size;

    // Zero-allocation persistent buffer for intermediate I420 conversion
    uint8_t* i420_buf = m_i420Pool.GetBuffer(total_i420_size);
    uint8_t* dst_y_plane = i420_buf;
    uint8_t* dst_u_plane = i420_buf + y_size;
    uint8_t* dst_v_plane = i420_buf + y_size + uv_size;

#if HAS_LIBYUV
    // ------------------------------------------------------------------------
    // 1. High-Speed Pixel Format Conversion & Stride Stripping via Google libyuv
    // ------------------------------------------------------------------------
    int res = -1;
    if (format == PIXEL_FORMAT_NV21) {
        // NV21: Semi-planar YUV 4:2:0 (V, U interleaved) - Android Camera1/Camera2
        res = libyuv::NV21ToI420(
            src_y, y_stride,
            src_uv, uv_stride,
            dst_y_plane, width,
            dst_u_plane, half_width,
            dst_v_plane, half_width,
            width, height
        );
    } else if (format == PIXEL_FORMAT_NV12) {
        // NV12: Semi-planar YUV 4:2:0 (U, V interleaved) - iOS / Camera2
        res = libyuv::NV12ToI420(
            src_y, y_stride,
            src_uv, uv_stride,
            dst_y_plane, width,
            dst_u_plane, half_width,
            dst_v_plane, half_width,
            width, height
        );
    } else if (format == PIXEL_FORMAT_I420 || format == PIXEL_FORMAT_YUV420P) {
        // I420 Planar Copy removing stride
        res = libyuv::I420Copy(
            src_y, y_stride,
            src_uv, uv_stride,
            src_uv + uv_size, uv_stride,
            dst_y_plane, width,
            dst_u_plane, half_width,
            dst_v_plane, half_width,
            width, height
        );
    } else {
        // Android 420SemiPlanar / flexible planes fallback using libyuv::Android420ToI420
        res = libyuv::Android420ToI420(
            src_y, y_stride,
            src_uv, uv_stride,
            src_uv + 1, uv_stride,
            2, // pixel_stride_uv
            dst_y_plane, width,
            dst_u_plane, half_width,
            dst_v_plane, half_width,
            width, height
        );
    }

    if (res != 0) {
        return false;
    }
#else
    // Pure C++ Fallback if libyuv headers are not present in current build
    if (y_stride == width) {
        std::memcpy(dst_y_plane, src_y, y_size);
    } else {
        for (int row = 0; row < height; ++row) {
            std::memcpy(dst_y_plane + row * width, src_y + row * y_stride, width);
        }
    }

    if (src_uv) {
        if (format == PIXEL_FORMAT_NV21) {
            for (int r = 0; r < half_height; ++r) {
                const uint8_t* uv_row = src_uv + r * uv_stride;
                uint8_t* u_row = dst_u_plane + r * half_width;
                uint8_t* v_row = dst_v_plane + r * half_width;
                for (int c = 0; c < half_width; ++c) {
                    v_row[c] = uv_row[2 * c];
                    u_row[c] = uv_row[2 * c + 1];
                }
            }
        } else {
            for (int r = 0; r < half_height; ++r) {
                const uint8_t* uv_row = src_uv + r * uv_stride;
                uint8_t* u_row = dst_u_plane + r * half_width;
                uint8_t* v_row = dst_v_plane + r * half_width;
                for (int c = 0; c < half_width; ++c) {
                    u_row[c] = uv_row[2 * c];
                    v_row[c] = uv_row[2 * c + 1];
                }
            }
        }
    }
#endif

    // ------------------------------------------------------------------------
    // 2. Hardware Accelerated Video Rotation via libyuv::I420Rotate
    // ------------------------------------------------------------------------
    const uint8_t* final_y = dst_y_plane;
    const uint8_t* final_u = dst_u_plane;
    const uint8_t* final_v = dst_v_plane;

    int final_w = width;
    int final_h = height;
    int final_size = total_i420_size;

    if (rotation != ROTATION_0) {
        final_w = (rotation == ROTATION_90 || rotation == ROTATION_270) ? height : width;
        final_h = (rotation == ROTATION_90 || rotation == ROTATION_270) ? width : height;

        int rot_y_size = final_w * final_h;
        int rot_uv_size = (final_w / 2) * (final_h / 2);
        final_size = rot_y_size + 2 * rot_uv_size;

        uint8_t* rot_buf = m_rotatedPool.GetBuffer(final_size);
        uint8_t* rot_y = rot_buf;
        uint8_t* rot_u = rot_buf + rot_y_size;
        uint8_t* rot_v = rot_buf + rot_y_size + rot_uv_size;

#if HAS_LIBYUV
        libyuv::RotationMode mode = libyuv::kRotate0;
        if (rotation == ROTATION_90) mode = libyuv::kRotate90;
        else if (rotation == ROTATION_180) mode = libyuv::kRotate180;
        else if (rotation == ROTATION_270) mode = libyuv::kRotate270;

        libyuv::I420Rotate(
            dst_y_plane, width,
            dst_u_plane, half_width,
            dst_v_plane, half_width,
            rot_y, final_w,
            rot_u, final_w / 2,
            rot_v, final_w / 2,
            width, height,
            mode
        );
#else
        if (rotation == ROTATION_90) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    rot_y[x * height + (height - 1 - y)] = dst_y_plane[y * width + x];
                }
            }
            int hw = width / 2;
            int hh = height / 2;
            for (int y = 0; y < hh; ++y) {
                for (int x = 0; x < hw; ++x) {
                    rot_u[x * hh + (hh - 1 - y)] = dst_u_plane[y * hw + x];
                    rot_v[x * hh + (hh - 1 - y)] = dst_v_plane[y * hw + x];
                }
            }
        } else if (rotation == ROTATION_180) {
            for (int i = 0; i < y_size; ++i) {
                rot_y[i] = dst_y_plane[y_size - 1 - i];
            }
            for (int i = 0; i < uv_size; ++i) {
                rot_u[i] = dst_u_plane[uv_size - 1 - i];
                rot_v[i] = dst_v_plane[uv_size - 1 - i];
            }
        } else if (rotation == ROTATION_270) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    rot_y[(width - 1 - x) * height + y] = dst_y_plane[y * width + x];
                }
            }
            int hw = width / 2;
            int hh = height / 2;
            for (int y = 0; y < hh; ++y) {
                for (int x = 0; x < hw; ++x) {
                    rot_u[(hw - 1 - x) * hh + y] = dst_u_plane[y * hw + x];
                    rot_v[(hw - 1 - x) * hh + y] = dst_v_plane[y * hw + x];
                }
            }
        }
#endif
        final_y = rot_y;
        final_u = rot_u;
        final_v = rot_v;
    }

    *out_width = final_w;
    *out_height = final_h;
    *dst_length = final_size;

    // ------------------------------------------------------------------------
    // 3. ZERO-ALLOCATION Buffer Pool Output (Reusable across 60fps loop)
    // ------------------------------------------------------------------------
    uint8_t* output_buf = m_outputPool.GetBuffer(final_size);
    if (final_y != output_buf) {
        std::memcpy(output_buf, final_y, final_size);
    }
    *dst_i420 = output_buf;

    return true;
}

static LibYuvConverter g_global_libyuv_converter;

extern "C" {

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
) {
    VideoRotation rot = ROTATION_0;
    if (rotation_degrees == 90) rot = ROTATION_90;
    else if (rotation_degrees == 180) rot = ROTATION_180;
    else if (rotation_degrees == 270) rot = ROTATION_270;

    return g_global_libyuv_converter.ConvertPlanesToI420(
        src_y, y_stride,
        src_uv, uv_stride,
        width, height,
        static_cast<PixelFormat>(format),
        rot,
        out_i420,
        out_length,
        out_width,
        out_height
    );
}

} // extern "C"
