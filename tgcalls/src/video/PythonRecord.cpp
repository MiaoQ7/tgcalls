#include "PythonRecord.h"


PythonRecord::PythonRecord(std::string file) {
  _file = file;
  _fp = fopen(_file.c_str(), "wb+");
}

PythonRecord::~PythonRecord() {
  if (_fp != nullptr) {
    fflush(_fp);
    fclose(_fp);
    _fp = nullptr;
  }
}

void PythonRecord::OnFrame(const VideoFrameT& frame) {
  auto vfb = frame.video_frame_buffer();
  if (_fp != nullptr) {
    fwrite(vfb.get()->GetI420()->DataY(), 1, frame.height() * frame.width(), _fp);
    fwrite(vfb.get()->GetI420()->DataU(), 1, frame.height() * frame.width() / 4, _fp);
    fwrite(vfb.get()->GetI420()->DataV(), 1, frame.height() * frame.width() / 4, _fp);
    fflush(_fp);
  }
}

void PythonRecord::OnDiscardedFrame() {
  printf("-- OnDiscardedFrame --");
}