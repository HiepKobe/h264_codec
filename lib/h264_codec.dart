import 'dart:ffi' as ffi;
import 'dart:isolate';
import 'dart:typed_data';
import 'package:ffi/ffi.dart' as pkg_ffi;

import 'h264_ffi_bindings.dart';
import 'h264_codec_platform_interface.dart';

/// Plugin main interface
class H264Codec {
  Future<String?> getPlatformVersion() {
    return H264CodecPlatform.instance.getPlatformVersion();
  }

  H264Encoder createEncoder({
    required int width,
    required int height,
    int fps = 30,
    int bitrate = 2000000,
    H264PixelFormat pixelFormat = H264PixelFormat.nv21,
  }) {
    final encoder = H264Encoder(
      width: width,
      height: height,
      fps: fps,
      bitrate: bitrate,
      pixelFormat: pixelFormat,
    );
    encoder.init();
    return encoder;
  }

  H264Decoder createDecoder({
    required int width,
    required int height,
  }) {
    final decoder = H264Decoder(width: width, height: height);
    decoder.init();
    return decoder;
  }
}

enum H264PixelFormat {
  i420(0),
  nv21(1),
  nv12(2),
  yuv420p(3),
  bgra(4);

  final int value;
  const H264PixelFormat(this.value);
}

/// Class representing encoded H.264 NAL Units
class H264Frame {
  final Uint8List data;
  final bool isKeyFrame;

  H264Frame({required this.data, required this.isKeyFrame});
}

/// Class representing decoded YUV frame
class DecodedYuvFrame {
  final Uint8List yuvData;
  final int width;
  final int height;

  DecodedYuvFrame({required this.yuvData, required this.width, required this.height});
}

/// High-Level Flutter Class for Frame-by-Frame H.264 Encoding
class H264Encoder {
  final int width;
  final int height;
  final int fps;
  final int bitrate;
  final H264PixelFormat pixelFormat;

  ffi.Pointer<ffi.Void>? _handle;
  bool _isDisposed = false;

  H264Encoder({
    required this.width,
    required this.height,
    this.fps = 30,
    this.bitrate = 2000000,
    this.pixelFormat = H264PixelFormat.nv21,
  });

  /// Initialize the encoder instance
  bool init() {
    final bindings = H264FFIBindings();
    _handle = bindings.createEncoderInstance(width, height, fps, bitrate, pixelFormat.value);
    return _handle != null && _handle != ffi.nullptr;
  }

  /// Synchronously encode a raw pixel frame to H.264
  H264Frame? encode(Uint8List rawBytes) {
    if (_isDisposed || _handle == null || _handle == ffi.nullptr) {
      throw StateError('H264Encoder is not initialized or has been disposed.');
    }

    final bindings = H264FFIBindings();

    // 1. Allocate native memory for input frame
    final ffi.Pointer<ffi.Uint8> inputPtr = pkg_ffi.calloc<ffi.Uint8>(rawBytes.length);
    final Uint8List nativeInputList = inputPtr.asTypedList(rawBytes.length);
    nativeInputList.setAll(0, rawBytes);

    // 2. Allocate pointers for output return parameters
    final ffi.Pointer<ffi.Pointer<ffi.Uint8>> outH264Ptr = pkg_ffi.calloc<ffi.Pointer<ffi.Uint8>>();
    final ffi.Pointer<ffi.Int32> outLengthPtr = pkg_ffi.calloc<ffi.Int32>();
    final ffi.Pointer<ffi.Bool> isKeyframePtr = pkg_ffi.calloc<ffi.Bool>();

    try {
      final success = bindings.encodeFrameInstance(
        _handle!,
        inputPtr,
        rawBytes.length,
        outH264Ptr,
        outLengthPtr,
        isKeyframePtr,
      );

      if (success && outH264Ptr.value != ffi.nullptr && outLengthPtr.value > 0) {
        final int length = outLengthPtr.value;
        final bool isKeyframe = isKeyframePtr.value;
        final ffi.Pointer<ffi.Uint8> h264NativeBuffer = outH264Ptr.value;

        // Copy native byte buffer to Dart Uint8List
        final Uint8List h264Bytes = Uint8List.fromList(h264NativeBuffer.asTypedList(length));

        // Safely free C++ allocated memory
        bindings.freeBuffer(h264NativeBuffer);

        return H264Frame(data: h264Bytes, isKeyFrame: isKeyframe);
      }
      return null;
    } finally {
      pkg_ffi.calloc.free(inputPtr);
      pkg_ffi.calloc.free(outH264Ptr);
      pkg_ffi.calloc.free(outLengthPtr);
      pkg_ffi.calloc.free(isKeyframePtr);
    }
  }

  /// Asynchronously encode frame on a separate Dart Isolate
  Future<H264Frame?> encodeAsync(Uint8List rawBytes) async {
    final w = width;
    final h = height;
    final f = fps;
    final b = bitrate;
    final pf = pixelFormat;

    return Isolate.run(() {
      final encoder = H264Encoder(
        width: w,
        height: h,
        fps: f,
        bitrate: b,
        pixelFormat: pf,
      );
      if (!encoder.init()) return null;
      try {
        return encoder.encode(rawBytes);
      } finally {
        encoder.dispose();
      }
    });
  }

  /// Release native resources
  void dispose() {
    if (_isDisposed) return;
    if (_handle != null && _handle != ffi.nullptr) {
      H264FFIBindings().destroyEncoderInstance(_handle!);
      _handle = null;
    }
    _isDisposed = true;
  }
}

/// High-Level Flutter Class for Frame-by-Frame H.264 Decoding
class H264Decoder {
  final int width;
  final int height;

  ffi.Pointer<ffi.Void>? _handle;
  bool _isDisposed = false;

  H264Decoder({
    required this.width,
    required this.height,
  });

  /// Initialize decoder instance
  bool init() {
    final bindings = H264FFIBindings();
    _handle = bindings.createDecoderInstance(width, height);
    return _handle != null && _handle != ffi.nullptr;
  }

  /// Synchronously decode H.264 packet to YUV I420 raw frame
  DecodedYuvFrame? decode(Uint8List h264Bytes) {
    if (_isDisposed || _handle == null || _handle == ffi.nullptr) {
      throw StateError('H264Decoder is not initialized or has been disposed.');
    }

    final bindings = H264FFIBindings();

    // 1. Allocate native memory for H.264 input stream
    final ffi.Pointer<ffi.Uint8> inputPtr = pkg_ffi.calloc<ffi.Uint8>(h264Bytes.length);
    final Uint8List nativeInputList = inputPtr.asTypedList(h264Bytes.length);
    nativeInputList.setAll(0, h264Bytes);

    // 2. Allocate return pointer arguments
    final ffi.Pointer<ffi.Pointer<ffi.Uint8>> outYuvPtr = pkg_ffi.calloc<ffi.Pointer<ffi.Uint8>>();
    final ffi.Pointer<ffi.Int32> outLengthPtr = pkg_ffi.calloc<ffi.Int32>();

    try {
      final success = bindings.decodeFrameInstance(
        _handle!,
        inputPtr,
        h264Bytes.length,
        outYuvPtr,
        outLengthPtr,
      );

      if (success && outYuvPtr.value != ffi.nullptr && outLengthPtr.value > 0) {
        final int length = outLengthPtr.value;
        final ffi.Pointer<ffi.Uint8> yuvNativeBuffer = outYuvPtr.value;

        // Copy decoded YUV data to Dart memory space
        final Uint8List yuvBytes = Uint8List.fromList(yuvNativeBuffer.asTypedList(length));

        // Free C++ native memory
        bindings.freeBuffer(yuvNativeBuffer);

        return DecodedYuvFrame(yuvData: yuvBytes, width: width, height: height);
      }
      return null;
    } finally {
      pkg_ffi.calloc.free(inputPtr);
      pkg_ffi.calloc.free(outYuvPtr);
      pkg_ffi.calloc.free(outLengthPtr);
    }
  }

  /// Asynchronously decode frame on a separate Dart Isolate
  Future<DecodedYuvFrame?> decodeAsync(Uint8List h264Bytes) async {
    final w = width;
    final h = height;

    return Isolate.run(() {
      final decoder = H264Decoder(width: w, height: h);
      if (!decoder.init()) return null;
      try {
        return decoder.decode(h264Bytes);
      } finally {
        decoder.dispose();
      }
    });
  }

  /// Release native resources
  void dispose() {
    if (_isDisposed) return;
    if (_handle != null && _handle != ffi.nullptr) {
      H264FFIBindings().destroyDecoderInstance(_handle!);
      _handle = null;
    }
    _isDisposed = true;
  }
}
