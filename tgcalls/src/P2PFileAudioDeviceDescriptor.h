#pragma once

#include <string>


class P2PFileAudioDeviceDescriptor {
public:
    // 输入文件名
    std::function<std::string()> _getInputFilename = nullptr;
    // 输出文件名
    std::function<std::string()> _getOutputFilename = nullptr;
    // 是否循环播放
    std::function<bool()> _isEndlessPlayout = nullptr;
    // 是否暂停播放
    std::function<bool()> _isPlayoutPaused = nullptr;
    // 是否暂停录音
    std::function<bool()> _isRecordingPaused = nullptr;
    // 播放到结束回调
    std::function<void(std::string)> _playoutEndedCallback = nullptr;
};
