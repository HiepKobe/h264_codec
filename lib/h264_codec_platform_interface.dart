import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'h264_codec_method_channel.dart';

abstract class H264CodecPlatform extends PlatformInterface {
  /// Constructs a H264CodecPlatform.
  H264CodecPlatform() : super(token: _token);

  static final Object _token = Object();

  static H264CodecPlatform _instance = MethodChannelH264Codec();

  /// The default instance of [H264CodecPlatform] to use.
  ///
  /// Defaults to [MethodChannelH264Codec].
  static H264CodecPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [H264CodecPlatform] when
  /// they register themselves.
  static set instance(H264CodecPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}
