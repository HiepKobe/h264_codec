import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/widgets.dart';
import 'h264_codec.dart';

/// App Lifecycle Manager for H.264 Codec instances.
/// Automatically handles background pausing and foreground recovery.
class H264LifecycleManager with WidgetsBindingObserver {
  final H264Encoder encoder;
  final H264Decoder decoder;

  bool _isAppActive = true;
  bool _isReinitializing = false;

  final StreamController<bool> _codecStateController = StreamController<bool>.broadcast();

  /// Stream emitting codec active state (true when foreground/active, false when paused)
  Stream<bool> get onCodecStateChanged => _codecStateController.stream;

  H264LifecycleManager({
    required this.encoder,
    required this.decoder,
  }) {
    WidgetsBinding.instance.addObserver(this);
  }

  bool get isAppActive => _isAppActive && !_isReinitializing;

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    super.didChangeAppLifecycleState(state);

    switch (state) {
      case AppLifecycleState.paused:
      case AppLifecycleState.inactive:
      case AppLifecycleState.detached:
      case AppLifecycleState.hidden:
        _handleAppPaused();
        break;
      case AppLifecycleState.resumed:
        _handleAppResumed();
        break;
    }
  }

  /// 1. Safely pause native codec session when app goes to background
  void _handleAppPaused() {
    if (!_isAppActive) return;
    _isAppActive = false;
    encoder.pause();
    _codecStateController.add(false);
    debugPrint('[H264Lifecycle] App Paused -> Native Codec Session safely paused.');
  }

  /// 2. Recover and warm up native codec session when app resumes foreground
  Future<void> _handleAppResumed() async {
    if (_isAppActive) return;
    _isReinitializing = true;
    debugPrint('[H264Lifecycle] App Resumed -> Re-initializing & Warming up Codec...');

    try {
      // Resume encoder pipeline
      encoder.resume();

      // Dynamic re-configuration if session parameters changed
      encoder.reconfigure(newBitrate: encoder.bitrate, newFps: encoder.fps);

      // Request IDR keyframe immediately to prevent black screen / corruption
      encoder.requestKeyframe();

      // Short delay (50ms) for hardware acceleration pipeline warm-up
      await Future.delayed(const Duration(milliseconds: 50));

      _isAppActive = true;
      _codecStateController.add(true);
      debugPrint('[H264Lifecycle] Codec Session Recovery Completed Successfully!');
    } catch (e) {
      debugPrint('[H264Lifecycle] Error recovering codec session: $e');
    } finally {
      _isReinitializing = false;
    }
  }

  /// Safe encode wrapper ignoring frames while backgrounded or recovering
  H264Frame? safeEncode(Uint8List rawBytes) {
    if (!isAppActive) {
      return null;
    }
    return encoder.encode(rawBytes);
  }

  /// Safe decode wrapper ignoring frames while backgrounded or recovering
  DecodedYuvFrame? safeDecode(Uint8List h264Bytes) {
    if (!isAppActive) {
      return null;
    }
    return decoder.decode(h264Bytes);
  }

  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _codecStateController.close();
  }
}
