#pragma once

#include <functional>
#include <memory>
#include <string>
#include <api/scoped_refptr.h>
#include <api/video/video_frame.h>
#include <api/video/i420_buffer.h>
#include <libyuv.h>
#include <media/base/video_broadcaster.h>

using VideoFrameT = webrtc::VideoFrame;
class PythonRecord : public rtc::VideoSinkInterface<VideoFrameT> {
public:
  PythonRecord(std::string file);

  ~PythonRecord();

  void OnFrame(const VideoFrameT& frame);

  void OnDiscardedFrame();
private:
  std::string _file;
  FILE* _fp = nullptr;
};
