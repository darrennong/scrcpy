#pragma once
#include "ScrPlayer.h"
#include <stdbool.h>
#include "Receiver.hpp"
#include "ControlMsg.hpp"
typedef struct control_msg_queue CBUF(struct control_msg, 64)control_msg_queue;

class Controller
{
private:
	SOCKET socket;
	SDL_Thread* thread = nullptr;
	SDL_mutex* mutex = nullptr;
	SDL_cond* msg_cond = nullptr;
	bool stopped;
	control_msg_queue queue;
	Receiver receiver;
    ControlMsg cmsg;
public:
    bool initialized = false;
	bool init(SOCKET control_socket) {
		this->socket = control_socket;
		cbuf_init(&queue);
		if (!receiver.init(socket)) {
			return false;
		}

		if (!(mutex = SDL_CreateMutex())) {
			receiver.destroy();
			return false;
		}

		if (!(msg_cond = SDL_CreateCond())) {
			receiver.destroy();
			SDL_DestroyMutex(mutex);
			return false;
		}

		control_socket = control_socket;
		stopped = false;
        initialized = true;
		return true;
	}

	void destroy() {
		SDL_DestroyCond(msg_cond);
		SDL_DestroyMutex(mutex);

		while (!cbuf_is_empty(&queue)) {
			cbuf_take(&queue, cmsg.getMsg());
			cmsg.destroyMsg();
		}

		receiver.destroy();
	}

    bool push_msg(const struct control_msg* msg) {
        mutex_lock(mutex);
        bool was_empty = cbuf_is_empty(&queue);
        bool res = !cbuf_is_full(&queue);
        if (res) {
            cbuf_push(&queue, *msg);
        }
        if (was_empty) {
            cond_signal(msg_cond);
        }
        mutex_unlock(mutex);
        return res;
    }
    size_t net_send_all(SOCKET socket, const char* buf, size_t len) {
        size_t w = 0;
        while (len > 0) {
            w = send(socket, buf, len, 0);
            if (w == -1) {
                return -1;
            }
            len -= w;
            buf = (char*)buf + w;
        }
        return w;
    }


   bool process_msg() {
        static unsigned char serialized_msg[CONTROL_MSG_MAX_SIZE];
        int length = cmsg.serialize(serialized_msg);
        if (!length) {
            return false;
        }
        int w = net_send_all(socket, (char*)serialized_msg, length);
        return w == length;
       return true;
    }

    static int run_controller(void* data) {
        Controller* controller = (Controller*)data;
        return controller->run();
    }
    int run(){
        for (;;) {
            mutex_lock(mutex);
            while (!stopped && cbuf_is_empty(&queue)) {
                cond_wait(msg_cond, mutex);
            }
            if (stopped) {
                // stop immediately, do not process further msgs
                mutex_unlock(mutex);
                break;
            }
            bool non_empty = !cbuf_is_empty(&queue);
            if (non_empty) {
                cbuf_take(&queue, cmsg.getMsg());
            }
            assert(non_empty);
            (void)non_empty;
            mutex_unlock(mutex);

            bool ok = process_msg();
            cmsg.destroyMsg();
            if (!ok) {
                LOGD("Could not write msg to socket");
                break;
            }
        }
        LOGE("Controller exited");
        return 0;
    }

	bool start() {
        LOGD("Starting controller thread");
        thread = SDL_CreateThread(run_controller, "controller",this);
        if (!thread) {
            LOGC("Could not start controller thread");
            return false;
        }
        if (!receiver.start()) {
            stop();
            SDL_WaitThread(thread, NULL);
            return false;
        }
        return true;
	}

    void stop() {
        mutex_lock(mutex);
        stopped = true;
        cond_signal(msg_cond);
        mutex_unlock(mutex);
    }

    void join() {
        SDL_WaitThread(thread, NULL);
        receiver.join();
    }
};

