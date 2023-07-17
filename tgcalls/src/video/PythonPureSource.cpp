#include "PythonPureSource.h"
#include "tgcalls/video/EncodedVideoFrameBuffer.h"
#include "api/video/encoded_image.h"


PythonPureSource::PythonPureSource(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height, bool rotate):
  _fps(fps), _width(width), _height(height), _rotate(rotate) {
  _getNextFrameBuffer = std::move(getNextFrameBuffer);
}

PythonPureSource::PythonPureSource(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height):
  _fps(fps), _width(width), _height(height) {
  _getNextFrameBuffer = std::move(getNextFrameBuffer);
}

webrtc::VideoFrame PythonPureSource::next_frame() {
  auto *frame = new std::string{_getNextFrameBuffer()};
  auto buffer = new rtc::RefCountedObject<tgcalls::EncodedVideoFrameBuffer>
    (_width, _height, webrtc::EncodedImageBuffer::Create((uint8_t *)frame->data(), frame->size()));
  
  auto builder = webrtc::VideoFrame::Builder()
    .set_video_frame_buffer(buffer);

  delete frame;
  return builder.build();
}
