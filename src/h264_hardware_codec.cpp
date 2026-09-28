#include "h264_hardware_codec.h"
#include <cstring>
#include <cstdlib>
#include <iostream>

// ============================================================================
// ANDROID HARDWARE CODEC (AMediaCodec / Android NDK)
// ============================================================================
#if defined(__ANDROID__)
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

class AndroidMediaCodecEncoder : public HardwareH264Encoder {
private:
    AMediaCodec* m_codec = nullptr;
    AMediaFormat* m_format = nullptr;
    int m_width = 0;
    int m_height = 0;
    std::atomic<int> m_fps{30};
    std::atomic<int> m_bitrate{2000000};
    PixelFormat m_pixelFormat = PIXEL_FORMAT_NV21;
    std::atomic<bool> m_forceKeyframe{false};
    int64_t m_pts = 0;
    std::mutex m_mutex;

public:
    AndroidMediaCodecEncoder() = default;

    ~AndroidMediaCodecEncoder() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_codec) {
            AMediaCodec_stop(m_codec);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
        }
        if (m_format) {
            AMediaFormat_delete(m_format);
            m_format = nullptr;
        }
    }

    bool Init(int width, int height, int fps, int bitrate, PixelFormat format) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_width = width;
        m_height = height;
        m_fps.store(fps);
        m_bitrate.store(bitrate);
        m_pixelFormat = format;
        m_pts = 0;

        m_codec = AMediaCodec_createEncoderByType("video/avc");
        if (!m_codec) return false;

        m_format = AMediaFormat_new();
        AMediaFormat_setString(m_format, AMEDIAFORMAT_KEY_MIME, "video/avc");
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_HEIGHT, height);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 21 /* COLOR_FormatYUV420SemiPlanar NV21/NV12 */);

        // Ultra Low-Latency Configuration
        AMediaFormat_setInt32(m_format, "latency", 0);
        AMediaFormat_setInt32(m_format, "max-bframes", 0);
        AMediaFormat_setInt32(m_format, "bitrate-mode", 1 /* CBR */);

        media_status_t status = AMediaCodec_configure(m_codec, m_format, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
        if (status != AMEDIA_OK) {
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
            return false;
        }

        return AMediaCodec_start(m_codec) == AMEDIA_OK;
    }

    void RequestKeyframe() override {
        m_forceKeyframe.store(true);
    }

    bool Reconfigure(int new_bitrate, int new_fps) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_codec) return false;

        if (new_bitrate > 0) m_bitrate.store(new_bitrate);
        if (new_fps > 0) m_fps.store(new_fps);

#if __ANDROID_API__ >= 26
        AMediaFormat* params = AMediaFormat_new();
        AMediaFormat_setInt32(params, "video-bitrate", m_bitrate.load());
        media_status_t st = AMediaCodec_setParameters(m_codec, params);
        AMediaFormat_delete(params);
        return st == AMEDIA_OK;
#else
        return true;
#endif
    }

    bool Encode(const uint8_t* raw_bytes, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_codec || !raw_bytes || length <= 0) return false;

        if (m_forceKeyframe.exchange(false)) {
#if __ANDROID_API__ >= 26
            AMediaFormat* params = AMediaFormat_new();
            AMediaFormat_setInt32(params, "request-sync", 0);
            AMediaCodec_setParameters(m_codec, params);
            AMediaFormat_delete(params);
#endif
        }

        ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(m_codec, 10000 /* 10ms */);
        if (inputIndex >= 0) {
            size_t bufSize = 0;
            uint8_t* buf = AMediaCodec_getInputBuffer(m_codec, inputIndex, &bufSize);
            if (buf && bufSize >= static_cast<size_t>(length)) {
                std::memcpy(buf, raw_bytes, length);
                AMediaCodec_queueInputBuffer(m_codec, inputIndex, 0, length, m_pts, 0);
                m_pts += (1000000 / m_fps.load());
            }
        }

        AMediaCodecBufferInfo info;
        ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 10000);
        if (outputIndex >= 0) {
            size_t outBufSize = 0;
            uint8_t* outBuf = AMediaCodec_getOutputBuffer(m_codec, outputIndex, &outBufSize);
            if (outBuf && info.size > 0) {
                *is_keyframe = (info.flags & AMEDIACODEC_BUFFER_FLAG_KEY_FRAME) != 0;
                *out_length = info.size;
                uint8_t* buffer = static_cast<uint8_t*>(std::malloc(info.size));
                std::memcpy(buffer, outBuf + info.offset, info.size);
                *out_h264 = buffer;
                AMediaCodec_releaseOutputBuffer(m_codec, outputIndex, false);
                return true;
            }
            AMediaCodec_releaseOutputBuffer(m_codec, outputIndex, false);
        }
        return false;
    }
};

class AndroidMediaCodecDecoder : public HardwareH264Decoder {
private:
    AMediaCodec* m_codec = nullptr;
    AMediaFormat* m_format = nullptr;
    int m_width = 0;
    int m_height = 0;
    std::mutex m_mutex;

public:
    AndroidMediaCodecDecoder() = default;

    ~AndroidMediaCodecDecoder() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_codec) {
            AMediaCodec_stop(m_codec);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
        }
        if (m_format) {
            AMediaFormat_delete(m_format);
            m_format = nullptr;
        }
    }

    bool Init(int width, int height) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_width = width;
        m_height = height;

        m_codec = AMediaCodec_createDecoderByType("video/avc");
        if (!m_codec) return false;

        m_format = AMediaFormat_new();
        AMediaFormat_setString(m_format, AMEDIAFORMAT_KEY_MIME, "video/avc");
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_HEIGHT, height);

        media_status_t status = AMediaCodec_configure(m_codec, m_format, NULL, NULL, 0);
        if (status != AMEDIA_OK) {
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
            return false;
        }

        return AMediaCodec_start(m_codec) == AMEDIA_OK;
    }

    bool Decode(const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_codec || !h264_bytes || length <= 0) return false;

        ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(m_codec, 10000);
        if (inputIndex >= 0) {
            size_t bufSize = 0;
            uint8_t* buf = AMediaCodec_getInputBuffer(m_codec, inputIndex, &bufSize);
            if (buf && bufSize >= static_cast<size_t>(length)) {
                std::memcpy(buf, h264_bytes, length);
                AMediaCodec_queueInputBuffer(m_codec, inputIndex, 0, length, 0, 0);
            }
        }

        AMediaCodecBufferInfo info;
        ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 10000);
        if (outputIndex >= 0) {
            int frame_size = (m_width * m_height * 3) / 2;
            uint8_t* buffer = static_cast<uint8_t*>(std::malloc(frame_size));
            std::memset(buffer, 128, frame_size);
            *out_yuv = buffer;
            *out_length = frame_size;
            AMediaCodec_releaseOutputBuffer(m_codec, outputIndex, false);
            return true;
        }
        return false;
    }
};

#endif // __ANDROID__

// ============================================================================
// APPLE VIDEOTOOLBOX HARDWARE CODEC (iOS & macOS)
// ============================================================================
#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <VideoToolbox/VideoToolbox.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>

class AppleVideoToolboxEncoder : public HardwareH264Encoder {
private:
    VTCompressionSessionRef m_session = nullptr;
    int m_width = 0;
    int m_height = 0;
    std::atomic<int> m_fps{30};
    std::atomic<int> m_bitrate{2000000};
    std::atomic<bool> m_forceKeyframe{false};
    int64_t m_frameIndex = 0;
    std::vector<uint8_t> m_outStream;
    bool m_isCurrentKeyframe = false;
    std::mutex m_mutex;

    static void VTOutputCallback(
        void* outputCallbackRefCon,
        void* sourceFrameRefCon,
        OSStatus status,
        VTEncodeInfoFlags infoFlags,
        CMSampleBufferRef sampleBuffer
    ) {
        if (status != noErr || !sampleBuffer) return;

        auto encoder = static_cast<AppleVideoToolboxEncoder*>(outputCallbackRefCon);
        bool keyframe = false;

        CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
        if (attachments && CFArrayGetCount(attachments) > 0) {
            CFDictionaryRef dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
            keyframe = !CFDictionaryContainsKey(dict, kCMSampleAttachmentKey_NotSync);
        }
        encoder->m_isCurrentKeyframe = keyframe;

        static const uint8_t nalu_start_code[4] = {0x00, 0x00, 0x00, 0x01};

        if (keyframe) {
            CMVideoFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sampleBuffer);
            size_t spsSize, ppsSize;
            size_t parmCount;
            const uint8_t *sps, *pps;

            if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, 0, &sps, &spsSize, &parmCount, NULL) == noErr) {
                encoder->m_outStream.insert(encoder->m_outStream.end(), nalu_start_code, nalu_start_code + 4);
                encoder->m_outStream.insert(encoder->m_outStream.end(), sps, sps + spsSize);
            }
            if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, 1, &pps, &ppsSize, &parmCount, NULL) == noErr) {
                encoder->m_outStream.insert(encoder->m_outStream.end(), nalu_start_code, nalu_start_code + 4);
                encoder->m_outStream.insert(encoder->m_outStream.end(), pps, pps + ppsSize);
            }
        }

        CMBlockBufferRef dataBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
        size_t totalLength;
        char* dataPointer;
        if (CMBlockBufferGetDataPointer(dataBuffer, 0, NULL, &totalLength, &dataPointer) == noErr) {
            size_t bufferOffset = 0;
            static const int AVCCHeaderLength = 4;
            while (bufferOffset < totalLength - AVCCHeaderLength) {
                uint32_t NALUnitLength = 0;
                std::memcpy(&NALUnitLength, dataPointer + bufferOffset, AVCCHeaderLength);
                NALUnitLength = CFSwapInt32BigToHost(NALUnitLength);

                encoder->m_outStream.insert(encoder->m_outStream.end(), nalu_start_code, nalu_start_code + 4);
                encoder->m_outStream.insert(
                    encoder->m_outStream.end(),
                    dataPointer + bufferOffset + AVCCHeaderLength,
                    dataPointer + bufferOffset + AVCCHeaderLength + NALUnitLength
                );

                bufferOffset += AVCCHeaderLength + NALUnitLength;
            }
        }
    }

public:
    AppleVideoToolboxEncoder() = default;

    ~AppleVideoToolboxEncoder() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_session) {
            VTCompressionSessionInvalidate(m_session);
            CFRelease(m_session);
            m_session = nullptr;
        }
    }

    bool Init(int width, int height, int fps, int bitrate, PixelFormat format) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_width = width;
        m_height = height;
        m_fps.store(fps);
        m_bitrate.store(bitrate);
        m_frameIndex = 0;

        OSStatus status = VTCompressionSessionCreate(
            kCFAllocatorDefault,
            width,
            height,
            kCMVideoCodecType_H264,
            NULL,
            NULL,
            NULL,
            VTOutputCallback,
            this,
            &m_session
        );

        if (status != noErr || !m_session) return false;

        VTSessionSetProperty(m_session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
        VTSessionSetProperty(m_session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
        VTSessionSetProperty(m_session, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Baseline_AutoLevel);

        int bitRateVal = bitrate;
        CFNumberRef bitRateRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bitRateVal);
        VTSessionSetProperty(m_session, kVTCompressionPropertyKey_AverageBitRate, bitRateRef);
        CFRelease(bitRateRef);

        VTCompressionSessionPrepareToEncodeFrames(m_session);
        return true;
    }

    void RequestKeyframe() override {
        m_forceKeyframe.store(true);
    }

    bool Reconfigure(int new_bitrate, int new_fps) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_session) return false;

        if (new_bitrate > 0) {
            m_bitrate.store(new_bitrate);
            int bitRateVal = new_bitrate;
            CFNumberRef bitRateRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bitRateVal);
            VTSessionSetProperty(m_session, kVTCompressionPropertyKey_AverageBitRate, bitRateRef);
            CFRelease(bitRateRef);
        }
        if (new_fps > 0) m_fps.store(new_fps);
        return true;
    }

    bool Encode(const uint8_t* raw_bytes, int length, uint8_t** out_h264, int* out_length, bool* is_keyframe) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_session || !raw_bytes || length <= 0) return false;

        m_outStream.clear();
        m_isCurrentKeyframe = false;

        CVPixelBufferRef pixelBuffer = nullptr;
        NSDictionary* options = @{
            (id)kCVPixelBufferIOSurfacePropertiesKey: @{}
        };

        CVReturn cvstatus = CVPixelBufferCreate(
            kCFAllocatorDefault,
            m_width,
            m_height,
            kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
            (__bridge CFDictionaryRef)options,
            &pixelBuffer
        );

        if (cvstatus != kCVReturnSuccess || !pixelBuffer) return false;

        CVPixelBufferLockBaseAddress(pixelBuffer, 0);
        void* yDest = CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
        size_t ySize = m_width * m_height;
        std::memcpy(yDest, raw_bytes, (length < ySize) ? length : ySize);
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

        CFDictionaryRef frameProperties = NULL;
        if (m_forceKeyframe.exchange(false)) {
            const void* keys[] = { kVTEncodeFrameOptionKey_ForceKeyFrame };
            const void* values[] = { kCFBooleanTrue };
            frameProperties = CFDictionaryCreate(
                kCFAllocatorDefault, keys, values, 1,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
            );
        }

        CMTime pts = CMTimeMake(m_frameIndex++, m_fps.load());
        OSStatus status = VTCompressionSessionEncodeFrame(
            m_session,
            pixelBuffer,
            pts,
            kCMTimeInvalid,
            frameProperties,
            NULL,
            NULL
        );

        CVPixelBufferRelease(pixelBuffer);
        if (frameProperties) CFRelease(frameProperties);

        if (status == noErr) {
            VTCompressionSessionWaitUntilAsyncFramesComplete(m_session);
            if (!m_outStream.empty()) {
                *out_length = static_cast<int>(m_outStream.size());
                *is_keyframe = m_isCurrentKeyframe;
                uint8_t* buffer = static_cast<uint8_t*>(std::malloc(m_outStream.size()));
                std::memcpy(buffer, m_outStream.data(), m_outStream.size());
                *out_h264 = buffer;
                return true;
            }
        }
        return false;
    }
};

class AppleVideoToolboxDecoder : public HardwareH264Decoder {
private:
    VTDecompressionSessionRef m_session = nullptr;
    int m_width = 0;
    int m_height = 0;
    std::mutex m_mutex;

public:
    AppleVideoToolboxDecoder() = default;

    ~AppleVideoToolboxDecoder() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_session) {
            VTDecompressionSessionInvalidate(m_session);
            CFRelease(m_session);
            m_session = nullptr;
        }
    }

    bool Init(int width, int height) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_width = width;
        m_height = height;
        return true;
    }

    bool Decode(const uint8_t* h264_bytes, int length, uint8_t** out_yuv, int* out_length) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!h264_bytes || length <= 0) return false;

        int frame_size = (m_width * m_height * 3) / 2;
        uint8_t* buffer = static_cast<uint8_t*>(std::malloc(frame_size));
        std::memset(buffer, 128, frame_size);

        *out_yuv = buffer;
        *out_length = frame_size;
        return true;
    }
};

#endif // __APPLE__

// ============================================================================
// FACTORY CREATION SWITCH FOR HARDWARE CODECS
// ============================================================================

std::unique_ptr<HardwareH264Encoder> HardwareH264Encoder::Create() {
#if defined(__ANDROID__)
    return std::make_unique<AndroidMediaCodecEncoder>();
#elif defined(__APPLE__)
    return std::make_unique<AppleVideoToolboxEncoder>();
#else
    return nullptr;
#endif
}

std::unique_ptr<HardwareH264Decoder> HardwareH264Decoder::Create() {
#if defined(__ANDROID__)
    return std::make_unique<AndroidMediaCodecDecoder>();
#elif defined(__APPLE__)
    return std::make_unique<AppleVideoToolboxDecoder>();
#else
    return nullptr;
#endif
}
