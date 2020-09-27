#pragma once
#include <stdbool.h>
#include <stdint.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <SDL2/SDL_atomic.h>
#include <SDL2/SDL_thread.h>
}
#include "FpsCounter.hpp"
#include "util/lock.h"

class VideoBuffer {
private:
    AVFrame* rendering_frame;
    SDL_mutex* mutex;
    bool render_expired_frames;
    bool interrupted;
    SDL_cond* rendering_frame_consumed_cond;
    bool renderingFrameConsumed;
    FpsCounter* fpsCounter;
    void swap_frames() {
        AVFrame* tmp = decoding_frame;
        decoding_frame = rendering_frame;
        rendering_frame = tmp;
    }
public:
    AVFrame* decoding_frame;
    void lock() {
        mutex_lock(mutex);
    }
    void unlock() {
        mutex_unlock(mutex);
    }
    bool init(FpsCounter* fps_counter, bool render_expired_frames) {
        fpsCounter = fps_counter;
        if (!(decoding_frame = av_frame_alloc())) {
            goto error_0;
        }
        if (!(rendering_frame = av_frame_alloc())) {
            goto error_1;
        }
        if (!(mutex = SDL_CreateMutex())) {
            goto error_2;
        }
        renderingFrameConsumed = render_expired_frames;
        if (renderingFrameConsumed) {
            if (!(rendering_frame_consumed_cond = SDL_CreateCond())) {
                SDL_DestroyMutex(mutex);
                goto error_2;
            }
            // interrupted is not used if expired frames are not rendered
            // since offering a frame will never block
            interrupted = false;
        }

        // there is initially no rendering frame, so consider it has already been
        // consumed
        renderingFrameConsumed = true;

        return true;

    error_2:
        av_frame_free(&rendering_frame);
    error_1:
        av_frame_free(&decoding_frame);
    error_0:
        return false;
    }

    void destroy() {
        if (render_expired_frames) {
            SDL_DestroyCond(rendering_frame_consumed_cond);
        }
        SDL_DestroyMutex(mutex);
        av_frame_free(&rendering_frame);
        av_frame_free(&decoding_frame);
    }

    // set the decoded frame as ready for rendering
    // this function locks frames->mutex during its execution
    // the output flag is set to report whether the previous frame has been skipped
    bool offer_decoded_frame() {
        bool previous_frame_skipped;
        lock();
        if (render_expired_frames) {
            // wait for the current (expired) frame to be consumed
            while (!renderingFrameConsumed && !interrupted) {
                SDL_CondWait(rendering_frame_consumed_cond, mutex);
            }
        }
        else if (!renderingFrameConsumed) {
            fpsCounter->add_skipped_frame();
        }
        swap_frames();
        previous_frame_skipped = !renderingFrameConsumed;
        renderingFrameConsumed = false;
        unlock();
        return previous_frame_skipped;
    }
    // mark the rendering frame as consumed and return it
    // MUST be called with frames->mutex locked!!!
    // the caller is expected to render the returned frame to some texture before
    // unlocking frames->mutex
    const AVFrame* consume_rendered_frame() {
        assert(!renderingFrameConsumed);
        renderingFrameConsumed = true;
        fpsCounter->add_rendered_frame();
        if (render_expired_frames) {
            // unblock video_buffer_offer_decoded_frame()
            SDL_CondSignal(rendering_frame_consumed_cond);
        }
        return rendering_frame;
    }

    // wake up and avoid any blocking call
    void interrupt() {
        if (render_expired_frames) {
            lock();
            interrupted = true;
            unlock();
            // wake up blocking wait
            SDL_CondSignal(rendering_frame_consumed_cond);
        }
    }
};