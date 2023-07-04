#pragma once

#include <pybind11/pybind11.h>

#include <modules/audio_device/include/audio_device.h>
#include <tgcalls/ThreadLocalObject.h>
#include <tgcalls/VideoCaptureInterface.h>

#include "config.h"
#include "InstanceHolder.h"
#include "RtcServer.h"
#include "WrappedAudioDeviceModuleImpl.h"

#include "video/PythonSource.h"
#include "video/PythonRecord.h"
#include "video/PythonVideoTrackSource.h"

namespace py = pybind11;

class NativeInstance {
public:
    std::unique_ptr<InstanceHolder> instanceHolder;

    bool _logToStdErr;
    string _logPath;

    int _outgoingAudioBitrateKbit = 128;

    std::function<void(const std::vector<uint8_t> &data)> signalingDataEmittedCallback;
    std::function<void(int)> stateUpdatedCallback;

    std::function<void(tgcalls::GroupJoinPayload payload)> _emitJoinPayloadCallback = nullptr;
    std::function<void(bool)> _networkStateUpdated = nullptr;

    std::shared_ptr<FileAudioDeviceDescriptor> _fileAudioDeviceDescriptor;
    std::shared_ptr<RawAudioDeviceDescriptor> _rawAudioDeviceDescriptor;
    std::shared_ptr<tgcalls::VideoCaptureInterface> _videoCapture;
    std::shared_ptr<P2PFileAudioDeviceDescriptor> _P2PFileAudioDeviceDescriptor;
    std::shared_ptr<P2PRawAudioDeviceDescriptor> _P2PRawAudioDeviceDescriptor;

    std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> _pythonRecordSink = nullptr;

    NativeInstance(bool, string);
    ~NativeInstance();

    void startCall(vector<RtcServer> servers, std::array<uint8_t, 256> authKey, bool isOutgoing, std::string logPath);

    // 语音通话
    void startCallVoice(vector<RtcServer> servers, std::array<uint8_t, 256> authKey, bool isOutgoing, std::string tag, std::string audioInputId, std::string audioOutputId);

    void stop();

    void setupGroupCall(
            std::function<void(tgcalls::GroupJoinPayload)> &,
            std::function<void(bool)> &,
            int
    );

    void startGroupCall(std::shared_ptr<FileAudioDeviceDescriptor>);
    void startGroupCall(std::shared_ptr<RawAudioDeviceDescriptor>);
    void startGroupCall(std::string, std::string);
    void stopGroupCall() const;
    bool isGroupCallNativeCreated() const;

    void setIsMuted(bool isMuted) const;
    void setVolume(uint32_t ssrc, double volume) const;
    void emitJoinPayload(std::function<void(tgcalls::GroupJoinPayload payload)> const &) const;
    void setConnectionMode(tgcalls::GroupConnectionMode, bool) const;

    void restartAudioInputDevice() const;
    void restartAudioOutputDevice() const;

    void stopAudioDeviceModule() const;
    void startAudioDeviceModule() const;

    std::vector<tgcalls::GroupInstanceInterface::AudioDevice> getPlayoutDevices() const;
    std::vector<tgcalls::GroupInstanceInterface::AudioDevice> getRecordingDevices() const;

    void setAudioOutputDevice(std::string id) const;
    void setAudioInputDevice(std::string id) const;

    void setVideoCapture(std::function<std::string()>, float, int, int);

    void receiveSignalingData(std::vector<uint8_t> &data) const;
    void setJoinResponsePayload(std::string const &) const;
    void setSignalingDataEmittedCallback(const std::function<void(const std::vector<uint8_t> &data)> &f);
    void setStateUpdatedCallback(const std::function<void(int)> &f);
    void startCallP2P(vector<RtcServer> servers, std::array<uint8_t, 256> authKey, bool isOutgoing, std::shared_ptr<P2PFileAudioDeviceDescriptor> fileAudioDeviceDescriptor);
    void startCallP2PRaw(vector<RtcServer> servers, std::array<uint8_t, 256> authKey, bool isOutgoing, std::shared_ptr<P2PRawAudioDeviceDescriptor> rawAudioDeviceDescriptor);
    void setP2PVideoCapture(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height);

    void setP2PVideoRecord(std::string file);

    void setRequestedVideoAspect(float aspect);
private:
    void createInstanceHolder(
        std::function<rtc::scoped_refptr<webrtc::AudioDeviceModule>(webrtc::TaskQueueFactory*)>,
        std::string,
        std::string
    );
};
