import 'dart:ffi' as ffi;
import 'dart:io';

// Native C Function Typedefs
typedef NativeInitEncoder = ffi.Bool Function(ffi.Int32 width, ffi.Int32 height, ffi.Int32 fps, ffi.Int32 bitrate);
typedef DartInitEncoder = bool Function(int width, int height, int fps, int bitrate);

typedef NativeEncodeFrame = ffi.Bool Function(
    ffi.Pointer<ffi.Uint8> rawYuv,
    ffi.Int32 length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outH264,
    ffi.Pointer<ffi.Int32> outLength,
    ffi.Pointer<ffi.Bool> isKeyframe
);
typedef DartEncodeFrame = bool Function(
    ffi.Pointer<ffi.Uint8> rawYuv,
    int length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outH264,
    ffi.Pointer<ffi.Int32> outLength,
    ffi.Pointer<ffi.Bool> isKeyframe
);

typedef NativeInitDecoder = ffi.Bool Function(ffi.Int32 width, ffi.Int32 height);
typedef DartInitDecoder = bool Function(int width, int height);

typedef NativeDecodeFrame = ffi.Bool Function(
    ffi.Pointer<ffi.Uint8> h264Bytes,
    ffi.Int32 length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outYuv,
    ffi.Pointer<ffi.Int32> outLength
);
typedef DartDecodeFrame = bool Function(
    ffi.Pointer<ffi.Uint8> h264Bytes,
    int length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outYuv,
    ffi.Pointer<ffi.Int32> outLength
);

typedef NativeFreeBuffer = ffi.Void Function(ffi.Pointer<ffi.Uint8> ptr);
typedef DartFreeBuffer = void Function(ffi.Pointer<ffi.Uint8> ptr);

typedef NativeDestroyEncoder = ffi.Void Function();
typedef DartDestroyEncoder = void Function();

typedef NativeDestroyDecoder = ffi.Void Function();
typedef DartDestroyDecoder = void Function();

// Multi-Instance Typedefs
typedef NativeCreateEncoderInstance = ffi.Pointer<ffi.Void> Function(ffi.Int32 width, ffi.Int32 height, ffi.Int32 fps, ffi.Int32 bitrate, ffi.Int32 pixelFormat);
typedef DartCreateEncoderInstance = ffi.Pointer<ffi.Void> Function(int width, int height, int fps, int bitrate, int pixelFormat);

typedef NativeEncodeFrameInstance = ffi.Bool Function(
    ffi.Pointer<ffi.Void> handle,
    ffi.Pointer<ffi.Uint8> rawBytes,
    ffi.Int32 length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outH264,
    ffi.Pointer<ffi.Int32> outLength,
    ffi.Pointer<ffi.Bool> isKeyframe
);
typedef DartEncodeFrameInstance = bool Function(
    ffi.Pointer<ffi.Void> handle,
    ffi.Pointer<ffi.Uint8> rawBytes,
    int length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outH264,
    ffi.Pointer<ffi.Int32> outLength,
    ffi.Pointer<ffi.Bool> isKeyframe
);

typedef NativeDestroyEncoderInstance = ffi.Void Function(ffi.Pointer<ffi.Void> handle);
typedef DartDestroyEncoderInstance = void Function(ffi.Pointer<ffi.Void> handle);

typedef NativeCreateDecoderInstance = ffi.Pointer<ffi.Void> Function(ffi.Int32 width, ffi.Int32 height);
typedef DartCreateDecoderInstance = ffi.Pointer<ffi.Void> Function(int width, int height);

typedef NativeDecodeFrameInstance = ffi.Bool Function(
    ffi.Pointer<ffi.Void> handle,
    ffi.Pointer<ffi.Uint8> h264Bytes,
    ffi.Int32 length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outYuv,
    ffi.Pointer<ffi.Int32> outLength
);
typedef DartDecodeFrameInstance = bool Function(
    ffi.Pointer<ffi.Void> handle,
    ffi.Pointer<ffi.Uint8> h264Bytes,
    int length,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> outYuv,
    ffi.Pointer<ffi.Int32> outLength
);

typedef NativeDestroyDecoderInstance = ffi.Void Function(ffi.Pointer<ffi.Void> handle);
typedef DartDestroyDecoderInstance = void Function(ffi.Pointer<ffi.Void> handle);

/// Low-level FFI binding class responsible for loading dynamic library and exposing C functions.
class H264FFIBindings {
  static final H264FFIBindings _instance = H264FFIBindings._internal();
  factory H264FFIBindings() => _instance;

  late final ffi.DynamicLibrary nativeLib;

  // Global C API Functions
  late final DartInitEncoder initEncoder;
  late final DartEncodeFrame encodeFrame;
  late final DartInitDecoder initDecoder;
  late final DartDecodeFrame decodeFrame;
  late final DartFreeBuffer freeBuffer;
  late final DartDestroyEncoder destroyEncoder;
  late final DartDestroyDecoder destroyDecoder;

  // Multi-Instance C API Functions
  late final DartCreateEncoderInstance createEncoderInstance;
  late final DartEncodeFrameInstance encodeFrameInstance;
  late final DartDestroyEncoderInstance destroyEncoderInstance;
  late final DartCreateDecoderInstance createDecoderInstance;
  late final DartDecodeFrameInstance decodeFrameInstance;
  late final DartDestroyDecoderInstance destroyDecoderInstance;

  H264FFIBindings._internal() {
    nativeLib = _loadDynamicLibrary();

    initEncoder = nativeLib
        .lookup<ffi.NativeFunction<NativeInitEncoder>>('init_encoder')
        .asFunction<DartInitEncoder>();

    encodeFrame = nativeLib
        .lookup<ffi.NativeFunction<NativeEncodeFrame>>('encode_frame')
        .asFunction<DartEncodeFrame>();

    initDecoder = nativeLib
        .lookup<ffi.NativeFunction<NativeInitDecoder>>('init_decoder')
        .asFunction<DartInitDecoder>();

    decodeFrame = nativeLib
        .lookup<ffi.NativeFunction<NativeDecodeFrame>>('decode_frame')
        .asFunction<DartDecodeFrame>();

    freeBuffer = nativeLib
        .lookup<ffi.NativeFunction<NativeFreeBuffer>>('free_buffer')
        .asFunction<DartFreeBuffer>();

    destroyEncoder = nativeLib
        .lookup<ffi.NativeFunction<NativeDestroyEncoder>>('destroy_encoder')
        .asFunction<DartDestroyEncoder>();

    destroyDecoder = nativeLib
        .lookup<ffi.NativeFunction<NativeDestroyDecoder>>('destroy_decoder')
        .asFunction<DartDestroyDecoder>();

    createEncoderInstance = nativeLib
        .lookup<ffi.NativeFunction<NativeCreateEncoderInstance>>('create_encoder_instance')
        .asFunction<DartCreateEncoderInstance>();

    encodeFrameInstance = nativeLib
        .lookup<ffi.NativeFunction<NativeEncodeFrameInstance>>('encode_frame_instance')
        .asFunction<DartEncodeFrameInstance>();

    destroyEncoderInstance = nativeLib
        .lookup<ffi.NativeFunction<NativeDestroyEncoderInstance>>('destroy_encoder_instance')
        .asFunction<DartDestroyEncoderInstance>();

    createDecoderInstance = nativeLib
        .lookup<ffi.NativeFunction<NativeCreateDecoderInstance>>('create_decoder_instance')
        .asFunction<DartCreateDecoderInstance>();

    decodeFrameInstance = nativeLib
        .lookup<ffi.NativeFunction<NativeDecodeFrameInstance>>('decode_frame_instance')
        .asFunction<DartDecodeFrameInstance>();

    destroyDecoderInstance = nativeLib
        .lookup<ffi.NativeFunction<NativeDestroyDecoderInstance>>('destroy_decoder_instance')
        .asFunction<DartDestroyDecoderInstance>();
  }

  static ffi.DynamicLibrary _loadDynamicLibrary() {
    if (Platform.isAndroid) {
      return ffi.DynamicLibrary.open('libh264_bridge.so');
    } else if (Platform.isIOS || Platform.isMacOS) {
      return ffi.DynamicLibrary.process();
    } else if (Platform.isWindows) {
      return ffi.DynamicLibrary.open('h264_bridge.dll');
    } else if (Platform.isLinux) {
      return ffi.DynamicLibrary.open('libh264_bridge.so');
    } else {
      throw UnsupportedError('Platform not supported for H.264 FFI Codec');
    }
  }
}
