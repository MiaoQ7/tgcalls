#pragma once

#include <functional>
#include <memory>
#include <string>
#include <api/scoped_refptr.h>
#include <api/video/video_frame.h>
#include <api/video/i420_buffer.h>
#include <libyuv.h>
#include "PythonSource.h"


class PythonSourceYUV : public PythonSource {
public:
  PythonSourceYUV(std::function<std::string()>, float, int, int);
  ~PythonSourceYUV() {
    if (_fp) {
      fclose(_fp);
    }
  };

  webrtc::VideoFrame next_frame();

private:
  std::function<std::string()> _getNextFrameBuffer = nullptr;
  float _fps;
  int _width;
  int _height;
  FILE* _fp;
  rtc::scoped_refptr<webrtc::I420Buffer> black_buffer = nullptr;
};
