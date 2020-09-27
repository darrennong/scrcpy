#pragma once
#include "ScrPlayer.h"
#include <stdbool.h>
#include <WinSock2.h>
#include "util/cbuf.h"
#include "util/log.h"
#include "command.h"
#include "util/lock.h"
#include <cassert>
#define DEFAULT_PUSH_TARGET "/sdcard/"
# define PROCESS_NONE NULL
typedef enum {
	ACTION_INSTALL_APK,
	ACTION_PUSH_FILE,
} file_handler_action_t;

typedef struct file_handler_request {
	file_handler_action_t action;
	char* file;
}file_handler_request_t;
typedef HANDLE process_t;
typedef struct file_handler_request_queue CBUF(file_handler_request, 16)file_handler_req_queue;

static bool is_apk(const char* file) {
    const char* ext = strrchr(file, '.');
    return ext && !strcmp(ext, ".apk");
}

class FileHandler
{
private:
    char* serial;
    const char* push_target;
    SDL_Thread* thread;
    SDL_mutex* mutex;
    SDL_cond* event_cond;
    bool stopped;
    bool initialized;
    process_t current_process;
    file_handler_req_queue queue;

    void file_handler_request_destroy(struct file_handler_request* req) {
        SDL_free(req->file);
    }

    static process_t
        install_apk(const char* serial, const char* file) {
        return adb_install(serial, file);
    }

    static process_t
        push_file(const char* serial, const char* file, const char* push_target) {
        return adb_push(serial, file, push_target);
    }

    bool
        file_handler_request(file_handler_action_t action, char* file) {
        // start file_handler if it's used for the first time
        if (!initialized) {
            if (!start()) {
                return false;
            }
            initialized = true;
        }

        LOGI("Request to %s %s", action == ACTION_INSTALL_APK ? "install" : "push",
            file);
        file_handler_request_t req;
        req.action = action;
        req.file = file;

        mutex_lock(mutex);
        bool was_empty = cbuf_is_empty(&queue);
        bool res = !cbuf_is_full(&queue);
        if (res) cbuf_push(&queue, req);
        if (was_empty) {
            cond_signal(event_cond);
        }
        mutex_unlock(mutex);
        return res;
    }

    static int run_file_handler(void* data) {
        FileHandler* file_handler = (FileHandler*)data;
        return file_handler->run();
    }

    int run() {
        for (;;) {
            mutex_lock(mutex);
            current_process = PROCESS_NONE;
            while (!stopped && cbuf_is_empty(&queue)) {
                cond_wait(event_cond, mutex);
            }
            if (stopped) {
                // stop immediately, do not process further events
                mutex_unlock(mutex);
                break;
            }
            struct file_handler_request req;
            bool non_empty = !cbuf_is_empty(&queue);
            if (non_empty) {
                cbuf_take(&queue, &req);
            }
            assert(non_empty);
            (void)non_empty;

            process_t process;
            if (req.action == ACTION_INSTALL_APK) {
                LOGI("Installing %s...", req.file);
                process = install_apk(serial, req.file);
            }
            else {
                LOGI("Pushing %s...", req.file);
                process = push_file(serial, req.file,
                    push_target);
            }
            current_process = process;
            mutex_unlock(mutex);

            if (req.action == ACTION_INSTALL_APK) {
                if (process_check_success(process, "adb install")) {
                    LOGI("%s successfully installed", req.file);
                }
                else {
                    LOGE("Failed to install %s", req.file);
                }
            }
            else {
                if (process_check_success(process, "adb push")) {
                    LOGI("%s successfully pushed to %s", req.file,
                        push_target);
                }
                else {
                    LOGE("Failed to push %s to %s", req.file,
                        push_target);
                }
            }

            file_handler_request_destroy(&req);
        }
        return 0;
    }


public:
    bool init(const char* serial, const char* push_target) {
        cbuf_init(&queue);

        if (!(mutex = SDL_CreateMutex())) {
            return false;
        }

        if (!(event_cond = SDL_CreateCond())) {
            SDL_DestroyMutex(mutex);
            return false;
        }

        if (serial) {
            serial = SDL_strdup(serial);
            if (!serial) {
                LOGW("Could not strdup serial");
                SDL_DestroyCond(event_cond);
                SDL_DestroyMutex(mutex);
                return false;
            }
        }
        else {
            serial = NULL;
        }

        // lazy initialization
        initialized = false;

        stopped = false;
        current_process = PROCESS_NONE;

        push_target = push_target ? push_target : DEFAULT_PUSH_TARGET;

        return true;
    }

    void destroy() {
        SDL_DestroyCond(event_cond);
        SDL_DestroyMutex(mutex);
        SDL_free(serial);

        struct file_handler_request req;
        while (!cbuf_is_empty(&queue)) {
            cbuf_take(&queue, &req)
                file_handler_request_destroy(&req);
        }
    }

    bool start() {
        LOGD("Starting file_handler thread");

        thread = SDL_CreateThread(run_file_handler, "file_handler", this);
        if (!thread) {
            LOGC("Could not start file_handler thread");
            return false;
        }

        return true;
    }

    void stop() {
        mutex_lock(mutex);
        stopped = true;
        cond_signal(event_cond);
        if (current_process != PROCESS_NONE) {
            if (!cmd_terminate(current_process)) {
                LOGW("Could not terminate install process");
            }
            cmd_simple_wait(current_process, NULL);
            current_process = PROCESS_NONE;
        }
        mutex_unlock(mutex);
    }

    void join() {
        SDL_WaitThread(thread, NULL);
    }

    // take ownership of file, and will SDL_free() it
    bool request(file_handler_action_t action, char* file) {
        // start file_handler if it's used for the first time
        if (!initialized) {
            if (!start()) {
                return false;
            }
            initialized = true;
        }

        LOGI("Request to %s %s", action == ACTION_INSTALL_APK ? "install" : "push",
            file);
        file_handler_request_t req;
        req.action = action;
        req.file = file;

        mutex_lock(mutex);
        bool was_empty = cbuf_is_empty(&queue);
        bool res = !cbuf_is_full(&queue);
        if (res) cbuf_push(&queue, req);
        if (was_empty) {
            cond_signal(event_cond);
        }
        mutex_unlock(mutex);
        return res;
    }
};