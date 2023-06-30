#pragma once

#include <thread>
#include <memory>
#include <functional>
#include <api/scoped_refptr.h>
#include <media/base/video_broadcaster.h>
#include <pc/video_track_source.h>
#include <libyuv.h>

#include "PythonSource.h"

namespace webrtc {
  class VideoTrackSourceInterface;
  class VideoFrame;
}

class PythonVideoTrackSource {
public:
  static std::function<webrtc::VideoTrackSourceInterface*()> create(std::unique_ptr<PythonSource> source, float fps);
  static rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> createPtr(std::unique_ptr<PythonSource> source, float fps);
};

class PythonVideoSource : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
public:
  PythonVideoSource(std::unique_ptr<PythonSource> source, int fps);

  ~PythonVideoSource() {
    _data->is_running = false;
  }

  // 视频接收接口
  using VideoFrameT = webrtc::VideoFrame;
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrameT> *sink, const rtc::VideoSinkWants &wants) override {
    _data->broadcaster.AddOrUpdateSink(sink, wants);
  }

  // TODO
  // RemoveSink must guarantee that at the time the method returns,
  // there is no current and no future calls to VideoSinkInterface::OnFrame.
  void RemoveSink(rtc::VideoSinkInterface<VideoFrameT> *sink) {
    _data->is_running = false;
    _data->broadcaster.RemoveSink(sink);
  }

private:
  struct Data {
    std::atomic<bool> is_running;
    rtc::VideoBroadcaster broadcaster;
  };
  std::shared_ptr<Data> _data;
};

class PythonVideoSourceImpl : public webrtc::VideoTrackSource {
public:
  static rtc::scoped_refptr<PythonVideoSourceImpl> Create(std::unique_ptr<PythonSource> source, float fps) {
    return rtc::scoped_refptr<PythonVideoSourceImpl>(new rtc::RefCountedObject<PythonVideoSourceImpl>(std::move(source), fps));
  }

  explicit PythonVideoSourceImpl(std::unique_ptr<PythonSource> source, float fps) :
    VideoTrackSource(false), source_(std::move(source), fps) {
  }

  PythonVideoSource GetSource_() {
    return source_;
  }

protected:
  PythonVideoSource source_;
  rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override {
    return &source_;
  }
};
