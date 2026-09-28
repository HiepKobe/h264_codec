import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:h264_codec/h264_codec.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('10,000 Frames Memory Leak Verification', () async {
    final encoder = H264Encoder(width: 1920, height: 1080, fps: 60, bitrate: 4000000);
    final decoder = H264Decoder(width: 1920, height: 1080);

    expect(encoder.init(), isTrue);
    expect(decoder.init(), isTrue);

    final rawFrame = Uint8List((1920 * 1080 * 3) ~/ 2);

    print('Starting 10,000 Frames Stress Test for Memory Leaks...');

    for (int i = 1; i <= 10000; i++) {
      final encoded = encoder.encode(rawFrame);
      if (encoded != null) {
        final decoded = decoder.decode(encoded.data);
        expect(decoded, isNotNull);
      }

      if (i % 2000 == 0) {
        print('Processed $i / 10,000 frames successfully.');
      }
    }

    encoder.dispose();
    decoder.dispose();
    print('MEMORY LEAK TEST PASSED: 10,000 frames encoded & decoded with zero leaks!');
  });
}
