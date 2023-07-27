#include <iostream>
#include <utility>

#include <rtc_base/ssl_adapter.h>

#include "NativeInstance.h"

namespace py = pybind11;

auto noticeDisplayed = false;

NativeInstance::NativeInstance(bool logToStdErr, string logPath, string host, uint16_t port, string login, string password)
    : _logToStdErr(logToStdErr), _logPath(std::move(logPath)), _host(std::move(host)), _port(port), _login(std::move(login)), _password(std::move(password)) {
  if (!noticeDisplayed) {
    auto ver = std::string(PROJECT_VER);
    auto dev = std::count(ver.begin(), ver.end(), '.') == 3 ? " DEV" : "";
    py::print("tgcalls v" + ver + dev + ", Copyright (C) 2020-2023 Ilya (Marshal) <https://github.com/MarshalX>");
    py::print("Licensed under the terms of the GNU Lesser General Public License v3 (LGPLv3) \n\n");

    noticeDisplayed = true;
  }
  // if (!host.empty()) {
  //   _proxy = std::make_unique<tgcalls::Proxy>(host, port, login, password);
  // } else {
  //   _proxy = std::make_unique<tgcalls::Proxy>();
  // }
  rtc::InitializeSSL();
  py::print("NativeInstance-1");
  tgcalls::Register<tgcalls::InstanceImpl>();
  py::print("NativeInstance-2");
}

NativeInstance::~NativeInstance() {
  printf("NativeInstance::~NativeInstance()\n");
  if (_videoCapture) {
    _videoCapture.reset();
  }
  if (_proxy) {
    _proxy.reset();
  }
  if (instanceHolder) {
    instanceHolder.reset();
  }
}

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
          std::make_shared<PythonSource>(std::move(getNextFrameBuffer), fps, width, height),fps),
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

  auto proxy = std::make_unique<tgcalls::Proxy>();
  
  if (!_host.empty()) {
    proxy->host = _host;
    proxy->port = _port;
    proxy->login = _login;
    proxy->password = _password;
  }

  std::vector<std::string> preferredVideoCodecs = {/*cricket::kH264CodecName*/};
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
          .initializationTimeout = 30,
          .receiveTimeout = 10,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = true,
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
          .preferredVideoCodecs = std::move(preferredVideoCodecs),
          .protocolVersion = tgcalls::ProtocolVersion::V0,
          // .preferredVideoCodecs = preferredVideoCodecs,
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
  descriptor.proxy = std::move(proxy);
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

  auto proxy = std::make_unique<tgcalls::Proxy>();
  
  if (!_host.empty()) {
    proxy->host = _host;
    proxy->port = _port;
    proxy->login = _login;
    proxy->password = _password;
  }
  std::vector<std::string> preferredVideoCodecs = {/*cricket::kH264CodecName*/};
  tgcalls::MediaDevicesConfig mediaConfig = {
      .audioInputId = audioInputId,
      .audioOutputId = audioOutputId,
      .inputVolume = 1.f,
      .outputVolume = 1.f};
  py::print("NativeInstance-CQ-1");
  tgcalls::Descriptor descriptor = {
      .config =
      tgcalls::Config{
          .initializationTimeout = 30,
          .receiveTimeout = 10,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = true,
          .enableStunMarking = true,
          .enableAEC = true,
          .enableNS = true,
          .enableAGC = true,
          .enableVolumeControl = true,
          .logPath = {"/home/tgcalls-log.txt"},
          .maxApiLayer = 92,
          .enableHighBitrateVideo = false,
          .preferredVideoCodecs = std::move(preferredVideoCodecs),
          .protocolVersion = tgcalls::ProtocolVersion::V0,
          // .preferredVideoCodecs = preferredVideoCodecs,
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
  descriptor.proxy = std::move(proxy);
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
      printf("trafficStats bytesSentWifi: %d  bytesReceivedWifi: %d\n", state.trafficStats.bytesSentWifi, state.trafficStats.bytesReceivedWifi);
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

  auto proxy = std::make_unique<tgcalls::Proxy>();
  
  if (!_host.empty()) {
    proxy->host = _host;
    proxy->port = _port;
    proxy->login = _login;
    proxy->password = _password;
  }
  std::vector<std::string> preferredVideoCodecs = {/*cricket::kH264CodecName*/};
  tgcalls::MediaDevicesConfig mediaConfig = {
      .audioInputId = "",
      .audioOutputId = "",
      .inputVolume = 1.f,
      .outputVolume = 1.f};
  py::print("NativeInstance-CQ-1");
  tgcalls::Descriptor descriptor = {
      .config =
      tgcalls::Config{
          .initializationTimeout = 30,
          .receiveTimeout = 10,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = true,
          .enableStunMarking = true,
          .enableAEC = true,
          .enableNS = true,
          .enableAGC = true,
          .enableVolumeControl = true,
          .logPath = {"/home/tgcalls-log.txt"},
          .maxApiLayer = 92,
          .enableHighBitrateVideo = false,
          .preferredVideoCodecs = std::move(preferredVideoCodecs),
          .protocolVersion = tgcalls::ProtocolVersion::V0,
          // .preferredVideoCodecs = preferredVideoCodecs,
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
  descriptor.proxy = std::move(proxy);
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

  auto proxy = std::make_unique<tgcalls::Proxy>();
  
  if (_host != "") {
    printf("Proxy: %s:%d\n", _host.c_str(), _port);
    proxy->host = _host;
    proxy->port = _port;
    proxy->login = _login;
    proxy->password = _password;
  }
  std::vector<std::string> preferredVideoCodecs = {/*cricket::kH264CodecName*/};
  tgcalls::MediaDevicesConfig mediaConfig = {
      .audioInputId = "",
      .audioOutputId = "",
      .inputVolume = 1.f,
      .outputVolume = 1.f};
  py::print("NativeInstance-CQ-1");
  tgcalls::Descriptor descriptor = {
      .config =
      tgcalls::Config{
          .initializationTimeout = 30,
          .receiveTimeout = 10,
          .dataSaving = tgcalls::DataSaving::Never,
          .enableP2P = false,
          .allowTCP = true,
          .enableStunMarking = true,
          .enableAEC = true,
          .enableNS = true,
          .enableAGC = true,
          .enableVolumeControl = true,
          .logPath = {"/home/tgcalls-log.txt"},
          .maxApiLayer = 92,
          .enableHighBitrateVideo = false,
          .preferredVideoCodecs = std::move(preferredVideoCodecs),
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
  descriptor.proxy = std::move(proxy);
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
  py::print("NativeInstance-CQ-5");
  instanceHolder->nativeInstance->setNetworkType(tgcalls::NetworkType::WiFi);
  py::print("NativeInstance-CQ-6");
  instanceHolder->nativeInstance->setRequestedVideoAspect(1);
  py::print("NativeInstance-CQ-7");
  instanceHolder->nativeInstance->setMuteMicrophone(false);
  py::print("NativeInstance-CQ-8");
}

void NativeInstance::setP2PVideoCapture(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height, bool rotate) {
  _videoCapture = tgcalls::VideoCaptureInterface::Create(
      tgcalls::StaticThreads::getThreads(),
      PythonVideoTrackSource::createPtr(
          std::make_shared<PythonSource>(std::move(getNextFrameBuffer), fps, width, height, rotate),fps),
      "python_video_track_source"
  );

  instanceHolder->nativeInstance->setVideoCapture(std::move(_videoCapture));
}

void NativeInstance::setP2PVideoCaptureYUV(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height)
{
  _videoCapture = tgcalls::VideoCaptureInterface::Create(
      tgcalls::StaticThreads::getThreads(),
      PythonVideoTrackSource::createPtr(
          std::make_shared<PythonSourceYUV>(std::move(getNextFrameBuffer), fps, width, height),fps),
      "python_video_track_source"
  );

  instanceHolder->nativeInstance->setVideoCapture(std::move(_videoCapture));
}

void NativeInstance::setP2PVideoCapturePure(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height)
{
  _videoCapture = tgcalls::VideoCaptureInterface::Create(
      tgcalls::StaticThreads::getThreads(),
      PythonVideoTrackSource::createPtr(
          std::make_shared<PythonPureSource>(std::move(getNextFrameBuffer), fps, width, height),fps),
      "python_video_track_source"
  );

  instanceHolder->nativeInstance->setVideoCapture(std::move(_videoCapture));
}

void NativeInstance::setP2PVideoRecord(std::string file) {
  // printf("setP2PVideoRecord 1");
  if (instanceHolder == nullptr || instanceHolder->nativeInstance == nullptr) {
    return;
  }
  // printf("setP2PVideoRecord 2");
  _pythonRecordSink = PythonRecord::createPtr(file);
  instanceHolder->nativeInstance->setIncomingVideoOutput(std::move(_pythonRecordSink));
  // printf("setP2PVideoRecord 3");
}

void NativeInstance::setRequestedVideoAspect(float aspect)
{
  if (instanceHolder == nullptr || instanceHolder->nativeInstance == nullptr) {
    return;
  }
  instanceHolder->nativeInstance->setRequestedVideoAspect(aspect);
}

void NativeInstance::cacheVideo(std::function<std::string()> getNextFrameBuffer, int width, int height, bool rotate, std::string cacheFilePath)
{
  std::uint32_t step = 0;
  auto _fp = fopen(cacheFilePath.c_str(), "wb");
  while (true) {
    step++;
    auto frame = new std::string{getNextFrameBuffer()};

    if (frame->compare("#end") == 0) {
      delete frame;
      fflush(_fp);
      fclose(_fp);
      break;
    }

    if (frame->empty()) {
      delete frame;
      continue;
    }

    rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(width, height);

    if (width < height) {
      rotate = true;
    }

    libyuv::ABGRToI420((uint8_t *) frame->data(), width * 4,
                      buffer->MutableDataY(), buffer->StrideY(),
                      buffer->MutableDataU(), buffer->StrideU(),
                      buffer->MutableDataV(), buffer->StrideV(),
                      width, height);

    delete frame;

    int dst_width = rotate ? 720 : 1280;
    int dst_height = rotate ? 1280 : 720;
    
    auto builder = webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(buffer->Scale(dst_width, dst_height));
    // if (rotate) {
    //   print("c++---rotate")
    //   builder.set_rotation(webrtc::kVideoRotation_90);
    // }

    auto video_frame = builder.build();
    

    // 写到文件
    if (_fp != nullptr) {
      fwrite(video_frame.video_frame_buffer().get()->GetI420()->DataY(), 1, dst_width * dst_height, _fp);
      fwrite(video_frame.video_frame_buffer().get()->GetI420()->DataU(), 1, dst_width * dst_height / 4, _fp);
      fwrite(video_frame.video_frame_buffer().get()->GetI420()->DataV(), 1, dst_width * dst_height / 4, _fp);
      fflush(_fp);
    }
  }
}

