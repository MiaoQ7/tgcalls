#pragma once

#include <rtc_base/logging.h>
#include <rtc_base/ref_counted_object.h>
#include <modules/audio_device/audio_device_impl.h>

#include "FileAudioDevice.h"
#include "FileAudioDeviceDescriptor.h"
#include "RawAudioDevice.h"
#include "RawAudioDeviceDescriptor.h"
#include "P2PFileAudioDevice.h"
#include "P2PFileAudioDeviceDescriptor.h"
#include "P2PRawAudioDevice.h"
#include "P2PRawAudioDeviceDescriptor.h"

namespace rtc {
  class PlatformThread;
}  // namespace rtc

class WrappedAudioDeviceModuleImpl : public webrtc::AudioDeviceModule {
public:
  int32_t Init() {
    return 0;
  }
  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> Create(
    AudioLayer,
    webrtc::TaskQueueFactory *,
    std::shared_ptr<P2PFileAudioDeviceDescriptor>);

  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> Create(
      AudioLayer,
      webrtc::TaskQueueFactory *,
      std::shared_ptr<FileAudioDeviceDescriptor>);

  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> Create(
      AudioLayer,
      webrtc::TaskQueueFactory *,
      std::shared_ptr<RawAudioDeviceDescriptor>);

  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> Create(
      AudioLayer,
      webrtc::TaskQueueFactory *,
      std::shared_ptr<P2PRawAudioDeviceDescriptor>);


  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> CreateForTest(
      AudioLayer,
      webrtc::TaskQueueFactory *,
      std::shared_ptr<P2PFileAudioDeviceDescriptor>);

  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> CreateForTest(
      AudioLayer,
      webrtc::TaskQueueFactory *,
      std::shared_ptr<FileAudioDeviceDescriptor>);

  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> CreateForTest(
      AudioLayer,
      webrtc::TaskQueueFactory *,
      std::shared_ptr<RawAudioDeviceDescriptor>);

  static rtc::scoped_refptr<webrtc::AudioDeviceModuleImpl> CreateForTest(
      AudioLayer,
      webrtc::TaskQueueFactory *,
      std::shared_ptr<P2PRawAudioDeviceDescriptor>);
};
