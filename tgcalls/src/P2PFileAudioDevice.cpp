#include "P2PFileAudioDevice.h"

#include <cstring>

#include <modules/audio_device/audio_device_impl.h>
#include <rtc_base/ref_counted_object.h>
#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/time_utils.h>
#include <system_wrappers/include/sleep.h>

const int kRecordingFixedSampleRate = 48000;
const size_t kRecordingNumChannels = 2;
const int kPlayoutFixedSampleRate = 48000;
const size_t kPlayoutNumChannels = 2;
const size_t kPlayoutBufferSize =
    kPlayoutFixedSampleRate / 100 * kPlayoutNumChannels * 2;
const size_t kRecordingBufferSize =
    kRecordingFixedSampleRate / 100 * kRecordingNumChannels * 2;

P2PFileAudioDevice::P2PFileAudioDevice(std::shared_ptr<P2PFileAudioDeviceDescriptor> fileAudioDeviceDescriptor)
    : _ptrAudioBuffer(nullptr),
      _recordingBuffer(nullptr),
      _playoutBuffer(nullptr),
      _recordingFramesLeft(0),
      _playoutFramesLeft(0),
      _recordingBufferSizeIn10MS(0),
      _recordingFramesIn10MS(0),
      _playoutFramesIn10MS(0),
      _playing(false),
      _recording(false),
      _lastCallPlayoutMillis(0),
      _lastCallRecordMillis(0),
      _fileAudioDeviceDescriptor(std::move(fileAudioDeviceDescriptor)) {}

P2PFileAudioDevice::~P2PFileAudioDevice() = default;

int32_t P2PFileAudioDevice::ActiveAudioLayer(
    webrtc::AudioDeviceModule::AudioLayer &audioLayer) const {
  return -1;
}

webrtc::AudioDeviceGeneric::InitStatus P2PFileAudioDevice::Init() {
  return InitStatus::OK;
}

int32_t P2PFileAudioDevice::Terminate() {
  return 0;
}

bool P2PFileAudioDevice::Initialized() const {
  return true;
}

int16_t P2PFileAudioDevice::PlayoutDevices() {
  return 1;
}

int16_t P2PFileAudioDevice::RecordingDevices() {
  return 1;
}

int32_t P2PFileAudioDevice::PlayoutDeviceName(uint16_t index,
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

int32_t P2PFileAudioDevice::RecordingDeviceName(uint16_t index,
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

int32_t P2PFileAudioDevice::SetPlayoutDevice(uint16_t index) {
  if (index == 0) {
    _playout_index = index;
    return 0;
  }
  return -1;
}

int32_t P2PFileAudioDevice::SetPlayoutDevice(
    webrtc::AudioDeviceModule::WindowsDeviceType device) {
  return -1;
}

int32_t P2PFileAudioDevice::SetRecordingDevice(uint16_t index) {
  if (index == 0) {
    _record_index = index;
    return _record_index;
  }
  return -1;
}

int32_t P2PFileAudioDevice::SetRecordingDevice(
    webrtc::AudioDeviceModule::WindowsDeviceType device) {
  return -1;
}

int32_t P2PFileAudioDevice::PlayoutIsAvailable(bool &available) {
  if (_playout_index == 0) {
    available = true;
    return _playout_index;
  }
  available = false;
  return -1;
}

int32_t P2PFileAudioDevice::InitPlayout() {
  webrtc::MutexLock lock(&mutex_);

  if (_playing) {
    return -1;
  }

  _playoutFramesIn10MS = static_cast<size_t>(kPlayoutFixedSampleRate / 100);

  if (_ptrAudioBuffer) {
    // Update webrtc audio buffer with the selected parameters
    _ptrAudioBuffer->SetPlayoutSampleRate(kPlayoutFixedSampleRate);
    _ptrAudioBuffer->SetPlayoutChannels(kPlayoutNumChannels);
  }
  return 0;
}

bool P2PFileAudioDevice::PlayoutIsInitialized() const {
  return _playoutFramesIn10MS != 0;
}

int32_t P2PFileAudioDevice::RecordingIsAvailable(bool &available) {
  if (_record_index == 0) {
    available = true;
    return _record_index;
  }
  available = false;
  return -1;
}

int32_t P2PFileAudioDevice::InitRecording() {
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

bool P2PFileAudioDevice::RecordingIsInitialized() const {
  return _recordingFramesIn10MS != 0;
}

int32_t P2PFileAudioDevice::StartPlayout() {
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

  // PLAYOUT
  auto _outputFilename = _fileAudioDeviceDescriptor->_getOutputFilename();
  if (!_outputFilename.empty()) {
    _outputFile = webrtc::FileWrapper::OpenWriteOnly(_outputFilename.c_str());
    if (!_outputFile.is_open()) {
      RTC_LOG(LS_ERROR) << "Failed to open playout file: " << _outputFilename;
      _playing = false;
      delete[] _playoutBuffer;
      _playoutBuffer = nullptr;
      return -1;
    }
  }

  _ptrThreadPlay.reset(new rtc::PlatformThread(
      PlayThreadFunc, this, "webrtc_audio_module_play_thread",
      rtc::kRealtimePriority));
  _ptrThreadPlay->Start();

  RTC_LOG(LS_INFO) << "Started playout capture to output file: "
                   << _outputFilename;
  return 0;
}

int32_t P2PFileAudioDevice::StopPlayout() {
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
  _outputFile.Close();

  RTC_LOG(LS_INFO) << "Stopped playout capture to output file";
  return 0;
}

bool P2PFileAudioDevice::Playing() const {
  return _playing;
}

int32_t P2PFileAudioDevice::StartRecording() {
  _recording = true;

  // Make sure we only create the buffer once.
  _recordingBufferSizeIn10MS =
      _recordingFramesIn10MS * kRecordingNumChannels * 2;
  if (!_recordingBuffer) {
    _recordingBuffer = new int8_t[_recordingBufferSizeIn10MS];
  }

  auto _inputFilename = _fileAudioDeviceDescriptor->_getInputFilename();
  if (!_inputFilename.empty()) {
    _inputFile = webrtc::FileWrapper::OpenReadOnly(_inputFilename.c_str());
    if (!_inputFile.is_open()) {
      RTC_LOG(LS_ERROR) << "Failed to open audio input file: "
                        << _inputFilename;
      _recording = false;
      delete[] _recordingBuffer;
      _recordingBuffer = nullptr;
      return -1;
    }
  }

  _ptrThreadRec.reset(new rtc::PlatformThread(
      RecThreadFunc, this, "webrtc_audio_module_capture_thread",
      rtc::kRealtimePriority));

  _ptrThreadRec->Start();

  RTC_LOG(LS_INFO) << "Started recording from input file: " << _inputFilename;

  return 0;
}

int32_t P2PFileAudioDevice::StopRecording() {
  {
    webrtc::MutexLock lock(&mutex_);
    _recording = false;
  }

  if (_ptrThreadRec) {
    _ptrThreadRec->Stop();
    _ptrThreadRec.reset();
  }

  webrtc::MutexLock lock(&mutex_);
  _recordingFramesLeft = 0;
  if (_recordingBuffer) {
    delete[] _recordingBuffer;
    _recordingBuffer = nullptr;
  }
  _inputFile.Close();

  RTC_LOG(LS_INFO) << "Stopped recording from input file";
  return 0;
}

bool P2PFileAudioDevice::Recording() const {
  return _recording;
}

int32_t P2PFileAudioDevice::InitSpeaker() {
  return -1;
}

bool P2PFileAudioDevice::SpeakerIsInitialized() const {
  return false;
}

int32_t P2PFileAudioDevice::InitMicrophone() {
  return 0;
}

bool P2PFileAudioDevice::MicrophoneIsInitialized() const {
  return true;
}

int32_t P2PFileAudioDevice::SpeakerVolumeIsAvailable(bool &available) {
  return -1;
}

int32_t P2PFileAudioDevice::SetSpeakerVolume(uint32_t volume) {
  return -1;
}

int32_t P2PFileAudioDevice::SpeakerVolume(uint32_t &volume) const {
  return -1;
}

int32_t P2PFileAudioDevice::MaxSpeakerVolume(uint32_t &maxVolume) const {
  return -1;
}

int32_t P2PFileAudioDevice::MinSpeakerVolume(uint32_t &minVolume) const {
  return -1;
}

int32_t P2PFileAudioDevice::MicrophoneVolumeIsAvailable(bool &available) {
  return -1;
}

int32_t P2PFileAudioDevice::SetMicrophoneVolume(uint32_t volume) {
  return -1;
}

int32_t P2PFileAudioDevice::MicrophoneVolume(uint32_t &volume) const {
  return -1;
}

int32_t P2PFileAudioDevice::MaxMicrophoneVolume(uint32_t &maxVolume) const {
  return -1;
}

int32_t P2PFileAudioDevice::MinMicrophoneVolume(uint32_t &minVolume) const {
  return -1;
}

int32_t P2PFileAudioDevice::SpeakerMuteIsAvailable(bool &available) {
  return -1;
}

int32_t P2PFileAudioDevice::SetSpeakerMute(bool enable) {
  return -1;
}

int32_t P2PFileAudioDevice::SpeakerMute(bool &enabled) const {
  return -1;
}

int32_t P2PFileAudioDevice::MicrophoneMuteIsAvailable(bool &available) {
  return -1;
}

int32_t P2PFileAudioDevice::SetMicrophoneMute(bool enable) {
  return -1;
}

int32_t P2PFileAudioDevice::MicrophoneMute(bool &enabled) const {
  return -1;
}

int32_t P2PFileAudioDevice::StereoPlayoutIsAvailable(bool &available) {
  available = true;
  return 0;
}

int32_t P2PFileAudioDevice::SetStereoPlayout(bool enable) {
  return 0;
}

int32_t P2PFileAudioDevice::StereoPlayout(bool &enabled) const {
  enabled = true;
  return 0;
}

int32_t P2PFileAudioDevice::StereoRecordingIsAvailable(bool &available) {
  available = true;
  return 0;
}

int32_t P2PFileAudioDevice::SetStereoRecording(bool enable) {
  return 0;
}

int32_t P2PFileAudioDevice::StereoRecording(bool &enabled) const {
  enabled = true;
  return 0;
}

int32_t P2PFileAudioDevice::PlayoutDelay(uint16_t &delayMS) const {
  return 0;
}

void P2PFileAudioDevice::AttachAudioBuffer(webrtc::AudioDeviceBuffer *audioBuffer) {
  webrtc::MutexLock lock(&mutex_);

  _ptrAudioBuffer = audioBuffer;

  // Inform the AudioBuffer about default settings for this implementation.
  // Set all values to zero here since the actual settings will be done by
  // InitPlayout and InitRecording later.
  _ptrAudioBuffer->SetRecordingSampleRate(0);
  _ptrAudioBuffer->SetPlayoutSampleRate(0);
  _ptrAudioBuffer->SetRecordingChannels(0);
  _ptrAudioBuffer->SetPlayoutChannels(0);
}

void P2PFileAudioDevice::PlayThreadFunc(void *pThis) {
  auto *device = static_cast<P2PFileAudioDevice *>(pThis);
  while (device->PlayThreadProcess()) {
  }
}

void P2PFileAudioDevice::RecThreadFunc(void *pThis) {
  auto *device = static_cast<P2PFileAudioDevice *>(pThis);
  while (device->RecThreadProcess()) {
  }
}

bool P2PFileAudioDevice::PlayThreadProcess() {
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
    if (_outputFile.is_open() && !_fileAudioDeviceDescriptor->_isRecordingPaused()) {
      _outputFile.Write(_playoutBuffer, kPlayoutBufferSize);
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

bool P2PFileAudioDevice::RecThreadProcess() {
  if (!_recording) {
    return false;
  }

  int64_t currentTime = rtc::TimeMillis();
  mutex_.Lock();

  if (_lastCallRecordMillis == 0 || currentTime - _lastCallRecordMillis >= 10) {
    if (_inputFile.is_open() && !_fileAudioDeviceDescriptor->_isPlayoutPaused()) {
      auto _inputFilename = _fileAudioDeviceDescriptor->_getInputFilename();
      if (_inputFile.Read(_recordingBuffer, kRecordingBufferSize) > 0) {
        _ptrAudioBuffer->SetRecordedBuffer(_recordingBuffer,
                                           _recordingFramesIn10MS);
      } else if (_fileAudioDeviceDescriptor->_isEndlessPlayout()) {
        _inputFile.Rewind();

        if (_fileAudioDeviceDescriptor->_playoutEndedCallback) {
          _fileAudioDeviceDescriptor->_playoutEndedCallback(_inputFilename);
        }
      } else {
        mutex_.Unlock();

        if (_fileAudioDeviceDescriptor->_playoutEndedCallback) {
          _fileAudioDeviceDescriptor->_playoutEndedCallback(_inputFilename);
        }

        return false;
      }
      _lastCallRecordMillis = currentTime;
      mutex_.Unlock();
      _ptrAudioBuffer->DeliverRecordedData();
      mutex_.Lock();
    }
  }

  mutex_.Unlock();

  int64_t deltaTimeMillis = rtc::TimeMillis() - currentTime;
  if (deltaTimeMillis < 10) {
    webrtc::SleepMs(10 - deltaTimeMillis);
  }

  return true;
}
