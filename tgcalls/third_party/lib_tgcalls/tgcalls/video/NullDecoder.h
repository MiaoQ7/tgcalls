#ifndef TGCALLS_NULL_DECODER_H
#define TGCALLS_NULL_DECODER_H

/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** NullDecoder.h
**
** -------------------------------------------------------------------------*/

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_codec.h"
#include "video/EncodedVideoFrameBuffer.h"

#pragma once
namespace tgcalls {
class NullDecoder : public webrtc::VideoDecoder {
  public:
 	NullDecoder(const webrtc::SdpVideoFormat& format) : m_format(format) {}
  virtual ~NullDecoder() override {}

	int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                             int32_t number_of_cores) override { 
		m_number_of_cores = number_of_cores;
    m_codec_settings = codec_settings;
		return true; 
	}

    int32_t Release() override {
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override {
		m_decoded_image_callback = callback;
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t Decode(const webrtc::EncodedImage& input_image, bool /*missing_frames*/, int64_t render_time_ms = -1) override {
	    if (!m_decoded_image_callback) {
			RTC_LOG(LS_WARNING) << "RegisterDecodeCompleteCallback() not called";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}
		rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData = input_image.GetEncodedData();
		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = new rtc::RefCountedObject<EncodedVideoFrameBuffer>(m_codec_settings->width, m_codec_settings->height, encodedData);
    // rtc::make_ref_counted<EncodedVideoFrameBuffer>(m_codec_settings->width, m_codec_settings->height, encodedData);
		
		webrtc::VideoFrame frame(buffer, webrtc::kVideoRotation_0, render_time_ms * rtc::kNumMicrosecsPerMillisec);
		frame.set_timestamp(input_image.Timestamp());
		frame.set_ntp_time_ms(input_image.NtpTimeMs());

		RTC_LOG(LS_VERBOSE) << "Decode " << frame.id() << " " << input_image._frameType << " " <<  buffer->width() << "x" <<  buffer->height() << " " <<  buffer->GetI420()->StrideY();

		m_decoded_image_callback->Decoded(frame);

		return WEBRTC_VIDEO_CODEC_OK;		
	}

    const char* ImplementationName() const override { return "NullDecoder"; }

	webrtc::DecodedImageCallback* m_decoded_image_callback;
	webrtc::SdpVideoFormat m_format;
  const webrtc::VideoCodec *m_codec_settings;
  int32_t m_number_of_cores;
};
} // namespace tgcalls
#endif 