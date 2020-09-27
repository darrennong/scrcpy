#pragma once
#include "ScrPlayer.h"
#include <stdbool.h>
#include <cassert>
#include <SDL2/SDL_stdinc.h>
#include "util/buffer_util.h"
#define DEVICE_MSG_MAX_SIZE (1 << 18) // 256k
// type: 1 byte; length: 4 bytes
#define DEVICE_MSG_TEXT_MAX_LENGTH (DEVICE_MSG_MAX_SIZE - 5)

enum device_msg_type {
    DEVICE_MSG_TYPE_CLIPBOARD,
};

struct device_msg {
    enum device_msg_type type;
    union {
        struct {
            char* text; // owned, to be freed by SDL_free()
        } clipboard;
    };
};
size_t device_msg_deserialize(const unsigned char* buf, size_t len,
    struct device_msg* msg) {
    if (len < 5) {
        // at least type + empty string length
        return 0; // not available
    }

    msg->type = (device_msg_type)buf[0];
    switch (msg->type) {
    case DEVICE_MSG_TYPE_CLIPBOARD: {
        size_t clipboard_len = buffer_read32be(&buf[1]);
        if (clipboard_len > len - 5) {
            return 0; // not available
        }
        char* text = (char*)SDL_malloc(clipboard_len + 1);
        if (!text) {
            LOGW("Could not allocate text for clipboard");
            return -1;
        }
        if (clipboard_len) {
            memcpy(text, &buf[5], clipboard_len);
        }
        text[clipboard_len] = '\0';

        msg->clipboard.text = text;
        return 5 + clipboard_len;
    }
    default:
        LOGW("Unknown device message type: %d", (int)msg->type);
        return -1; // error, we cannot recover
    }
}

void device_msg_destroy(struct device_msg* msg) {
    if (msg->type == DEVICE_MSG_TYPE_CLIPBOARD) {
        SDL_free(msg->clipboard.text);
    }
}

class Receiver
{
private:
    SOCKET socket;
    SDL_Thread* thread;
    SDL_mutex* mutex;

    void process_msg(device_msg* msg) {
        switch (msg->type) {
        case DEVICE_MSG_TYPE_CLIPBOARD: {
            char* current = SDL_GetClipboardText();
            bool same = current && !strcmp(current, msg->clipboard.text);
            SDL_free(current);
            if (same) {
                LOGD("Computer clipboard unchanged");
                return;
            }

            LOGI("Device clipboard copied");
            SDL_SetClipboardText(msg->clipboard.text);
            break;
        }
        }
    }

public:

    bool init(SOCKET control_socket) {
        if (!(mutex = SDL_CreateMutex())) {
            return false;
        }
        socket = control_socket;
        return true;
    }

    void destroy() {
        SDL_DestroyMutex(mutex);
    }

    size_t process_msgs(const unsigned char* buf, size_t len) {
        size_t head = 0;
        for (;;) {
            struct device_msg msg;
            size_t r = device_msg_deserialize(&buf[head], len - head, &msg);
            if (r == -1) {
                return -1;
            }
            if (r == 0) {
                return head;
            }

            process_msg(&msg);
            device_msg_destroy(&msg);

            head += r;
            assert(head <= len);
            if (head == len) {
                return head;
            }
        }
    }

    static int run_receiver(void* data) {
        Receiver* receiver = (Receiver*)data;
        return receiver->run();
    }
    int run(){
        static unsigned char buf[DEVICE_MSG_MAX_SIZE];
        size_t head = 0;

        for (;;) {
            assert(head < DEVICE_MSG_MAX_SIZE);
            size_t r = recv(socket, (char *)(buf + head),DEVICE_MSG_MAX_SIZE - head,0);
            if (r <= 0) {
                LOGD("Receiver stopped");
                break;
            }

            head += r;
            size_t consumed = process_msgs(buf, head);
            if (consumed == -1) {
                // an error occurred
                break;
            }

            if (consumed) {
                head -= consumed;
                // shift the remaining data in the buffer
                memmove(buf, &buf[consumed], head);
            }
        }
        LOGD("Receiver exited£¡");
        return 0;
    }

    bool start() {
        LOGD("Starting receiver thread");

        thread = SDL_CreateThread(run_receiver, "receiver", this);
        if (!thread) {
            LOGC("Could not start receiver thread");
            return false;
        }

        return true;
    }

    void join() {
        SDL_WaitThread(thread, NULL);
    }
};

