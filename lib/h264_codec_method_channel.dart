import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'h264_codec_platform_interface.dart';

/// An implementation of [H264CodecPlatform] that uses method channels.
class MethodChannelH264Codec extends H264CodecPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('h264_codec');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>(
      'getPlatformVersion',
    );
    return version;
  }
}
