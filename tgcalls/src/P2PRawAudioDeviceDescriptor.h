#pragma once

#include <string>
#include <functional>

#include <pybind11/pybind11.h>

namespace py = pybind11;

class P2PRawAudioDeviceDescriptor {
public:
    // 获取播放缓存回调
    std::function<std::string(size_t)> _getPlayedBufferCallback = nullptr;
    // 设置录音缓存回调
    std::function<void(const py::bytes &frame, size_t)> _setRecordedBufferCallback = nullptr;
    // 是否暂停播放
    std::function<bool()> _isPlayoutPaused = nullptr;
    // 是否暂停录音
    std::function<bool()> _isRecordingPaused = nullptr;

    void _setRecordedBuffer(int8_t*, size_t) const;
    std::string* _getPlayoutBuffer(size_t) const;
};
