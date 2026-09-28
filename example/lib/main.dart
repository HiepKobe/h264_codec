import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:h264_codec/h264_codec.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _platformVersion = 'Unknown';
  final _h264Plugin = H264Codec();

  // Codec instances
  H264Encoder? _encoder;
  H264Decoder? _decoder;

  // Recording State & Stats
  bool _isRecording = false;
  Timer? _recordingTimer;
  int _frameCount = 0;
  int _encodedFrameCount = 0;
  int _decodedFrameCount = 0;
  int _totalH264Bytes = 0;
  bool _lastIsKeyFrame = false;
  int _lastDecodedYuvSize = 0;
  int _currentBitrate = 2000000;
  String _statusText = 'Sẵn sàng ghi hình';

  static const int _width = 640;
  static const int _height = 480;

  @override
  void initState() {
    super.initState();
    initPlatformState();
  }

  @override
  void dispose() {
    _stopRecording();
    super.dispose();
  }

  Future<void> initPlatformState() async {
    String platformVersion;
    try {
      platformVersion =
          await _h264Plugin.getPlatformVersion() ?? 'Unknown platform version';
    } on PlatformException {
      platformVersion = 'Failed to get platform version.';
    }

    if (!mounted) return;

    setState(() {
      _platformVersion = platformVersion;
    });
  }

  /// Bắt đầu quá trình Ghi hình (Mã hóa + Giải mã H.264 thời gian thực)
  void _startRecording() {
    if (_isRecording) return;

    // Khởi tạo Encoder & Decoder
    _encoder = H264Encoder(
      width: _width,
      height: _height,
      fps: 30,
      bitrate: _currentBitrate,
      pixelFormat: H264PixelFormat.nv21,
    );

    _decoder = H264Decoder(
      width: _width,
      height: _height,
    );

    final bool encSuccess = _encoder!.init();
    final bool decSuccess = _decoder!.init();

    if (!encSuccess || !decSuccess) {
      setState(() {
        _statusText = 'Khởi tạo Native Codec thất bại!';
      });
      return;
    }

    setState(() {
      _isRecording = true;
      _frameCount = 0;
      _encodedFrameCount = 0;
      _decodedFrameCount = 0;
      _totalH264Bytes = 0;
      _statusText = 'Đang ghi hình & nén H.264 realtime...';
    });

    // Giả lập luồng Camera 30 FPS (Mỗi 33ms gửi 1 Frame thô)
    _recordingTimer = Timer.periodic(const Duration(milliseconds: 33), (timer) {
      _processFrame();
    });
  }

  /// Dừng quá trình ghi hình và giải phóng tài nguyên Native
  void _stopRecording() {
    if (!_isRecording) return;

    _recordingTimer?.cancel();
    _recordingTimer = null;

    _encoder?.dispose();
    _encoder = null;

    _decoder?.dispose();
    _decoder = null;

    setState(() {
      _isRecording = false;
      _statusText = 'Đã dừng ghi hình. Đã giải phóng Codec Native.';
    });
  }

  /// Ép nén ra 1 IDR Keyframe ngay lập tức
  void _forceKeyframe() {
    if (_isRecording && _encoder != null) {
      _encoder!.requestKeyframe();
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Đã yêu cầu Ép Keyframe (Force IDR NALU)'),
          duration: Duration(milliseconds: 800),
        ),
      );
    }
  }

  /// Đổi bitrate động (Adaptive Bitrate Realtime Test)
  void _changeBitrate(int newBitrate) {
    setState(() {
      _currentBitrate = newBitrate;
    });
    if (_isRecording && _encoder != null) {
      _encoder!.reconfigure(newBitrate: newBitrate);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Đã đổi Bitrate động: ${(newBitrate / 1000).toStringAsFixed(0)} Kbps'),
          duration: const Duration(milliseconds: 800),
        ),
      );
    }
  }

  /// Giả lập 1 Frame ảnh thô NV21 từ Camera và chạy luồng Mã hóa -> Giải mã H.264
  void _processFrame() {
    if (!_isRecording || _encoder == null || _decoder == null) return;

    _frameCount++;

    final int frameSize = (_width * _height * 1.5).toInt();
    final Uint8List rawFrameData = Uint8List(frameSize);

    final int fillByte = (_frameCount * 5) % 256;
    rawFrameData.fillRange(0, frameSize, fillByte);

    final H264Frame? encodedFrame = _encoder!.encode(rawFrameData);

    if (encodedFrame != null) {
      _encodedFrameCount++;
      _totalH264Bytes += encodedFrame.data.length;
      _lastIsKeyFrame = encodedFrame.isKeyFrame;

      final DecodedYuvFrame? decodedFrame = _decoder!.decode(encodedFrame.data);

      if (decodedFrame != null) {
        _decodedFrameCount++;
        _lastDecodedYuvSize = decodedFrame.yuvData.length;
      }
    }

    if (mounted) {
      setState(() {});
    }
  }

  void _resetStats() {
    setState(() {
      _frameCount = 0;
      _encodedFrameCount = 0;
      _decodedFrameCount = 0;
      _totalH264Bytes = 0;
      _lastIsKeyFrame = false;
      _lastDecodedYuvSize = 0;
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        colorSchemeSeed: Colors.indigo,
      ),
      home: Scaffold(
        appBar: AppBar(
          title: const Text('H.264 Codec Realtime Test'),
          elevation: 2,
        ),
        body: SingleChildScrollView(
          padding: const EdgeInsets.all(16.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // Info Card
              Card(
                color: Colors.indigo.shade50,
                child: Padding(
                  padding: const EdgeInsets.all(12.0),
                  child: Row(
                    children: [
                      const Icon(Icons.info_outline, color: Colors.indigo),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          'Nền tảng: $_platformVersion\nĐộ phân giải: ${_width}x$_height (NV21) @ ${(_currentBitrate / 1000).toStringAsFixed(0)} Kbps',
                          style: const TextStyle(fontWeight: FontWeight.w500),
                        ),
                      ),
                    ],
                  ),
                ),
              ),

              const SizedBox(height: 16),

              // Status Card
              Card(
                elevation: 3,
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Column(
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Container(
                            width: 12,
                            height: 12,
                            decoration: BoxDecoration(
                              shape: BoxShape.circle,
                              color: _isRecording ? Colors.red : Colors.grey,
                            ),
                          ),
                          const SizedBox(width: 8),
                          Text(
                            _isRecording ? 'ĐANG GHI HÌNH' : 'ĐÃ DỪNG',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                              color: _isRecording ? Colors.red : Colors.grey.shade700,
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Text(
                        _statusText,
                        style: TextStyle(color: Colors.grey.shade700),
                        textAlign: TextAlign.center,
                      ),
                    ],
                  ),
                ),
              ),

              const SizedBox(height: 16),

              // Control Buttons (Ghi hình / Dừng)
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: _isRecording ? null : _startRecording,
                      icon: const Icon(Icons.videocam),
                      label: const Text('Ghi hình'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.red.shade600,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(vertical: 14),
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(10),
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: _isRecording ? _stopRecording : null,
                      icon: const Icon(Icons.stop),
                      label: const Text('Dừng'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.grey.shade800,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(vertical: 14),
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(10),
                        ),
                      ),
                    ),
                  ),
                ],
              ),

              const SizedBox(height: 12),

              // WebRTC Low-Latency / Dynamic Codec Control Buttons
              Wrap(
                spacing: 8,
                runSpacing: 8,
                alignment: WrapAlignment.center,
                children: [
                  ActionChip(
                    avatar: const Icon(Icons.key, size: 16),
                    label: const Text('Ép Keyframe (Force IDR)'),
                    onPressed: _isRecording ? _forceKeyframe : null,
                  ),
                  ActionChip(
                    avatar: const Icon(Icons.speed, size: 16),
                    label: const Text('Bitrate 500 Kbps'),
                    onPressed: _isRecording ? () => _changeBitrate(500000) : null,
                  ),
                  ActionChip(
                    avatar: const Icon(Icons.high_quality, size: 16),
                    label: const Text('Bitrate 4 Mbps'),
                    onPressed: _isRecording ? () => _changeBitrate(4000000) : null,
                  ),
                ],
              ),

              const SizedBox(height: 20),

              // Realtime Codec Statistics
              const Text(
                'Thống kê Realtime Codec',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 10),

              GridView.count(
                crossAxisCount: 2,
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                childAspectRatio: 2.2,
                crossAxisSpacing: 10,
                mainAxisSpacing: 10,
                children: [
                  _buildStatTile(
                    'Frame Đã Nén',
                    '$_encodedFrameCount / $_frameCount',
                    Icons.compress,
                    Colors.blue,
                  ),
                  _buildStatTile(
                    'Frame Đã Giải Mã',
                    '$_decodedFrameCount',
                    Icons.expand,
                    Colors.green,
                  ),
                  _buildStatTile(
                    'Dung Lượng H.264',
                    '${(_totalH264Bytes / 1024).toStringAsFixed(1)} KB',
                    Icons.data_usage,
                    Colors.orange,
                  ),
                  _buildStatTile(
                    'Frame Loại',
                    _lastIsKeyFrame ? 'I-Frame (Key)' : 'P-Frame (Delta)',
                    Icons.filter_hdr,
                    _lastIsKeyFrame ? Colors.purple : Colors.teal,
                  ),
                ],
              ),

              const SizedBox(height: 12),

              Card(
                child: ListTile(
                  leading: const Icon(Icons.image, color: Colors.blueGrey),
                  title: const Text('Decoded YUV Frame Buffer'),
                  subtitle: Text('Size: $_lastDecodedYuvSize bytes (${_width}x$_height)'),
                ),
              ),

              const SizedBox(height: 12),

              OutlinedButton.icon(
                onPressed: _resetStats,
                icon: const Icon(Icons.refresh),
                label: const Text('Reset Thống kê'),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildStatTile(String label, String value, IconData icon, Color color) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: color.withOpacity(0.1),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: color.withOpacity(0.3)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Row(
            children: [
              Icon(icon, size: 18, color: color),
              const SizedBox(width: 6),
              Expanded(
                child: Text(
                  label,
                  style: TextStyle(
                    fontSize: 12,
                    color: Colors.grey.shade800,
                    fontWeight: FontWeight.w500,
                  ),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            ],
          ),
          const SizedBox(height: 4),
          Text(
            value,
            style: TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.bold,
              color: color,
            ),
          ),
        ],
      ),
    );
  }
}
