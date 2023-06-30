#include "PythonRecord.h"


PythonRecord::PythonRecord(std::string file) {
  _file = file;
  _fp = fopen(_file.c_str(), "wb+");
}

PythonRecord::~PythonRecord() {
  printf("===========PythonRecord Destory Start\n");
  if (_fp != nullptr) {
    fflush(_fp);
    fclose(_fp);
    _fp = nullptr;
  }
  printf("===========PythonRecord Destory END\n");
}

void PythonRecord::OnFrame(const webrtc::VideoFrame& frame) {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(frame.video_frame_buffer());
  rtc::scoped_refptr<webrtc::I420Buffer> newYUVFrame = webrtc::I420Buffer::Create(width, height);
  newYUVFrame->ScaleFrom(*buffer->ToI420());

  if (_fp != nullptr) {
    fwrite(newYUVFrame.get()->GetI420()->DataY(), 1, width * height, _fp);
    fwrite(newYUVFrame.get()->GetI420()->DataU(), 1, width * height / 4, _fp);
    fwrite(newYUVFrame.get()->GetI420()->DataV(), 1, width * height / 4, _fp);
    fflush(_fp);
  }
}

void PythonRecord::OnDiscardedFrame() {
  printf("-- OnDiscardedFrame --\n");
}

std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> PythonRecord::createPtr(std::string file) {
  return std::make_shared<PythonRecord>(file);
}

std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> PythonRecord::createPtr(std::string file, int width, int height)
{
  return std::make_shared<PythonRecord>(file, width, height);
}
