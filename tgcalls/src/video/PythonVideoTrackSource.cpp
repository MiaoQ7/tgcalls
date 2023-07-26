#include <system_wrappers/include/sleep.h>

#include "PythonVideoTrackSource.h"

class PythonVideoSource : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
public:
  PythonVideoSource(std::shared_ptr<PythonSource> source, int fps) {
    // TODO rewrite this thread
    _data = std::make_shared<Data>();
    _data->is_running = true;
    _source = std::move(source);
    std::thread([fps, data = _data, source = _source] {
      std::uint32_t step = 0;
      while (data->is_running) {
        step++;

        int64_t current_time = rtc::TimeMillis();
        auto frame = source->next_frame();

        frame.set_id(static_cast<std::uint16_t>(step));
        frame.set_timestamp_us(rtc::TimeMicros());

        if (data->is_running) {
          data->broadcaster.OnFrame(frame);
        }

        int64_t delta_time_millis = rtc::TimeMillis() - current_time;
        if (delta_time_millis < 1000 / fps) {
          webrtc::SleepMs(1000 / fps - delta_time_millis);
        }
      }
      printf("PythonVideoSource LOOP EXIT\n");
    }).detach();
  }

  ~PythonVideoSource() {
    printf("PythonVideoSource::~PythonVideoSource\n");
    _data->is_running = false;
    _source.reset();
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
  std::shared_ptr<PythonSource> _source;
};

class PythonVideoSourceImpl : public webrtc::VideoTrackSource {
public:
  static rtc::scoped_refptr<PythonVideoSourceImpl> Create(std::shared_ptr<PythonSource> source, float fps) {
    return rtc::scoped_refptr<PythonVideoSourceImpl>(new rtc::RefCountedObject<PythonVideoSourceImpl>(std::move(source), fps));
  }

  explicit PythonVideoSourceImpl(std::shared_ptr<PythonSource> source, float fps) :
    VideoTrackSource(false), source_(std::move(source), fps) {
  }

  ~PythonVideoSourceImpl() {
    printf("PythonVideoSourceImpl::~PythonVideoSourceImpl()\n");
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

std::function<webrtc::VideoTrackSourceInterface*()> PythonVideoTrackSource::create(std::shared_ptr<PythonSource> frame_source, float fps) {
  auto source = PythonVideoSourceImpl::Create(std::move(frame_source), fps);
  return [source] {
    return source.get();
  };
}

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> PythonVideoTrackSource::createPtr(std::shared_ptr<PythonSource> frame_source, float fps) {
  return PythonVideoSourceImpl::Create(std::move(frame_source), fps);
}
