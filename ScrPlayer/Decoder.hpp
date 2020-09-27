#pragma once
#include "ScrPlayer.h"
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include "compat.h"
#include "events.h"
#include "videoBuffer.hpp"
#include "util/buffer_util.h"
#include "util/log.h"
#define EVENT_NEW_SESSION SDL_USEREVENT
#define EVENT_NEW_FRAME (SDL_USEREVENT + 1)
#define EVENT_STREAM_STOPPED (SDL_USEREVENT + 2)

class Decoder
{
private:
    VideoBuffer* videoBuffer;
    AVCodecContext* codec_ctx;
public:
    // set the decoded frame as ready for rendering, and notify
    void push_frame() {
        if (videoBuffer->offer_decoded_frame()) {
            // the previous EVENT_NEW_FRAME will consume this frame
            return;
        }
        SDL_Event new_frame_event;
        new_frame_event.type = EVENT_NEW_FRAME;
        SDL_PushEvent(&new_frame_event);
    }

    void init(VideoBuffer* vb) {
        videoBuffer = vb;
    }

    bool open(const AVCodec* codec) {
        codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            LOGC("Could not allocate decoder context");
            return false;
        }

        if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
            LOGE("Could not open codec");
            avcodec_free_context(&codec_ctx);
            return false;
        }
        return true;
    }

    void close() {
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
    }

    bool push(const AVPacket* packet) {
        // the new decoding/encoding API has been introduced by:
        // <http://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=7fc329e2dd6226dfecaa4a1d7adf353bf2773726>
#ifdef SCRCPY_LAVF_HAS_NEW_ENCODING_DECODING_API
        int ret;
        if ((ret = avcodec_send_packet(codec_ctx, packet)) < 0) {
            LOGE("Could not send video packet: %d", ret);
            return false;
        }
        ret = avcodec_receive_frame(codec_ctx,videoBuffer->decoding_frame);
        if (!ret) {
            // a frame was received
            push_frame();
        }
        else if (ret != AVERROR(EAGAIN)) {
            LOGE("Could not receive video frame: %d", ret);
            return false;
        }
#else
        int got_picture;
        int len = avcodec_decode_video2(codec_ctx,videoBuffer->decoding_frame,&got_picture,
            packet);
        if (len < 0) {
            LOGE("Could not decode video packet: %d", len);
            return false;
        }
        if (got_picture) {
            push_frame();
        }
#endif
        return true;
    }

    void interrupt() {
        videoBuffer->interrupt();
    }
};

