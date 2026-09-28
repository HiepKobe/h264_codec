import 'package:flutter_test/flutter_test.dart';
import 'package:h264_codec/h264_codec.dart';
import 'package:h264_codec/h264_codec_platform_interface.dart';
import 'package:h264_codec/h264_codec_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockH264CodecPlatform
    with MockPlatformInterfaceMixin
    implements H264CodecPlatform {
  @override
  Future<String?> getPlatformVersion() => Future.value('42');
}

void main() {
  final H264CodecPlatform initialPlatform = H264CodecPlatform.instance;

  test('$MethodChannelH264Codec is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelH264Codec>());
  });

  test('getPlatformVersion', () async {
    H264Codec h264CodecPlugin = H264Codec();
    MockH264CodecPlatform fakePlatform = MockH264CodecPlatform();
    H264CodecPlatform.instance = fakePlatform;

    expect(await h264CodecPlugin.getPlatformVersion(), '42');
  });

  test('createEncoder and createDecoder instantiates objects', () {
    H264Codec h264CodecPlugin = H264Codec();
    final encoder = h264CodecPlugin.createEncoder(width: 640, height: 480);
    expect(encoder.width, 640);
    expect(encoder.height, 480);

    final decoder = h264CodecPlugin.createDecoder(width: 640, height: 480);
    expect(decoder.width, 640);
    expect(decoder.height, 480);
  });
}
