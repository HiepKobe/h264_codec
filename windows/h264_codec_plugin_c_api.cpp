#include "include/h264_codec/h264_codec_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "h264_codec_plugin.h"

void H264CodecPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  h264_codec::H264CodecPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
