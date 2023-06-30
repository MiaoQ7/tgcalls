#pragma once

#include <functional>
#include <memory>
#include <string>
#include <api/scoped_refptr.h>
#include <api/video/video_frame.h>
#include <api/video/i420_buffer.h>
#include <libyuv.h>
#include <media/base/video_broadcaster.h>

class PythonRecord : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
  PythonRecord(std::string file);

  PythonRecord(std::string file, int width, int height);

  ~PythonRecord();

  void OnFrame(const webrtc::VideoFrame& frame);

  void OnDiscardedFrame();

  static std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> createPtr(std::string file);

  static std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> createPtr(std::string file, int width, int height);
private:
  std::string _file;
  int width = 640;
  int height = 360;
  FILE* _fp = nullptr;
};
