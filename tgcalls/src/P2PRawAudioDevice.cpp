#include "P2PRawAudioDevice.h"

#include <cstring>

#include <modules/audio_device/audio_device_impl.h>
#include <rtc_base/ref_counted_object.h>
#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/time_utils.h>
#include <system_wrappers/include/sleep.h>

// TODO set from Python
const int kRecordingFixedSampleRate = 48000;
const size_t kRecordingNumChannels = 1;
const int kPlayoutFixedSampleRate = 48000;
const size_t kPlayoutNumChannels = 1;
const size_t kPlayoutBufferSize =
    kPlayoutFixedSampleRate / 100 * kPlayoutNumChannels * 2;
const size_t kRecordingBufferSize =
    kRecordingFixedSampleRate / 100 * kRecordingNumChannels * 2;

P2PRawAudioDevice::P2PRawAudioDevice(std::shared_ptr<P2PRawAudioDeviceDescriptor> RawAudioDeviceDescriptor)
    : _ptrAudioBuffer(nullptr),
      _recordingBuffer(nullptr),
      _playoutBuffer(nullptr),
      _playoutFramesLeft(0),
      _recordingBufferSizeIn10MS(0),
      _recordingFramesIn10MS(0),
      _playoutFramesIn10MS(0),
      _playing(false),
      _recording(false),
      _lastCallPlayoutMillis(0),
      _lastCallRecordMillis(0),
      _rawAudioDeviceDescriptor(std::move(RawAudioDeviceDescriptor)) {}

P2PRawAudioDevice::~P2PRawAudioDevice() = default;

int32_t P2PRawAudioDevice::ActiveAudioLayer(
    webrtc::AudioDeviceModule::AudioLayer &audioLayer) const {
  return -1;
}

webrtc::AudioDeviceGeneric::InitStatus P2PRawAudioDevice::Init() {
  return InitStatus::OK;
}

int32_t P2PRawAudioDevice::Terminate() {
  return 0;
}

bool P2PRawAudioDevice::Initialized() const {
  return true;
}

int16_t P2PRawAudioDevice::PlayoutDevices() {
  return 1;
}

int16_t P2PRawAudioDevice::RecordingDevices() {
  return 1;
}

int32_t P2PRawAudioDevice::PlayoutDeviceName(uint16_t index,
                                           char name[webrtc::kAdmMaxDeviceNameSize],
                                           char guid[webrtc::kAdmMaxGuidSize]) {
  const char *kName = "dummy_device";
  const char *kGuid = "dummy_device_unique_id";
  if (index < 1) {
    memset(name, 0, webrtc::kAdmMaxDeviceNameSize);
    memset(guid, 0, webrtc::kAdmMaxGuidSize);
    memcpy(name, kName, strlen(kName));
    memcpy(guid, kGuid, strlen(guid));
    return 0;
  }
  return -1;
}

int32_t P2PRawAudioDevice::RecordingDeviceName(uint16_t index,
                                             char name[webrtc::kAdmMaxDeviceNameSize],
                                             char guid[webrtc::kAdmMaxGuidSize]) {
  const char *kName = "dummy_device";
  const char *kGuid = "dummy_device_unique_id";
  if (index < 1) {
    memset(name, 0, webrtc::kAdmMaxDeviceNameSize);
    memset(guid, 0, webrtc::kAdmMaxGuidSize);
    memcpy(name, kName, strlen(kName));
    memcpy(guid, kGuid, strlen(guid));
    return 0;
  }
  return -1;
}

int32_t P2PRawAudioDevice::SetPlayoutDevice(uint16_t index) {
  if (index == 0) {
    _playout_index = index;
    return 0;
  }
  return -1;
}

int32_t P2PRawAudioDevice::SetPlayoutDevice(
    webrtc::AudioDeviceModule::WindowsDeviceType device) {
  return -1;
}

int32_t P2PRawAudioDevice::SetRecordingDevice(uint16_t index) {
  if (index == 0) {
    _record_index = index;
    return _record_index;
  }
  return -1;
}

int32_t P2PRawAudioDevice::SetRecordingDevice(
    webrtc::AudioDeviceModule::WindowsDeviceType device) {
  return -1;
}

int32_t P2PRawAudioDevice::PlayoutIsAvailable(bool &available) {
  if (_playout_index == 0) {
    available = true;
    return _playout_index;
  }
  available = false;
  return -1;
}

int32_t P2PRawAudioDevice::InitPlayout() {
  webrtc::MutexLock lock(&mutex_);

  if (_playing) {
    return -1;
  }

  _playoutFramesIn10MS = static_cast<size_t>(kPlayoutFixedSampleRate / 100);

  if (_ptrAudioBuffer) {
    _ptrAudioBuffer->SetPlayoutSampleRate(kPlayoutFixedSampleRate);
    _ptrAudioBuffer->SetPlayoutChannels(kPlayoutNumChannels);
  }
  return 0;
}

bool P2PRawAudioDevice::PlayoutIsInitialized() const {
  return _playoutFramesIn10MS != 0;
}

int32_t P2PRawAudioDevice::RecordingIsAvailable(bool &available) {
  if (_record_index == 0) {
    available = true;
    return _record_index;
  }
  available = false;
  return -1;
}

int32_t P2PRawAudioDevice::InitRecording() {
  webrtc::MutexLock lock(&mutex_);

  if (_recording) {
    return -1;
  }

  _recordingFramesIn10MS = static_cast<size_t>(kRecordingFixedSampleRate / 100);

  if (_ptrAudioBuffer) {
    _ptrAudioBuffer->SetRecordingSampleRate(kRecordingFixedSampleRate);
    _ptrAudioBuffer->SetRecordingChannels(kRecordingNumChannels);
  }
  return 0;
}

bool P2PRawAudioDevice::RecordingIsInitialized() const {
  return _recordingFramesIn10MS != 0;
}

int32_t P2PRawAudioDevice::StartPlayout() {
  if (_playing) {
    return 0;
  }

  _playing = true;
  _playoutFramesLeft = 0;

  if (!_playoutBuffer) {
    _playoutBuffer = new int8_t[kPlayoutBufferSize];
  }
  if (!_playoutBuffer) {
    _playing = false;
    return -1;
  }

  _ptrThreadPlay.reset(new rtc::PlatformThread(
      PlayThreadFunc, this, "webrtc_audio_module_play_thread",
      rtc::kRealtimePriority));
  _ptrThreadPlay->Start();

  RTC_LOG(LS_INFO) << "Started playout capture Python callback";
  return 0;
}

int32_t P2PRawAudioDevice::StopPlayout() {
  {
    webrtc::MutexLock lock(&mutex_);
    _playing = false;
  }
  // stop playout thread first
  if (_ptrThreadPlay) {
    _ptrThreadPlay->Stop();
    _ptrThreadPlay.reset();
  }

  webrtc::MutexLock lock(&mutex_);

  _playoutFramesLeft = 0;
  delete[] _playoutBuffer;
  _playoutBuffer = nullptr;

  RTC_LOG(LS_INFO) << "Stopped playout capture to Python";
  return 0;
}

bool P2PRawAudioDevice::Playing() const {
  return _playing;
}

int32_t P2PRawAudioDevice::StartRecording() {
  _recording = true;

  // Make sure we only create the buffer once.
  _recordingBufferSizeIn10MS = _recordingFramesIn10MS * kRecordingNumChannels * 2;
  if (!_recordingBuffer) {
    _recordingBuffer = new int8_t[_recordingBufferSizeIn10MS];
  }

  _ptrThreadRec.reset(new rtc::PlatformThread(
      RecThreadFunc, this, "webrtc_audio_module_capture_thread",
      rtc::kRealtimePriority));

  _ptrThreadRec->Start();

  RTC_LOG(LS_INFO) << "Started recording from Python";

  return 0;
}

int32_t P2PRawAudioDevice::StopRecording() {
  {
    webrtc::MutexLock lock(&mutex_);
    _recording = false;
  }

  if (_ptrThreadRec) {
    _ptrThreadRec->Stop();
    _ptrThreadRec.reset();
  }

  webrtc::MutexLock lock(&mutex_);
  if (_recordingBuffer) {
    delete[] _recordingBuffer;
    _recordingBuffer = nullptr;
  }

  RTC_LOG(LS_INFO) << "Stopped recording from Python";
  return 0;
}

bool P2PRawAudioDevice::Recording() const {
  return _recording;
}

int32_t P2PRawAudioDevice::InitSpeaker() {
  return -1;
}

bool P2PRawAudioDevice::SpeakerIsInitialized() const {
  return false;
}

int32_t P2PRawAudioDevice::InitMicrophone() {
  return 0;
}

bool P2PRawAudioDevice::MicrophoneIsInitialized() const {
  return true;
}

int32_t P2PRawAudioDevice::SpeakerVolumeIsAvailable(bool &available) {
  return -1;
}

int32_t P2PRawAudioDevice::SetSpeakerVolume(uint32_t volume) {
  return -1;
}

int32_t P2PRawAudioDevice::SpeakerVolume(uint32_t &volume) const {
  return -1;
}

int32_t P2PRawAudioDevice::MaxSpeakerVolume(uint32_t &maxVolume) const {
  return -1;
}

int32_t P2PRawAudioDevice::MinSpeakerVolume(uint32_t &minVolume) const {
  return -1;
}

int32_t P2PRawAudioDevice::MicrophoneVolumeIsAvailable(bool &available) {
  return -1;
}

int32_t P2PRawAudioDevice::SetMicrophoneVolume(uint32_t volume) {
  return -1;
}

int32_t P2PRawAudioDevice::MicrophoneVolume(uint32_t &volume) const {
  return -1;
}

int32_t P2PRawAudioDevice::MaxMicrophoneVolume(uint32_t &maxVolume) const {
  return -1;
}

int32_t P2PRawAudioDevice::MinMicrophoneVolume(uint32_t &minVolume) const {
  return -1;
}

int32_t P2PRawAudioDevice::SpeakerMuteIsAvailable(bool &available) {
  return -1;
}

int32_t P2PRawAudioDevice::SetSpeakerMute(bool enable) {
  return -1;
}

int32_t P2PRawAudioDevice::SpeakerMute(bool &enabled) const {
  return -1;
}

int32_t P2PRawAudioDevice::MicrophoneMuteIsAvailable(bool &available) {
  return -1;
}

int32_t P2PRawAudioDevice::SetMicrophoneMute(bool enable) {
  return -1;
}

int32_t P2PRawAudioDevice::MicrophoneMute(bool &enabled) const {
  return -1;
}

int32_t P2PRawAudioDevice::StereoPlayoutIsAvailable(bool &available) {
  available = true;
  return 0;
}

int32_t P2PRawAudioDevice::SetStereoPlayout(bool enable) {
  return 0;
}

int32_t P2PRawAudioDevice::StereoPlayout(bool &enabled) const {
  enabled = true;
  return 0;
}

int32_t P2PRawAudioDevice::StereoRecordingIsAvailable(bool &available) {
  available = true;
  return 0;
}

int32_t P2PRawAudioDevice::SetStereoRecording(bool enable) {
  return 0;
}

int32_t P2PRawAudioDevice::StereoRecording(bool &enabled) const {
  enabled = true;
  return 0;
}

int32_t P2PRawAudioDevice::PlayoutDelay(uint16_t &delayMS) const {
  return 0;
}

void P2PRawAudioDevice::AttachAudioBuffer(webrtc::AudioDeviceBuffer *audioBuffer) {
  webrtc::MutexLock lock(&mutex_);

  _ptrAudioBuffer = audioBuffer;
  _ptrAudioBuffer->SetRecordingSampleRate(0);
  _ptrAudioBuffer->SetPlayoutSampleRate(0);
  _ptrAudioBuffer->SetRecordingChannels(0);
  _ptrAudioBuffer->SetPlayoutChannels(0);
}

void P2PRawAudioDevice::PlayThreadFunc(void *pThis) {
  auto *device = static_cast<P2PRawAudioDevice *>(pThis);
  while (device->PlayThreadProcess()) {
  }
}

void P2PRawAudioDevice::RecThreadFunc(void *pThis) {
  auto *device = static_cast<P2PRawAudioDevice *>(pThis);
  while (device->RecThreadProcess()) {
  }
}

bool P2PRawAudioDevice::PlayThreadProcess() {
  if (!_playing) {
    return false;
  }
  int64_t currentTime = rtc::TimeMillis();
  mutex_.Lock();

  if (_lastCallPlayoutMillis == 0 ||
      currentTime - _lastCallPlayoutMillis >= 10) {
    mutex_.Unlock();
    _ptrAudioBuffer->RequestPlayoutData(_playoutFramesIn10MS);
    mutex_.Lock();

    _playoutFramesLeft = _ptrAudioBuffer->GetPlayoutData(_playoutBuffer);
    RTC_DCHECK_EQ(_playoutFramesIn10MS, _playoutFramesLeft);
    if (!_rawAudioDeviceDescriptor->_isRecordingPaused()) {
      _rawAudioDeviceDescriptor->_setRecordedBuffer(_playoutBuffer, kPlayoutBufferSize);
    }
    _lastCallPlayoutMillis = currentTime;
  }
  _playoutFramesLeft = 0;
  mutex_.Unlock();

  int64_t deltaTimeMillis = rtc::TimeMillis() - currentTime;
  if (deltaTimeMillis < 10) {
    webrtc::SleepMs(10 - deltaTimeMillis);
  }

  return true;
}

bool P2PRawAudioDevice::RecThreadProcess() {
  if (!_recording) {
    return false;
  }

  int64_t currentTime = rtc::TimeMillis();
  mutex_.Lock();

  if (_lastCallRecordMillis == 0 || currentTime - _lastCallRecordMillis >= 10) {
    if (!_rawAudioDeviceDescriptor->_isPlayoutPaused()) {
      auto recordingStringBuffer = _rawAudioDeviceDescriptor->_getPlayoutBuffer(kRecordingBufferSize);
//      in prev impl was setting of _recordingBuffer
      _ptrAudioBuffer->SetRecordedBuffer((int8_t *) recordingStringBuffer->data(), _recordingFramesIn10MS);

      _lastCallRecordMillis = currentTime;
      mutex_.Unlock();
      _ptrAudioBuffer->DeliverRecordedData();
      mutex_.Lock();

      delete recordingStringBuffer;
    }
  }

  mutex_.Unlock();

  int64_t deltaTimeMillis = rtc::TimeMillis() - currentTime;
  if (deltaTimeMillis < 10) {
    webrtc::SleepMs(10 - deltaTimeMillis);
  }

  return true;
}
