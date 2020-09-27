#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
#include <SDL2\SDL_timer.h>
#include <cassert>
#include <WinBase.h>
#define FPS_COUNTER_INTERVAL_MS 1000
class FpsCounter
{
private:
    SDL_Thread* thread;
    SDL_mutex* mutex;
    SDL_cond* state_cond;

    // atomic so that we can check without locking the mutex
    // if the FPS counter is disabled, we don't want to lock unnecessarily
    unsigned long long volatile started;

    // the following fields are protected by the mutex
    bool interrupted;
    unsigned nr_rendered;
    unsigned nr_skipped;
    uint32_t next_timestamp;
public:

    bool init() {
        mutex = SDL_CreateMutex();
        if (!mutex) {
            return false;
        }
        state_cond = SDL_CreateCond();
        if (!state_cond) {
            SDL_DestroyMutex(mutex);
            return false;
        }
        thread = NULL;
        InterlockedAnd(&started, 0);
        // no need to initialize the other fields, they are unused until started

        return true;
    }

    bool isStarted() {
        return started;
    }

    bool setStarted(bool val) {
        if (val) {
            InterlockedOr(&started, 1);
        }
        else {
            InterlockedAnd(&started, 0);
        }
        return started;
    }

    void display() {
        unsigned rendered_per_second = nr_rendered * 1000 / FPS_COUNTER_INTERVAL_MS;
        if (nr_skipped) {
            printf("%u fps (+%u frames skipped)", rendered_per_second, nr_skipped);
        }
        else {
            printf("%u fps", rendered_per_second);
        }
    }

    void check_interval_expired(uint32_t now) {
        if (now < next_timestamp) {
            return;
        }
        display();
        nr_rendered = 0;
        nr_skipped = 0;
        // add a multiple of the interval
        uint32_t elapsed_slices = (now - next_timestamp) / FPS_COUNTER_INTERVAL_MS + 1;
        next_timestamp += FPS_COUNTER_INTERVAL_MS * elapsed_slices;
    }

    int run() {
        SDL_LockMutex(mutex);
        while (!interrupted) {
            while (!interrupted && !isStarted()) {
                SDL_CondWait(state_cond, mutex);
            }
            while (!interrupted) {
                uint32_t now = SDL_GetTicks();
                check_interval_expired(now);

                assert(next_timestamp > now);
                uint32_t remaining = next_timestamp - now;

                // ignore the reason (timeout or signaled), we just loop anyway
                SDL_CondWaitTimeout(state_cond, mutex, remaining);
            }
        }
        SDL_UnlockMutex(mutex);
        return 0;
    }

    static int run_fps_counter(void* data) {
        FpsCounter* counter = (FpsCounter*)data;
        return counter->run();
    }

    bool start() {
        SDL_LockMutex(mutex);
        next_timestamp = SDL_GetTicks() + FPS_COUNTER_INTERVAL_MS;
        nr_rendered = 0;
        nr_skipped = 0;
        SDL_UnlockMutex(mutex);

        setStarted(true);
        SDL_CondSignal(state_cond);

        // counter->thread is always accessed from the same thread, no need to lock
        if (!thread) {
            thread = SDL_CreateThread(run_fps_counter, "fps counter", this);
            if (!thread) printf("Could not start FPS counter thread");
            return false;
        }
        return true;
    }

    void stop() {
        setStarted(false);
        SDL_CondSignal(state_cond);
    }

    void interrupt() {
        if (!thread) {
            return;
        }

        SDL_LockMutex(mutex);
        interrupted = true;
        SDL_UnlockMutex(mutex);
        // wake up blocking wait
        SDL_CondSignal(state_cond);
    }


    void join() {
        if (thread) {
            SDL_WaitThread(thread, NULL);
        }
    }

    void add_rendered_frame() {
        if (!isStarted()) {
            return;
        }
        SDL_LockMutex(mutex);
        uint32_t now = SDL_GetTicks();
        check_interval_expired(now);
       InterlockedIncrement(&nr_rendered);
       SDL_UnlockMutex(mutex);
    }

    void add_skipped_frame() {
        if (!isStarted()) {
            return;
        }
        SDL_LockMutex(mutex);
        uint32_t now = SDL_GetTicks();
        check_interval_expired(now);
        InterlockedIncrement(&nr_skipped);
        SDL_UnlockMutex(mutex);
    }
    void switch_fps_counter_state() {
        // the started state can only be written from the current thread, so there
        // is no ToCToU issue
        if (started) {
            stop();
            LOGI("FPS counter stopped");
        }
        else {
            if (start()) {
                LOGI("FPS counter started");
            }
            else {
                LOGE("FPS counter starting failed");
            }
        }
    }
};