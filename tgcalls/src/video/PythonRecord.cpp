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
  auto vfb = frame.video_frame_buffer();
  if (_fp != nullptr) {
    fwrite(vfb.get()->GetI420()->DataY(), 1, frame.height() * frame.width(), _fp);
    fwrite(vfb.get()->GetI420()->DataU(), 1, frame.height() * frame.width() / 4, _fp);
    fwrite(vfb.get()->GetI420()->DataV(), 1, frame.height() * frame.width() / 4, _fp);
    fflush(_fp);
  }
}

void PythonRecord::OnDiscardedFrame() {
  printf("-- OnDiscardedFrame --\n");
}

std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> PythonRecord::createPtr(std::string file) {
  return std::make_shared<PythonRecord>(file);
}