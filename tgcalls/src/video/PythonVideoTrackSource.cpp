#include <system_wrappers/include/sleep.h>

#include "PythonVideoTrackSource.h"

PythonVideoSource::PythonVideoSource(std::unique_ptr<PythonSource> source, int fps) {
    // TODO rewrite this thread
    _data = std::make_shared<Data>();
    _data->is_running = true;

    std::thread([fps, data = _data, source = std::move(source)] {
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
    }).detach();
  }

std::function<webrtc::VideoTrackSourceInterface*()> PythonVideoTrackSource::create(std::unique_ptr<PythonSource> frame_source, float fps) {
  auto source = PythonVideoSourceImpl::Create(std::move(frame_source), fps);
  return [source] {
    return source.get();
  };
}

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> PythonVideoTrackSource::createPtr(std::unique_ptr<PythonSource> frame_source, float fps) {
  return PythonVideoSourceImpl::Create(std::move(frame_source), fps);
}
