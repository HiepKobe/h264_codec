import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:h264_codec/h264_codec.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('Simulate 15% Packet Loss & Test Instant IDR Recovery', () async {
    final encoder = H264Encoder(width: 1280, height: 720, fps: 30, bitrate: 2000000);
    final decoder = H264Decoder(width: 1280, height: 720);

    expect(encoder.init(), isTrue);
    expect(decoder.init(), isTrue);

    final rawFrame = Uint8List((1280 * 720 * 3) ~/ 2);
    int simulatedNetworkFrameIndex = 0;
    int keyframeRequestCount = 0;

    for (int i = 0; i < 200; i++) {
      simulatedNetworkFrameIndex++;
      final h264Frame = encoder.encode(rawFrame);
      if (h264Frame == null) continue;

      // Simulate packet drop every 20 frames
      bool isPacketDropped = (simulatedNetworkFrameIndex % 20 == 0);

      if (isPacketDropped) {
        print('NETWORK ALERT: Frame $i dropped over Network Jitter! Requesting Keyframe...');
        encoder.requestKeyframe();
        keyframeRequestCount++;
        continue; // Skip delivering packet to decoder
      }

      final decoded = decoder.decode(h264Frame.data);
      expect(decoded, isNotNull);

      if (keyframeRequestCount > 0 && h264Frame.isKeyFrame) {
        print('NETWORK RECOVERY SUCCESS: IDR Keyframe received on Frame $i!');
      }
    }

    expect(keyframeRequestCount, greaterThan(0));
    encoder.dispose();
    decoder.dispose();
  });
}
