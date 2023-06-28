#include "P2PRawAudioDeviceDescriptor.h"

void P2PRawAudioDeviceDescriptor::_setRecordedBuffer(int8_t *frame, size_t length) const {
  auto bytes = std::string((const char *) frame, sizeof(int8_t) * length);
  _setRecordedBufferCallback(bytes, length);
}

std::string *P2PRawAudioDeviceDescriptor::_getPlayoutBuffer(size_t length) const {
  std::string frame = _getPlayedBufferCallback(length);

  return new std::string{frame};
}
