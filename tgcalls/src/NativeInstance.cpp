#include <iostream>
#include <utility>

#include <rtc_base/ssl_adapter.h>

#include "NativeInstance.h"
#include "tgcalls/InstanceImpl.h"

namespace py = pybind11;

auto noticeDisplayed = false;

NativeInstance::NativeInstance(bool logToStdErr, string logPath)
    : _logToStdErr(logToStdErr), _logPath(std::move(logPath)) {
  if (!noticeDisplayed) {
    auto ver = std::string(PROJECT_VER);
    auto dev = std::count(ver.begin(), ver.end(), '.') == 3 ? " DEV" : "";
    py::print("tgcalls v" + ver + dev + ", Copyright (C) 2020-2023 Ilya (Marshal) <https://github.com/MarshalX>");
    py::print("Licensed under the terms of the GNU Lesser General Public License v3 (LGPLv3) \n\n");

    noticeDisplayed = true;
  }
  rtc::InitializeSSL();
  py::print("NativeInstance-1");
  tgcalls::Register<tgcalls::InstanceImpl>();
  py::print("NativeInstance-2");
}

NativeInstance::~NativeInstance() = default;

void NativeInstance::setupGroupCall(
    std::function<void(tgcalls::GroupJoinPayload)> &emitJoinPayloadCallback,
    std::function<void(bool)> &networkStateUpdated,
    int outgoingAudioBitrateKbit) {
  _emitJoinPayloadCallback = emitJoinPayloadCallback;
  _networkStateUpdated = networkStateUpdated;
  _outgoingAudioBitrateKbit = outgoingAudioBitrateKbit;
}

class PytgcallsRequestMediaChannelDescriptionTask final : public tgcalls::RequestMediaChannelDescriptionTask {
public:
  PytgcallsRequestMediaChannelDescriptionTask();

  void cancel() {};
};

PytgcallsRequestMediaChannelDescriptionTask::PytgcallsRequestMediaChannelDescriptionTask() {
  std::cout << "PytgcallsRequestMediaChannelDescriptionTask";
}

void NativeInstance::createInstanceHolder(
    std::function<rtc::scoped_refptr<webrtc::AudioDeviceModule>(webrtc::TaskQueueFactory *)> createAudioDeviceModule,
    std::string initialInputDeviceId = "",
    std::string initialOutputDeviceId = ""
) {
  tgcalls::GroupInstanceDescriptor descriptor{
      .threads = tgcalls::StaticThreads::getThreads(),
      .config = tgcalls::GroupConfig{.need_log = true,
          .logPath = {std::move(_logPath)},
          .logToStdErr = _logToStdErr},
      .networkStateUpdated =
      [=](tgcalls::GroupNetworkState groupNetworkState) {
        _networkStateUpdated(groupNetworkState.isConnected);
      },
      .audioLevelsUpdated = [=](tgcalls::GroupLevelsUpdate const &update) {}, // its necessary for audio analyzing (VAD)
      .initialInputDeviceId = std::move(initialInputDeviceId),
      .initialOutputDeviceId = std::move(initialOutputDeviceId),
      .createAudioDeviceModule = std::move(createAudioDeviceModule),
      .outgoingAudioBitrateKbit = _outgoingAudioBitrateKbit,
      .disableOutgoingAudioProcessing = true,
      .videoContentType = tgcalls::VideoContentType::Generic,
      .requestMediaChannelDescriptions = [=](
          std::vector<uint32_t> const & ssrcs,
          std::function<void(std::vector<tgcalls::MediaChannelDescription> &&)> done) {
        auto result = std::make_shared<PytgcallsRequestMediaChannelDescriptionTask>();
        return result;
      },
      // deprecated
//      .participantDescriptionsRequired =
//      [=](std::vector<uint32_t> const &ssrcs) {
//        _participantDescriptionsRequired(ssrcs);
//      },
      //        .requestBroadcastPart = [=](int64_t time, int64_t period,
      //        std::function<void(tgcalls::BroadcastPart &&)> done) {},
  };

  instanceHolder = std::make_unique<InstanceHolder>();
  instanceHolder->groupNativeInstance = std::make_unique<tgcalls::GroupInstanceCustomImpl>(std::move(descriptor));
  instanceHolder->groupNativeInstance->emitJoinPayload(
      [=](tgcalls::GroupJoinPayload payload) {
        _emitJoinPayloadCallback(std::move(payload));
      }
  );
}

void NativeInstance::startGroupCall(std::shared_ptr<FileAudioDeviceDescriptor> fileAudioDeviceDescriptor) {
  _fileAudioDeviceDescriptor = std::move(fileAudioDeviceDescriptor);
  createInstanceHolder(
      [&](webrtc::TaskQueueFactory *taskQueueFactory) -> rtc::scoped_refptr<webrtc::AudioDeviceModule> {
        return WrappedAudioDeviceModuleImpl::Create(
            webrtc::AudioDeviceModule::kDummyAudio, taskQueueFactory, std::move(_fileAudioDeviceDescriptor)
        );
      });
}

void NativeInstance::startGroupCall(std::shared_ptr<RawAudioDeviceDescriptor> rawAudioDeviceDescriptor) {
  _rawAudioDeviceDescriptor = std::move(rawAudioDeviceDescriptor);
  createInstanceHolder(
      [&](webrtc::TaskQueueFactory *taskQueueFactory) -> rtc::scoped_refptr<webrtc::AudioDeviceModule> {
        return WrappedAudioDeviceModuleImpl::Create(
            webrtc::AudioDeviceModule::kDummyAudio, taskQueueFactory, std::move(_rawAudioDeviceDescriptor)
        );
      });
}

void NativeInstance::startGroupCall(std::string initialInputDeviceId = "", std::string initialOutputDeviceId = "") {
  createInstanceHolder(
      [&](webrtc::TaskQueueFactory *taskQueueFactory) -> rtc::scoped_refptr<webrtc::AudioDeviceModule> {
        return webrtc::AudioDeviceModule::Create(
            webrtc::AudioDeviceModule::kPlatformDefaultAudio, taskQueueFactory
        );
      }, std::move(initialInputDeviceId), std::move(initialOutputDeviceId));
}

void NativeInstance::stopGroupCall() const {
  instanceHolder->groupNativeInstance = nullptr;
}

bool NativeInstance::isGroupCallNativeCreated() const {
  return instanceHolder != nullptr && instanceHolder->groupNativeInstance != nullptr;
}

void NativeInstance::emitJoinPayload(std::function<void(tgcalls::GroupJoinPayload)> const &f) const {
  instanceHolder->groupNativeInstance->emitJoinPayload(f);
}

void NativeInstance::setJoinResponsePayload(std::string const &payload) const {
  instanceHolder->groupNativeInstance->setJoinResponsePayload(payload);
}

void NativeInstance::setIsMuted(bool isMuted) const {
  instanceHolder->groupNativeInstance->setIsMuted(isMuted);
}

void NativeInstance::setVolume(uint32_t ssrc, double volume) const {
  instanceHolder->groupNativeInstance->setVolume(ssrc, volume);
}

void NativeInstance::setConnectionMode(
    tgcalls::GroupConnectionMode connectionMode,
    bool keepBroadcastIfWasEnabled) const {
  instanceHolder->groupNativeInstance->setConnectionMode(
      connectionMode, keepBroadcastIfWasEnabled);
}

void NativeInstance::stopAudioDeviceModule() const {
  instanceHolder->groupNativeInstance->performWithAudioDeviceModule(
      [&](const rtc::scoped_refptr<tgcalls::WrappedAudioDeviceModule>& audioDeviceModule) {
        if (!audioDeviceModule) {
          return;
        }

        audioDeviceModule->StopRecording();
        audioDeviceModule->StopPlayout();
//        audioDeviceModule->Stop();
      }
  );
}

void NativeInstance::startAudioDeviceModule() const {
  instanceHolder->groupNativeInstance->performWithAudioDeviceModule(
      [&](const rtc::scoped_refptr<tgcalls::WrappedAudioDeviceModule>& audioDeviceModule) {
        if (!audioDeviceModule) {
          return;
        }

        if (!audioDeviceModule->Recording()) {
          audioDeviceModule->StartRecording();
        }
        if (!audioDeviceModule->Playing()){
          audioDeviceModule->StartPlayout();
        }
      }
  );
}

void NativeInstance::restartAudioInputDevice() const {
  instanceHolder->groupNativeInstance->performWithAudioDeviceModule(
      [&](const rtc::scoped_refptr<tgcalls::WrappedAudioDeviceModule>& audioDeviceModule) {
        if (!audioDeviceModule) {
          return;
        }

        const auto recording = audioDeviceModule->Recording();
        if (recording) {
          audioDeviceModule->StopRecording();
        }
        if (recording && audioDeviceModule->InitRecording() == 0) {
          audioDeviceModule->StartRecording();
        }
      }
  );
}

void NativeInstance::restartAudioOutputDevice() const {
  instanceHolder->groupNativeInstance->performWithAudioDeviceModule(
      [&](const rtc::scoped_refptr<tgcalls::WrappedAudioDeviceModule>& audioDeviceModule) {
        if (!audioDeviceModule) {
          return;
        }

        if (audioDeviceModule->Playing()) {
          audioDeviceModule->StopPlayout();
        }
        if (audioDeviceModule->InitPlayout() == 0) {
          audioDeviceModule->StartPlayout();
        }
      }
  );
}

std::vector<tgcalls::GroupInstanceInterface::AudioDevice> NativeInstance::getPlayoutDevices() const {
  return instanceHolder->groupNativeInstance->getAudioDevices(
      tgcalls::GroupInstanceInterface::AudioDevice::Type::Output
  );
}


std::vector<tgcalls::GroupInstanceInterface::AudioDevice> NativeInstance::getRecordingDevices() const {
  return instanceHolder->groupNativeInstance->getAudioDevices(
      tgcalls::GroupInstanceInterface::AudioDevice::Type::Input
  );
}


void NativeInstance::setAudioOutputDevice(std::string id) const {
  instanceHolder->groupNativeInstance->setAudioOutputDevice(std::move(id));
}

void NativeInstance::setAudioInputDevice(std::string id) const {
  instanceHolder->groupNativeInstance->setAudioInputDevice(std::move(id));
}

void NativeInstance::setVideoCapture(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height) {
  _videoCapture = tgcalls::VideoCaptureInterface::Create(
      tgcalls::StaticThreads::getThreads(),
      PythonVideoTrackSource::createPtr(
          std::make_unique<PythonSource>(std::move(getNextFrameBuffer), fps, width, height),fps),
      "python_video_track_source"
  );

  instanceHolder->groupNativeInstance->setVideoCapture(std::move(_videoCapture));
}

void NativeInstance::startCall(vector<RtcServer> servers,
                               std::array<uint8_t, 256> authKey,
                               bool isOutgoing, string logPath) {
  auto encryptionKeyValue = std::make_shared<std::array<uint8_t, 256>>();
  std::memcpy(encryptionKeyValue->data(), &authKey, 256);

  std::shared_ptr<tgcalls::VideoCaptureInterface> videoCapture = nullptr;

  tgcalls::MediaDevicesConfig mediaConfig = {
      // .audioInputId = "VB-Cable",
      .audioInputId = "Unix FIFO source /home/callmic.pipe",
      .audioOutputId = "callout",
      //            .audioInputId = "0",
      //            .audioOutputId = "0",
      .inputVolume = 1.f,
      .outputVolume = 1.f};
  py::print("NativeInstance-CQ-1");
  tgcalls::Descriptor descriptor = {
      .config =
      tgcalls::Config{
          .initializationTimeout = 1000,
          .receiveTimeout = 1000,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = false,
          .enableStunMarking = true,
          .enableAEC = true,
          .enableNS = true,
          .enableAGC = true,
          .enableVolumeControl = true,
          .logPath = {std::move(logPath)},
          // .statsLogPath = {"/Users/marshal/projects/tgcalls/python-binding/"
          //                  "pytgcalls/tgcalls-stat.txt"},
          .maxApiLayer = 92,
          .enableHighBitrateVideo = false,
          .preferredVideoCodecs = std::vector<std::string>(),
          .protocolVersion = tgcalls::ProtocolVersion::V0,
          //                .preferredVideoCodecs = {cricket::kVp9CodecName}
      },
      .persistentState = {std::vector<uint8_t>()},
      .initialNetworkType = tgcalls::NetworkType::WiFi,
      .encryptionKey = tgcalls::EncryptionKey(encryptionKeyValue, isOutgoing),
      .mediaDevicesConfig = mediaConfig,
      .videoCapture = videoCapture,
      .stateUpdated =
      [=](tgcalls::State state) {
        // printf("stateUpdated %d\n", (int)state);
        if (stateUpdatedCallback != nullptr) {
          stateUpdatedCallback((int)state);
        }
      },
      .signalBarsUpdated =
      [=](int count) {
        // printf("signalBarsUpdated %d\n", count);
      },
      .audioLevelUpdated =
      [=](float level) {
        // printf("audioLevelUpdated\n");
      },
      .remoteBatteryLevelIsLowUpdated =
      [=](bool isLow) {
        // printf("remoteBatteryLevelIsLowUpdated\n");
      },
      .remoteMediaStateUpdated =
      [=](tgcalls::AudioState audioState, tgcalls::VideoState videoState) {
        // printf("remoteMediaStateUpdated\n");
      },
      .remotePrefferedAspectRatioUpdated =
      [=](float ratio) {
        // printf("remotePrefferedAspectRatioUpdated\n");
      },
      .signalingDataEmitted =
      [=](const std::vector<uint8_t> &data) {
        // printf("signalingDataEmitted\n");
        if (signalingDataEmittedCallback != nullptr) {
          signalingDataEmittedCallback(data);
        }
      },
  };
  py::print("NativeInstance-CQ-2");
  for (int i = 0, size = servers.size(); i < size; ++i) {
    RtcServer rtcServer = std::move(servers.at(i));

    const auto host = rtcServer.ip;
    const auto hostv6 = rtcServer.ipv6;
    const auto port = uint16_t(rtcServer.port);

    if (rtcServer.isStun) {
      const auto pushStun = [&](const string &host) {
        descriptor.rtcServers.push_back(
            tgcalls::RtcServer{.host = host, .port = port, .isTurn = false});
      };
      pushStun(host);
      pushStun(hostv6);
    }

    const auto username = rtcServer.login;
    const auto password = rtcServer.password;
    if (rtcServer.isTurn) {
      const auto pushTurn = [&](const string &host) {
        descriptor.rtcServers.push_back(tgcalls::RtcServer{
            .host = host,
            .port = port,
            .login = username,
            .password = password,
            .isTurn = true,
        });
      };
      pushTurn(host);
      pushTurn(hostv6);
    }
  }

  instanceHolder = std::make_unique<InstanceHolder>();
  instanceHolder->nativeInstance =
      tgcalls::Meta::Create("3.0.0", std::move(descriptor));
  py::print("NativeInstance-CQ-4");
  instanceHolder->_videoCapture = videoCapture;
  instanceHolder->nativeInstance->setNetworkType(tgcalls::NetworkType::WiFi);
  instanceHolder->nativeInstance->setRequestedVideoAspect(1);
  instanceHolder->nativeInstance->setMuteMicrophone(false);
}

void NativeInstance::startCallVoice(vector<RtcServer> servers, std::array<uint8_t, 256> authKey, bool isOutgoing, 
  std::string tag, std::string audioInputId, std::string audioOutputId) {
  auto encryptionKeyValue = std::make_shared<std::array<uint8_t, 256>>();
  std::memcpy(encryptionKeyValue->data(), &authKey, 256);

  std::shared_ptr<tgcalls::VideoCaptureInterface> videoCapture = nullptr;

  tgcalls::MediaDevicesConfig mediaConfig = {
      .audioInputId = audioInputId,
      .audioOutputId = audioOutputId,
      .inputVolume = 1.f,
      .outputVolume = 1.f};
  py::print("NativeInstance-CQ-1");
  tgcalls::Descriptor descriptor = {
      .config =
      tgcalls::Config{
          .initializationTimeout = 1000,
          .receiveTimeout = 1000,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = false,
          .enableStunMarking = true,
          .enableAEC = true,
          .enableNS = true,
          .enableAGC = true,
          .enableVolumeControl = true,
          .logPath = {"/home/tgcalls-log.txt"},
          .maxApiLayer = 92,
          .enableHighBitrateVideo = false,
          .preferredVideoCodecs = std::vector<std::string>(),
          .protocolVersion = tgcalls::ProtocolVersion::V0,
          //                .preferredVideoCodecs = {cricket::kVp9CodecName}
      },
      .persistentState = {std::vector<uint8_t>()},
      .initialNetworkType = tgcalls::NetworkType::WiFi,
      .encryptionKey = tgcalls::EncryptionKey(encryptionKeyValue, isOutgoing),
      .mediaDevicesConfig = mediaConfig,
      .videoCapture = videoCapture,
      .stateUpdated =
      [=](tgcalls::State state) {
        // printf("stateUpdated %d\n", (int)state);
        if (stateUpdatedCallback != nullptr) {
          stateUpdatedCallback((int)state);
        }
      },
      .signalBarsUpdated =
      [=](int count) {
        // printf("signalBarsUpdated %d\n", count);
      },
      .audioLevelUpdated =
      [=](float level) {
        // printf("audioLevelUpdated\n");
      },
      .remoteBatteryLevelIsLowUpdated =
      [=](bool isLow) {
        // printf("remoteBatteryLevelIsLowUpdated\n");
      },
      .remoteMediaStateUpdated =
      [=](tgcalls::AudioState audioState, tgcalls::VideoState videoState) {
        // printf("remoteMediaStateUpdated\n");
      },
      .remotePrefferedAspectRatioUpdated =
      [=](float ratio) {
        // printf("remotePrefferedAspectRatioUpdated\n");
      },
      .signalingDataEmitted =
      [=](const std::vector<uint8_t> &data) {
        // printf("signalingDataEmitted\n");
        if (signalingDataEmittedCallback != nullptr) {
          signalingDataEmittedCallback(data);
        }
      },
  };
  py::print("NativeInstance-CQ-2");
  for (int i = 0, size = servers.size(); i < size; ++i) {
    RtcServer rtcServer = std::move(servers.at(i));

    const auto host = rtcServer.ip;
    const auto hostv6 = rtcServer.ipv6;
    const auto port = uint16_t(rtcServer.port);

    if (rtcServer.isStun) {
      const auto pushStun = [&](const string &host) {
        descriptor.rtcServers.push_back(
            tgcalls::RtcServer{.host = host, .port = port, .isTurn = false});
      };
      pushStun(host);
      pushStun(hostv6);
    }

    const auto username = rtcServer.login;
    const auto password = rtcServer.password;
    if (rtcServer.isTurn) {
      const auto pushTurn = [&](const string &host) {
        descriptor.rtcServers.push_back(tgcalls::RtcServer{
            .host = host,
            .port = port,
            .login = username,
            .password = password,
            .isTurn = true,
        });
      };
      pushTurn(host);
      pushTurn(hostv6);
    }
  }

  instanceHolder = std::make_unique<InstanceHolder>();
  instanceHolder->nativeInstance =
      tgcalls::Meta::Create("3.0.0", std::move(descriptor));
  py::print("NativeInstance-CQ-4");
  instanceHolder->_videoCapture = videoCapture;
  instanceHolder->nativeInstance->setNetworkType(tgcalls::NetworkType::WiFi);
  instanceHolder->nativeInstance->setRequestedVideoAspect(1);
  instanceHolder->nativeInstance->setMuteMicrophone(false);
}

void NativeInstance::stop() {
  if (instanceHolder != nullptr && instanceHolder->nativeInstance != nullptr) {
    instanceHolder->nativeInstance->stop([&](tgcalls::FinalState state) {
      printf("stop state %s\n", state.debugLog.c_str());
    });
  }
}

void NativeInstance::receiveSignalingData(std::vector<uint8_t> &data) const {
  instanceHolder->nativeInstance->receiveSignalingData(data);
}

void NativeInstance::setSignalingDataEmittedCallback(
    const std::function<void(const std::vector<uint8_t> &data)> &f) {
  signalingDataEmittedCallback = f;
}

void NativeInstance::setStateUpdatedCallback(
    const std::function<void(int)> &f) {
  stateUpdatedCallback = f;
}

void NativeInstance::startCallP2P(vector<RtcServer> servers, std::array<uint8_t, 256> authKey, bool isOutgoing, 
                                        std::shared_ptr<P2PFileAudioDeviceDescriptor> fileAudioDeviceDescriptor) {
  _P2PFileAudioDeviceDescriptor = std::move(fileAudioDeviceDescriptor);
  auto encryptionKeyValue = std::make_shared<std::array<uint8_t, 256>>();
  std::memcpy(encryptionKeyValue->data(), &authKey, 256);

  std::shared_ptr<tgcalls::VideoCaptureInterface> videoCapture = nullptr;

  tgcalls::MediaDevicesConfig mediaConfig = {
      .audioInputId = "",
      .audioOutputId = "",
      .inputVolume = 1.f,
      .outputVolume = 1.f};
  py::print("NativeInstance-CQ-1");
  tgcalls::Descriptor descriptor = {
      .config =
      tgcalls::Config{
          .initializationTimeout = 1000,
          .receiveTimeout = 1000,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = false,
          .enableStunMarking = true,
          .enableAEC = true,
          .enableNS = true,
          .enableAGC = true,
          .enableVolumeControl = true,
          .logPath = {"/home/tgcalls-log.txt"},
          .maxApiLayer = 92,
          .enableHighBitrateVideo = false,
          .preferredVideoCodecs = std::vector<std::string>(),
          .protocolVersion = tgcalls::ProtocolVersion::V0,
          //                .preferredVideoCodecs = {cricket::kVp9CodecName}
      },
      .persistentState = {std::vector<uint8_t>()},
      .initialNetworkType = tgcalls::NetworkType::WiFi,
      .encryptionKey = tgcalls::EncryptionKey(encryptionKeyValue, isOutgoing),
      .mediaDevicesConfig = mediaConfig,
      .videoCapture = videoCapture,
      .stateUpdated =
      [=](tgcalls::State state) {
        // printf("stateUpdated %d\n", (int)state);
        if (stateUpdatedCallback != nullptr) {
          stateUpdatedCallback((int)state);
        }
      },
      .signalBarsUpdated =
      [=](int count) {
        // printf("signalBarsUpdated %d\n", count);
      },
      .audioLevelUpdated =
      [=](float level) {
        // printf("audioLevelUpdated\n");
      },
      .remoteBatteryLevelIsLowUpdated =
      [=](bool isLow) {
        // printf("remoteBatteryLevelIsLowUpdated\n");
      },
      .remoteMediaStateUpdated =
      [=](tgcalls::AudioState audioState, tgcalls::VideoState videoState) {
        // printf("remoteMediaStateUpdated\n");
      },
      .remotePrefferedAspectRatioUpdated =
      [=](float ratio) {
        // printf("remotePrefferedAspectRatioUpdated\n");
      },
      .signalingDataEmitted =
      [=](const std::vector<uint8_t> &data) {
        // printf("signalingDataEmitted\n");
        if (signalingDataEmittedCallback != nullptr) {
          signalingDataEmittedCallback(data);
        }
      },
      .createAudioDeviceModule =  [&](webrtc::TaskQueueFactory *taskQueueFactory) -> rtc::scoped_refptr<webrtc::AudioDeviceModule> {
        return WrappedAudioDeviceModuleImpl::Create(
            webrtc::AudioDeviceModule::kDummyAudio, taskQueueFactory, std::move(_P2PFileAudioDeviceDescriptor)
        );
      },
  };
  py::print("NativeInstance-CQ-2");
  for (int i = 0, size = servers.size(); i < size; ++i) {
    RtcServer rtcServer = std::move(servers.at(i));

    const auto host = rtcServer.ip;
    const auto hostv6 = rtcServer.ipv6;
    const auto port = uint16_t(rtcServer.port);

    if (rtcServer.isStun) {
      const auto pushStun = [&](const string &host) {
        descriptor.rtcServers.push_back(
            tgcalls::RtcServer{.host = host, .port = port, .isTurn = false});
      };
      pushStun(host);
      pushStun(hostv6);
    }

    const auto username = rtcServer.login;
    const auto password = rtcServer.password;
    if (rtcServer.isTurn) {
      const auto pushTurn = [&](const string &host) {
        descriptor.rtcServers.push_back(tgcalls::RtcServer{
            .host = host,
            .port = port,
            .login = username,
            .password = password,
            .isTurn = true,
        });
      };
      pushTurn(host);
      pushTurn(hostv6);
    }
  }

  instanceHolder = std::make_unique<InstanceHolder>();
  instanceHolder->nativeInstance =
      tgcalls::Meta::Create("3.0.0", std::move(descriptor));
  py::print("NativeInstance-CQ-4");
  instanceHolder->_videoCapture = videoCapture;
  instanceHolder->nativeInstance->setNetworkType(tgcalls::NetworkType::WiFi);
  instanceHolder->nativeInstance->setRequestedVideoAspect(1);
  instanceHolder->nativeInstance->setMuteMicrophone(false);
}


void NativeInstance::startCallP2PRaw(vector<RtcServer> servers, std::array<uint8_t, 256> authKey, bool isOutgoing, 
                                        std::shared_ptr<P2PRawAudioDeviceDescriptor> rawAudioDeviceDescriptor) {
  _P2PRawAudioDeviceDescriptor = std::move(rawAudioDeviceDescriptor);
  auto encryptionKeyValue = std::make_shared<std::array<uint8_t, 256>>();
  std::memcpy(encryptionKeyValue->data(), &authKey, 256);

  std::shared_ptr<tgcalls::VideoCaptureInterface> videoCapture = nullptr;

  tgcalls::MediaDevicesConfig mediaConfig = {
      .audioInputId = "",
      .audioOutputId = "",
      .inputVolume = 1.f,
      .outputVolume = 1.f};
  py::print("NativeInstance-CQ-1");
  tgcalls::Descriptor descriptor = {
      .config =
      tgcalls::Config{
          .initializationTimeout = 1000,
          .receiveTimeout = 1000,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = false,
          .enableStunMarking = true,
          .enableAEC = true,
          .enableNS = true,
          .enableAGC = true,
          .enableVolumeControl = true,
          .logPath = {"/home/tgcalls-log.txt"},
          .maxApiLayer = 92,
          .enableHighBitrateVideo = false,
          .preferredVideoCodecs = std::vector<std::string>(),
          .protocolVersion = tgcalls::ProtocolVersion::V0,
          //                .preferredVideoCodecs = {cricket::kVp9CodecName}
      },
      .persistentState = {std::vector<uint8_t>()},
      .initialNetworkType = tgcalls::NetworkType::WiFi,
      .encryptionKey = tgcalls::EncryptionKey(encryptionKeyValue, isOutgoing),
      .mediaDevicesConfig = mediaConfig,
      .videoCapture = videoCapture,
      .stateUpdated =
      [=](tgcalls::State state) {
        // printf("stateUpdated %d\n", (int)state);
        if (stateUpdatedCallback != nullptr) {
          stateUpdatedCallback((int)state);
        }
      },
      .signalBarsUpdated =
      [=](int count) {
        // printf("signalBarsUpdated %d\n", count);
      },
      .audioLevelUpdated =
      [=](float level) {
        // printf("audioLevelUpdated\n");
      },
      .remoteBatteryLevelIsLowUpdated =
      [=](bool isLow) {
        // printf("remoteBatteryLevelIsLowUpdated\n");
      },
      .remoteMediaStateUpdated =
      [=](tgcalls::AudioState audioState, tgcalls::VideoState videoState) {
        // printf("remoteMediaStateUpdated\n");
      },
      .remotePrefferedAspectRatioUpdated =
      [=](float ratio) {
        // printf("remotePrefferedAspectRatioUpdated\n");
      },
      .signalingDataEmitted =
      [=](const std::vector<uint8_t> &data) {
        // printf("signalingDataEmitted\n");
        if (signalingDataEmittedCallback != nullptr) {
          signalingDataEmittedCallback(data);
        }
      },
      .createAudioDeviceModule =  [&](webrtc::TaskQueueFactory *taskQueueFactory) -> rtc::scoped_refptr<webrtc::AudioDeviceModule> {
        return WrappedAudioDeviceModuleImpl::Create(
            webrtc::AudioDeviceModule::kDummyAudio, taskQueueFactory, std::move(_P2PRawAudioDeviceDescriptor)
        );
      },
  };
  py::print("NativeInstance-CQ-2");
  for (int i = 0, size = servers.size(); i < size; ++i) {
    RtcServer rtcServer = std::move(servers.at(i));

    const auto host = rtcServer.ip;
    const auto hostv6 = rtcServer.ipv6;
    const auto port = uint16_t(rtcServer.port);

    if (rtcServer.isStun) {
      const auto pushStun = [&](const string &host) {
        descriptor.rtcServers.push_back(
            tgcalls::RtcServer{.host = host, .port = port, .isTurn = false});
      };
      pushStun(host);
      pushStun(hostv6);
    }

    const auto username = rtcServer.login;
    const auto password = rtcServer.password;
    if (rtcServer.isTurn) {
      const auto pushTurn = [&](const string &host) {
        descriptor.rtcServers.push_back(tgcalls::RtcServer{
            .host = host,
            .port = port,
            .login = username,
            .password = password,
            .isTurn = true,
        });
      };
      pushTurn(host);
      pushTurn(hostv6);
    }
  }

  instanceHolder = std::make_unique<InstanceHolder>();
  instanceHolder->nativeInstance =
      tgcalls::Meta::Create("3.0.0", std::move(descriptor));
  py::print("NativeInstance-CQ-4");
  instanceHolder->_videoCapture = videoCapture;
  instanceHolder->nativeInstance->setNetworkType(tgcalls::NetworkType::WiFi);
  instanceHolder->nativeInstance->setRequestedVideoAspect(1);
  instanceHolder->nativeInstance->setMuteMicrophone(false);
}

void NativeInstance::setP2PVideoCapture(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height) {
  _pythonVideoSourceImpl = PythonVideoTrackSource::createPtr(
          std::make_unique<PythonSource>(std::move(getNextFrameBuffer), fps, width, height),fps);
  _videoCapture = tgcalls::VideoCaptureInterface::Create(
      tgcalls::StaticThreads::getThreads(),
      _pythonVideoSourceImpl,
      "python_video_track_source"
  );

  instanceHolder->nativeInstance->setVideoCapture(std::move(_videoCapture));
}

void NativeInstance::setP2PVideoRecord(std::string file) {
  if (_pythonVideoSourceImpl == nullptr) {
    return;
  }
  PythonVideoSourceImpl* pythonVideoSourceImpl = dynamic_cast<PythonVideoSourceImpl*>(_pythonVideoSourceImpl.get());
  // rtc::VideoSinkInterface<VideoFrameT> *sink, const rtc::VideoSinkWants &wants
  rtc::VideoSinkWants wants = rtc::VideoSinkWants();
  wants.resolutions = { rtc::VideoSinkWants::FrameSize(1280, 720) };
  PythonRecord record = PythonRecord(file);
  _pythonRecordSink = &record;
  pythonVideoSourceImpl->GetSource_().AddOrUpdateSink(&record, wants);
}