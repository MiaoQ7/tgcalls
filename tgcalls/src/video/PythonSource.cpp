#include "PythonSource.h"


PythonSource::PythonSource(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height, bool rotate):
  _fps(fps), _width(width), _height(height), _rotate(rotate) {
  _getNextFrameBuffer = std::move(getNextFrameBuffer);
}

PythonSource::PythonSource(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height):
  _fps(fps), _width(width), _height(height) {
  _getNextFrameBuffer = std::move(getNextFrameBuffer);
}

webrtc::VideoFrame PythonSource::next_frame() {
  auto *frame = new std::string{_getNextFrameBuffer()};

  rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(_width, _height);

  libyuv::ABGRToI420((uint8_t *) frame->data(), _width * 4,
                     buffer->MutableDataY(), buffer->StrideY(),
                     buffer->MutableDataU(), buffer->StrideU(),
                     buffer->MutableDataV(), buffer->StrideV(),
                     _width, _height);

  delete frame;

  bool rotate = false;
  if (_width < _height) {
    rotate = true;
  }

  auto builder = webrtc::VideoFrame::Builder()
    .set_video_frame_buffer(buffer->Scale(rotate ? _required_height : _required_width, rotate ? _required_width : _required_height));
  if (_rotate) {
    builder.set_rotation(webrtc::kVideoRotation_90);
  }

  return builder.build();
}
