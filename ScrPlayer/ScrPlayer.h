#pragma once
#include "Winsock2.h"
#include <windows.h>
#include "resource.h"

extern "C" {
#include <SDL2/SDL.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
}

#include "targetver.h"
//#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include "common.h"
#include "util/log.h"
#include "util/lock.h"
#include "util/cbuf.h"

BOOL WINAPI windows_ctrl_handler(DWORD ctrl_type);

#define DEVICE_NAME_FIELD_LENGTH 64
#define SC_MAX_SHORTCUT_MODS 8
#define BUFSIZE 0x10000
#define HEADER_SIZE 12
#define NO_PTS UINT64_C(0xffffffffffffffff)
#define CONTROL_MSG_MAX_SIZE (1 << 18) // 256k

#define CONTROL_MSG_INJECT_TEXT_MAX_LENGTH 300
// type: 1 byte; paste flag: 1 byte; length: 4 bytes
#define CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH (CONTROL_MSG_MAX_SIZE - 6)

#define POINTER_ID_MOUSE UINT64_C(0xFFFFFFFFFFFFFFFF);
#define POINTER_ID_VIRTUAL_FINGER UINT64_C(0xFFFFFFFFFFFFFFFE);

enum control_msg_type {
    CONTROL_MSG_TYPE_INJECT_KEYCODE,
    CONTROL_MSG_TYPE_INJECT_TEXT,
    CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
    CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
    CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON,
    CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    CONTROL_MSG_TYPE_COLLAPSE_NOTIFICATION_PANEL,
    CONTROL_MSG_TYPE_GET_CLIPBOARD,
    CONTROL_MSG_TYPE_SET_CLIPBOARD,
    CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE,
    CONTROL_MSG_TYPE_ROTATE_DEVICE,
};

enum screen_power_mode {
    // see <https://android.googlesource.com/platform/frameworks/base.git/+/pie-release-2/core/java/android/view/SurfaceControl.java#305>
    SCREEN_POWER_MODE_OFF = 0,
    SCREEN_POWER_MODE_NORMAL = 2,
};

typedef struct control_msg {
    enum control_msg_type type;
    union {
        struct {
            enum android_keyevent_action action;
            enum android_keycode keycode;
            uint32_t repeat;
            enum android_metastate metastate;
        } inject_keycode;
        struct {
            char* text; // owned, to be freed by SDL_free()
        } inject_text;
        struct {
            enum android_motionevent_action action;
            enum android_motionevent_buttons buttons;
            uint64_t pointer_id;
            struct position position;
            float pressure;
        } inject_touch_event;
        struct {
            struct position position;
            int32_t hscroll;
            int32_t vscroll;
        } inject_scroll_event;
        struct {
            char* text; // owned, to be freed by SDL_free()
            bool paste;
        } set_clipboard;
        struct {
            enum screen_power_mode mode;
        } set_screen_power_mode;
    };
}control_msg;

struct sc_shortcut_mods {
    unsigned data[SC_MAX_SHORTCUT_MODS];
    unsigned count;
};

struct sc_port_range {
    uint16_t first;
    uint16_t last;
};

static const int ACTION_DOWN = 1;
static const int ACTION_UP = 1 << 1;

enum sc_shortcut_mod {
    SC_MOD_LCTRL = 1 << 0,
    SC_MOD_RCTRL = 1 << 1,
    SC_MOD_LALT = 1 << 2,
    SC_MOD_RALT = 1 << 3,
    SC_MOD_LSUPER = 1 << 4,
    SC_MOD_RSUPER = 1 << 5,
};
enum sc_record_format {
    SC_RECORD_FORMAT_AUTO,
    SC_RECORD_FORMAT_MP4,
    SC_RECORD_FORMAT_MKV,
};

enum event_result {
    EVENT_RESULT_CONTINUE,
    EVENT_RESULT_STOPPED_BY_USER,
    EVENT_RESULT_STOPPED_BY_EOS,
};