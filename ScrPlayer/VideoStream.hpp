#pragma once
#include "ScrPlayer.h"
#include <stdbool.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include "Decoder.hpp"
#include "Recorder.hpp"

class VideoStream
{
private:
    SOCKET socket;
    VideoBuffer* video_buffer = nullptr;
    SDL_Thread* thread = nullptr;
    Decoder* decoder = nullptr;
    Recorder* recorder = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVCodecParserContext* parser = nullptr;
    // successive packets may need to be concatenated, until a non-config
    // packet is available
    bool has_pending;
    AVPacket pending;
    bool recv_packet(AVPacket* packet) {
        // The video stream contains raw packets, without time information. When we
        // record, we retrieve the timestamps separately, from a "meta" header
        // added by the server before each raw packet.
        //
        // The "meta" header length is 12 bytes:
        // [. . . . . . . .|. . . .]. . . . . . . . . . . . . . . ...
        //  <-------------> <-----> <-----------------------------...
        //        PTS        packet        raw packet
        //                    size
        //
        // It is followed by <packet_size> bytes containing the packet/frame.

        uint8_t header[HEADER_SIZE];
        //size_t r = net_recv_all(socket, header, HEADER_SIZE);
        size_t r = recv(socket, (char*)header, HEADER_SIZE, MSG_WAITALL);
        if (r < HEADER_SIZE) {
            return false;
        }
        //LOGI("Receive data length: %d", r);
        uint64_t pts = buffer_read64be(header);
        uint32_t len = buffer_read32be(&header[8]);
        assert(pts == NO_PTS || (pts & 0x8000000000000000) == 0);
        assert(len);
        if (av_new_packet(packet, len)) {
            LOGE("Could not allocate packet");
            return false;
        }

        //r = net_recv_all(socket, packet->data, len);
        r = recv(socket, (char*)packet->data, len, MSG_WAITALL);
        //LOGI("Receive Packet length: %d", r);
        if (r < 0 || ((uint32_t)r) < len) {
            av_packet_unref(packet);
            return false;
        }

        packet->pts = pts != NO_PTS ? (int64_t)pts : AV_NOPTS_VALUE;

        return true;
    }

    static void notify_stopped(void) {
        SDL_Event stop_event;
        stop_event.type = EVENT_STREAM_STOPPED;
        SDL_PushEvent(&stop_event);
    }

    bool process_config_packet(AVPacket* packet) {
        if (recorder && !recorder->push(packet)) {
            LOGE("Could not send config packet to recorder");
            return false;
        }
        return true;
    }
    bool process_frame(AVPacket* packet) {
        if (decoder && !decoder->push(packet)) {
            return false;
        }

        if (recorder) {
            packet->dts = packet->pts;

            if (!recorder->push(packet)) {
                LOGE("Could not send packet to recorder");
                return false;
            }
        }

        return true;
    }

    bool parse(AVPacket* packet) {
        uint8_t* in_data = packet->data;
        int in_len = packet->size;
        uint8_t* out_data = NULL;
        int out_len = 0;
        int r = av_parser_parse2(parser, codec_ctx,
            &out_data, &out_len, in_data, in_len,
            AV_NOPTS_VALUE, AV_NOPTS_VALUE, -1);

        // PARSER_FLAG_COMPLETE_FRAMES is set
        assert(r == in_len);
        (void)r;
        assert(out_len == in_len);

        if (parser->key_frame == 1) {
            packet->flags |= AV_PKT_FLAG_KEY;
        }

        bool ok = process_frame(packet);
        if (!ok) {
            LOGE("Could not process frame");
            return false;
        }

        return true;
    }
    bool push_packet(AVPacket* packet) {
        bool is_config = packet->pts == AV_NOPTS_VALUE;

        // A config packet must not be decoded immetiately (it contains no
        // frame); instead, it must be concatenated with the future data packet.
        if (has_pending || is_config) {
            size_t offset;
            if (has_pending) {
                offset = pending.size;
                if (av_grow_packet(&pending, packet->size)) {
                    LOGE("Could not grow packet");
                    return false;
                }
            }
            else {
                offset = 0;
                if (av_new_packet(&pending, packet->size)) {
                    LOGE("Could not create packet");
                    return false;
                }
                has_pending = true;
            }

            memcpy(pending.data + offset, packet->data, packet->size);

            if (!is_config) {
                // prepare the concat packet to send to the decoder
                pending.pts = packet->pts;
                pending.dts = packet->dts;
                pending.flags = packet->flags;
                packet = &pending;
            }
        }

        if (is_config) {
            // config packet
            bool ok = process_config_packet(packet);
            if (!ok) {
                return false;
            }
        }
        else {
            // data packet
            bool ok = parse(packet);

            if (has_pending) {
                // the pending packet must be discarded (consumed or error)
                has_pending = false;
                av_packet_unref(&pending);
            }

            if (!ok) {
                return false;
            }
        }
        return true;
    }

    static int run_stream(void* data) {
        VideoStream* stream = (VideoStream*)data;
        return stream->run();
    }
    int run(){
        AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            LOGE("H.264 decoder not found");
            goto end;
        }

        codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            LOGC("Could not allocate codec context");
            goto end;
        }

        if (decoder && !decoder->open(codec)) {
            LOGE("Could not open decoder");
            goto finally_free_codec_ctx;
        }

        if (recorder&&false) {
            if (!recorder->open(codec)) {
                LOGE("Could not open recorder");
                goto finally_close_decoder;
            }

            if (!recorder->start()) {
                LOGE("Could not start recorder");
                goto finally_close_recorder;
            }
        }

        parser = av_parser_init(AV_CODEC_ID_H264);
        if (!parser) {
            LOGE("Could not initialize parser");
            goto finally_stop_and_join_recorder;
        }

        // We must only pass complete frames to av_parser_parse2()!
        // It's more complicated, but this allows to reduce the latency by 1 frame!
        parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

        for (;;) {
            AVPacket packet;
            bool ok = recv_packet(&packet);
            //LOGI("Receiv packet ok:%d",ok);
            if (!ok) {
                // end of stream
                break;
            }

            ok = push_packet(&packet);
            //LOGI("Push packet ok:%d", ok);
            av_packet_unref(&packet);
            //LOGI("Av packet unref ok:%d", ok);
            if (!ok) {
                // cannot process packet (error already logged)
                break;
            }
        }

        LOGD("End of frames");

        if (has_pending) {
            av_packet_unref(&pending);
        }

        av_parser_close(parser);
    finally_stop_and_join_recorder:
        if (recorder) {
            recorder->stop();
            LOGI("Finishing recording...");
            recorder->join();
        }
    finally_close_recorder:
        if (recorder) {
            recorder->close();
        }
    finally_close_decoder:
        if (decoder) {
            decoder->close();
        }
    finally_free_codec_ctx:
        avcodec_free_context(&codec_ctx);
    end:
        notify_stopped();
        return 0;
    }


public:
    bool started = false;
    void init(SOCKET socket, Decoder* decoder, Recorder* recorder) {
        this->socket = socket;
        this->decoder = decoder,
        this->recorder = recorder;
        has_pending = false;
    }

    bool start() {
        LOGD("Starting stream thread");
        thread = SDL_CreateThread(run_stream, "stream", this);
        if (!thread) {
            LOGC("Could not start stream thread");
            return false;
        }
        started = true;
        return true;
    }

    void stop() {
        if (decoder) {
            decoder->interrupt();
        }
        started = false;
    }

    void join() {
        SDL_WaitThread(thread, NULL);
    }
};

