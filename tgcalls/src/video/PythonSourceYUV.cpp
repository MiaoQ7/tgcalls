#include "PythonSourceYUV.h"


PythonSourceYUV::PythonSourceYUV(std::function<std::string()> getNextFrameBuffer, float fps, int width, int height):
  PythonSource(getNextFrameBuffer, fps, width, height, false), _fps(fps), _width(width), _height(height) {
  _getNextFrameBuffer = std::move(getNextFrameBuffer);
}

webrtc::VideoFrame PythonSourceYUV::next_frame() {
  auto *frame = new std::string{_getNextFrameBuffer()};

  if (((uint8_t *) frame->data())[0] == 0) {
    if (black_buffer == nullptr) {
      black_buffer = webrtc::I420Buffer::Create(_width, _height);
      libyuv::ConvertToI420((uint8_t *) frame->data(), frame->size(),
        black_buffer->MutableDataY(), black_buffer->StrideY(),
        black_buffer->MutableDataU(), black_buffer->StrideU(),
        black_buffer->MutableDataV(), black_buffer->StrideV(),
        0, 0,
        _width, _height, 
        _width, _height,
        libyuv::kRotate0, libyuv::FOURCC_ABGR);
    }

    delete frame;
    
    auto builder = webrtc::VideoFrame::Builder()
    .set_video_frame_buffer(black_buffer);
  
    return builder.build();
  }

  rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(_width, _height);

  libyuv::ConvertToI420((uint8_t *) frame->data(), frame->size(),
                     buffer->MutableDataY(), buffer->StrideY(),
                     buffer->MutableDataU(), buffer->StrideU(),
                     buffer->MutableDataV(), buffer->StrideV(),
                     0, 0,
                     _width, _height, 
                     _width, _height,
                     libyuv::kRotate0, libyuv::FOURCC_I420);

  delete frame;

  auto builder = webrtc::VideoFrame::Builder()
    .set_video_frame_buffer(buffer);
  
  return builder.build();
}
