#ifndef TGCALLS_NULL_ENCODER_H
#define TGCALLS_NULL_ENCODER_H
/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** NullEncoder.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "common_video/h264/h264_common.h"

namespace tgcalls {
class NullEncoder : public webrtc::VideoEncoder {
   public:
	NullEncoder(const webrtc::SdpVideoFormat& format) : m_format(format) {}
    virtual ~NullEncoder() override {}

    int32_t InitEncode(const webrtc::VideoCodec* codec_settings, const webrtc::VideoEncoder::Settings& settings) override {
		RTC_LOG(LS_WARNING) << "InitEncode";
		return WEBRTC_VIDEO_CODEC_OK;
	}
    int32_t Release() override {
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override {
		m_encoded_image_callback = callback;
    	return WEBRTC_VIDEO_CODEC_OK;
	}
    void SetRates(const RateControlParameters& parameters) override {
		RTC_LOG(LS_VERBOSE) << "SetRates() " << parameters.target_bitrate.ToString() << " " << parameters.bitrate.ToString() << " " << parameters.bandwidth_allocation.kbps() << " " << parameters.framerate_fps;
	}

    int32_t Encode(const webrtc::VideoFrame& frame, const std::vector<webrtc::VideoFrameType>* frame_types) override {
	    if (!m_encoded_image_callback) {
			RTC_LOG(LS_WARNING) << "RegisterEncodeCompleteCallback() not called";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}

		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = frame.video_frame_buffer();
		if (buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
			RTC_LOG(LS_WARNING) << "buffer type must be kNative";
			return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;		
		}

		// compute frametype
		uint8_t* data = (uint8_t*)buffer->GetI420()->DataY();
		size_t dataSize = buffer->GetI420()->StrideY();
		webrtc::VideoFrameType frameType = webrtc::VideoFrameType::kVideoFrameDelta;
		std::vector<webrtc::H264::NaluIndex> naluIndexes = webrtc::H264::FindNaluIndices(data, dataSize);
		for (webrtc::H264::NaluIndex  index : naluIndexes) {
			webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(data[index.payload_start_offset]);
			if (nalu_type ==  webrtc::H264::NaluType::kIdr) {
				frameType = webrtc::VideoFrameType::kVideoFrameKey;
				break;
			}
		}

		// build webrtc::EncodedImage
		webrtc::EncodedImage encoded_image;
		encoded_image.SetEncodedData(webrtc::EncodedImageBuffer::Create(data, dataSize));
		encoded_image.SetTimestamp(frame.timestamp());
		encoded_image.ntp_time_ms_ = frame.ntp_time_ms();
		encoded_image._frameType = frameType;

		RTC_LOG(LS_INFO) << "EncodedImage " << frame.id() << " " << encoded_image._frameType << " " <<  buffer->width() << "x" <<  buffer->height() << " " <<  buffer->GetI420()->StrideY();

		// forward to callback
		webrtc::CodecSpecificInfo codec_specific;
		codec_specific.codecType = webrtc::VideoCodecType::kVideoCodecH264;
        webrtc::EncodedImageCallback::Result result = m_encoded_image_callback->OnEncodedImage(encoded_image, &codec_specific);
        if (result.error == webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED) {
            RTC_LOG(LS_ERROR) << "Error in parsing EncodedImage" << encoded_image._frameType;
		}
		RTC_LOG(LS_INFO) << "EncodedImage WEBRTC_VIDEO_CODEC_OK";
		return WEBRTC_VIDEO_CODEC_OK;
	}

    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override {
	    webrtc::VideoEncoder::EncoderInfo info;
		info.supports_native_handle = true;
		info.has_trusted_rate_controller = true;
		info.implementation_name = "NullEncoder";
		return info;
	}

  private:
	webrtc::EncodedImageCallback* m_encoded_image_callback;
	webrtc::SdpVideoFormat m_format;	
};
}  // namespace tgcalls
#endif