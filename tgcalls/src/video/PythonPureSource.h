#pragma once

#include <functional>
#include <memory>
#include <string>
#include <api/scoped_refptr.h>
#include <api/video/video_frame.h>
#include <api/video/i420_buffer.h>
#include <libyuv.h>
#include "PythonSource.h"


class PythonPureSource : public PythonSource {
public:
  PythonPureSource(std::function<std::string()>, float, int, int, bool);
  PythonPureSource(std::function<std::string()>, float, int, int);
  ~PythonPureSource() = default;

  virtual webrtc::VideoFrame next_frame();

private:
  std::function<std::string()> _getNextFrameBuffer = nullptr;
  float _fps;
  int _width;
  int _height;
  bool _rotate = false;

  // 360P  640x360  720P  1280x720
  int _required_width = 1280;
  int _required_height = 720;
};
