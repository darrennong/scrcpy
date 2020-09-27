#pragma once
#include "ScrPlayer.h"
#include <stdbool.h>
#include <libavformat/avformat.h>
#include "util/queue.h"
#include "util/log.h"
#include "util/lock.h"
#include "common.h"
static const AVRational SCRCPY_TIME_BASE = { 1, 1000000 }; // timestamps in us
typedef struct record_packet {
    AVPacket packet;
    struct record_packet* next;
}record_packet;

static struct record_packet*
record_packet_new(const AVPacket* packet) {
    struct record_packet* rec = (struct record_packet*)SDL_malloc(sizeof(*rec));
    if (!rec) {
        return NULL;
    }

    // av_packet_ref() does not initialize all fields in old FFmpeg versions
    // See <https://github.com/Genymobile/scrcpy/issues/707>
    av_init_packet(&rec->packet);

    if (av_packet_ref(&rec->packet, packet)) {
        SDL_free(rec);
        return NULL;
    }
    return rec;
}

struct recorder_queue QUEUE(struct record_packet);

class Recorder
{
private:
    char* filename;
    enum sc_record_format format;
    AVFormatContext* ctx;
    struct size declared_frame_size;
    bool header_written;

    SDL_Thread* thread;
    SDL_mutex* mutex;
    SDL_cond* queue_cond;
    bool stopped; // set on recorder_stop() by the stream reader
    bool failed; // set on packet write failure
    struct recorder_queue queue;

    // we can write a packet only once we received the next one so that we can
    // set its duration (next_pts - current_pts)
    // "previous" is only accessed from the recorder thread, so it does not
    // need to be protected by the mutex
    struct record_packet* previous;
    const char* get_format_name() {
        switch (format) {
        case SC_RECORD_FORMAT_MP4: return "mp4";
        case SC_RECORD_FORMAT_MKV: return "matroska";
        default: return NULL;
        }
    }
    static const AVOutputFormat* find_muxer(const char* name) {
#ifdef SCRCPY_LAVF_HAS_NEW_MUXER_ITERATOR_API
        void* opaque = NULL;
#endif
        const AVOutputFormat* oformat = NULL;
        do {
#ifdef SCRCPY_LAVF_HAS_NEW_MUXER_ITERATOR_API
            oformat = av_muxer_iterate(&opaque);
#else
            oformat = av_oformat_next(oformat);
#endif
            // until null or with name "mp4"
        } while (oformat && strcmp(oformat->name, name));
        return oformat;
    }
    
    static void packet_delete(struct record_packet* rec) {
        av_packet_unref(&rec->packet);
        SDL_free(rec);
    }

    void queue_clear(struct recorder_queue* queue) {
        while (!queue_is_empty(queue)) {
            struct record_packet* rec;
            queue_take(queue, next, &rec);
            packet_delete(rec);
        }
    }

    void rescale_packet(AVPacket* packet) {
        AVStream* ostream = ctx->streams[0];
        av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, ostream->time_base);
    }

    bool write_header(const AVPacket* packet) {
        AVStream* ostream = ctx->streams[0];

        uint8_t* extradata = (uint8_t*)av_malloc(packet->size * sizeof(uint8_t));
        if (!extradata) {
            LOGC("Could not allocate extradata");
            return false;
        }

        // copy the first packet to the extra data
        memcpy(extradata, packet->data, packet->size);

#ifdef SCRCPY_LAVF_HAS_NEW_CODEC_PARAMS_API
        ostream->codecpar->extradata = extradata;
        ostream->codecpar->extradata_size = packet->size;
#else
        ostream->codec->extradata = extradata;
        ostream->codec->extradata_size = packet->size;
#endif

        int ret = avformat_write_header(ctx, NULL);
        if (ret < 0) {
            LOGE("Failed to write header to %s", filename);
            return false;
        }

        return true;
    }

    bool write(AVPacket* packet) {
        if (!header_written) {
            if (packet->pts != AV_NOPTS_VALUE) {
                LOGE("The first packet is not a config packet");
                return false;
            }
            bool ok = write_header(packet);
            if (!ok) {
                return false;
            }
            header_written = true;
            return true;
        }

        if (packet->pts == AV_NOPTS_VALUE) {
            // ignore config packets
            return true;
        }

        rescale_packet(packet);
        return av_write_frame(ctx, packet) >= 0;
    }


    static int run_recorder(void* data) {
        Recorder* recorder = (Recorder*)data;
        return recorder->run();
    }

    int run(){
        for (;;) {
            mutex_lock(mutex);

            while (!stopped && queue_is_empty(&queue)) {
                cond_wait(queue_cond, mutex);
            }

            // if stopped is set, continue to process the remaining events (to
            // finish the recording) before actually stopping

            if (stopped && queue_is_empty(&queue)) {
                mutex_unlock(mutex);
                struct record_packet* last = previous;
                if (last) {
                    // assign an arbitrary duration to the last packet
                    last->packet.duration = 100000;
                    bool ok = write(&last->packet);
                    if (!ok) {
                        // failing to write the last frame is not very serious, no
                        // future frame may depend on it, so the resulting file
                        // will still be valid
                        LOGW("Could not record last packet");
                    }
                    packet_delete(last);
                }
                break;
            }

            record_packet* rec;
            queue_take(&queue, next, &rec);

            mutex_unlock(mutex);

            // recorder->previous is only written from this thread, no need to lock
            record_packet* previous =  rec;

            if (!previous) {
                // we just received the first packet
                continue;
            }

            // config packets have no PTS, we must ignore them
            if (rec->packet.pts != AV_NOPTS_VALUE
                && previous->packet.pts != AV_NOPTS_VALUE) {
                // we now know the duration of the previous packet
                previous->packet.duration = rec->packet.pts - previous->packet.pts;
            }

            bool ok = write(&previous->packet);
            packet_delete(previous);
            if (!ok) {
                LOGE("Could not record packet");

                mutex_lock(mutex);
                failed = true;
                // discard pending packets
                queue_clear(&queue);
                mutex_unlock(mutex);
                break;
            }

        }

        LOGD("Recorder thread ended");

        return 0;
    }
public:

    bool init(const char* filename,
        enum sc_record_format format, struct size declared_frame_size) {
        this->filename = SDL_strdup(filename);
        if (!filename) {
            LOGE("Could not strdup filename");
            return false;
        }
        mutex = SDL_CreateMutex();
        if (!mutex) {
            LOGC("Could not create mutex");
            SDL_free(this->filename);
            return false;
        }
        queue_cond = SDL_CreateCond();
        if (!queue_cond) {
            LOGC("Could not create cond");
            SDL_DestroyMutex(mutex);
            SDL_free(this->filename);
            return false;
        }

        queue_init(&queue);
        stopped = false;
        failed = false;
        this->format = format;
        this->declared_frame_size = declared_frame_size;
        header_written = false;
        previous = NULL;

        return true;
    }

    void destroy() {
        SDL_DestroyCond(queue_cond);
        SDL_DestroyMutex(mutex);
        SDL_free(filename);
    }

    bool open(const AVCodec* input_codec) {
        const char* format_name = get_format_name();
        assert(format_name);
        const AVOutputFormat* format = find_muxer(format_name);
        if (!format) {
            LOGE("Could not find muxer");
            return false;
        }

        ctx = avformat_alloc_context();
        if (!ctx) {
            LOGE("Could not allocate output context");
            return false;
        }

        // contrary to the deprecated API (av_oformat_next()), av_muxer_iterate()
        // returns (on purpose) a pointer-to-const, but AVFormatContext.oformat
        // still expects a pointer-to-non-const (it has not be updated accordingly)
        // <https://github.com/FFmpeg/FFmpeg/commit/0694d8702421e7aff1340038559c438b61bb30dd>
        ctx->oformat = (AVOutputFormat*)format;

        av_dict_set(&ctx->metadata, "comment",
            "Recorded by scrcpy " , 0);

        AVStream* ostream = avformat_new_stream(ctx, input_codec);
        if (!ostream) {
            avformat_free_context(ctx);
            return false;
        }

#ifdef SCRCPY_LAVF_HAS_NEW_CODEC_PARAMS_API
        ostream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        ostream->codecpar->codec_id = input_codec->id;
        ostream->codecpar->format = AV_PIX_FMT_YUV420P;
        ostream->codecpar->width = declared_frame_size.width;
        ostream->codecpar->height = declared_frame_size.height;
#else
        ostream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
        ostream->codec->codec_id = input_codec->id;
        ostream->codec->pix_fmt = AV_PIX_FMT_YUV420P;
        ostream->codec->width = declared_frame_size.width;
        ostream->codec->height = declared_frame_size.height;
#endif

        int ret = avio_open(&ctx->pb, filename,
            AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("Failed to open output file: %s", filename);
            // ostream will be cleaned up during context cleaning
            avformat_free_context(ctx);
            return false;
        }

        LOGI("Recording started to %s file: %s", format_name, filename);

        return true;
    }

    void close() {
        if (header_written) {
            int ret = av_write_trailer(ctx);
            if (ret < 0) {
                LOGE("Failed to write trailer to %s", filename);
                failed = true;
            }
        }
        else {
            // the recorded file is empty
            failed = true;
        }
        avio_close(ctx->pb);
        avformat_free_context(ctx);

        if (failed) {
            LOGE("Recording failed to %s", filename);
        }
        else {
            const char* format_name = get_format_name();
            LOGI("Recording complete to %s file: %s", format_name, filename);
        }
    }

    bool start() {
        LOGD("Starting recorder thread");
        thread = SDL_CreateThread(run_recorder, "recorder", this);
        if (!thread) {
            LOGC("Could not start recorder thread");
            return false;
        }

        return true;
    }

    void stop(){
        mutex_lock(mutex);
        stopped = true;
        cond_signal(queue_cond);
        mutex_unlock(mutex);
    }

    void join() {
        SDL_WaitThread(thread, NULL);
    }

    bool push(const AVPacket* packet) {
        mutex_lock(mutex);
        assert(!stopped);

        if (failed) {
            // reject any new packet (this will stop the stream)
            return false;
        }

        struct record_packet* rec = record_packet_new(packet);
        if (!rec) {
            LOGC("Could not allocate record packet");
            return false;
        }

        queue_push(&queue, next, rec);
        cond_signal(queue_cond);

        mutex_unlock(mutex);
        return true;
    }
};

