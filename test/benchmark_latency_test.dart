import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:h264_codec/h264_codec.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('Glass-to-Glass Latency Benchmark Target < 50ms', () async {
    final encoder = H264Encoder(width: 1280, height: 720, fps: 60, bitrate: 3000000);
    final decoder = H264Decoder(width: 1280, height: 720);

    expect(encoder.init(), isTrue);
    expect(decoder.init(), isTrue);

    final rawYuvFrame = Uint8List((1280 * 720 * 3) ~/ 2); // Dummy NV21 frame
    final List<double> latencyList = [];

    // Warmup 10 frames
    for (int i = 0; i < 10; i++) {
      final f = encoder.encode(rawYuvFrame);
      if (f != null) decoder.decode(f.data);
    }

    // Benchmark 100 frames
    for (int i = 0; i < 100; i++) {
      final int startUs = DateTime.now().microsecondsSinceEpoch;

      final h264Frame = encoder.encode(rawYuvFrame);
      expect(h264Frame, isNotNull);

      final decodedYuv = decoder.decode(h264Frame!.data);
      expect(decodedYuv, isNotNull);

      final int endUs = DateTime.now().microsecondsSinceEpoch;
      final double latencyMs = (endUs - startUs) / 1000.0;
      latencyList.add(latencyMs);
    }

    final double avgLatency = latencyList.reduce((a, b) => a + b) / latencyList.length;
    print('==================================================');
    print('AVERAGE GLASS-TO-GLASS LATENCY: ${avgLatency.toStringAsFixed(2)} ms');
    print('==================================================');

    expect(avgLatency, lessThan(50.0), reason: 'Latency must be under 50ms for Realtime WebRTC/Streaming');

    encoder.dispose();
    decoder.dispose();
  });
}
