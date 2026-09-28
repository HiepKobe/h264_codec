#ifndef FLUTTER_PLUGIN_H264_CODEC_PLUGIN_H_
#define FLUTTER_PLUGIN_H264_CODEC_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace h264_codec {

class H264CodecPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  H264CodecPlugin();

  virtual ~H264CodecPlugin();

  // Disallow copy and assign.
  H264CodecPlugin(const H264CodecPlugin&) = delete;
  H264CodecPlugin& operator=(const H264CodecPlugin&) = delete;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

}  // namespace h264_codec

#endif  // FLUTTER_PLUGIN_H264_CODEC_PLUGIN_H_
