#pragma once
#include "ScrPlayer.h"
#include "util/buffer_util.h"
#include "util/str_util.h"
#include <cassert>
class ControlMsg
{
private:
	control_msg msg;

    void write_position(uint8_t* buf, position* position) {
        buffer_write32be(&buf[0], position->point.x);
        buffer_write32be(&buf[4], position->point.y);
        buffer_write16be(&buf[8], position->screen_size.width);
        buffer_write16be(&buf[10], position->screen_size.height);
    }

    // write length (2 bytes) + string (non nul-terminated)
    size_t write_string(const char* utf8, size_t max_len, unsigned char* buf) {
        size_t len = utf8_truncation_index(utf8, max_len);
        buffer_write32be(buf, len);
        memcpy(&buf[4], utf8, len);
        return 4 + len;
    }

    static uint16_t
        to_fixed_point_16(float f) {
        assert(f >= 0.0f && f <= 1.0f);
        uint32_t u = f * 0x1p16f; // 2^16
        if (u >= 0xffff) {
            u = 0xffff;
        }
        return (uint16_t)u;
    }

public:
    control_msg* getMsg() {
        return &msg;
    }
    // buf size must be at least CONTROL_MSG_MAX_SIZE
    // return the number of bytes written
    size_t serialize(unsigned char* buf) {
        buf[0] = msg.type;
        switch (msg.type) {
        case CONTROL_MSG_TYPE_INJECT_KEYCODE:
            buf[1] = msg.inject_keycode.action;
            buffer_write32be(&buf[2], msg.inject_keycode.keycode);
            buffer_write32be(&buf[6], msg.inject_keycode.repeat);
            buffer_write32be(&buf[10], msg.inject_keycode.metastate);
            return 14;
        case CONTROL_MSG_TYPE_INJECT_TEXT: {
            size_t len =
                write_string(msg.inject_text.text,
                    CONTROL_MSG_INJECT_TEXT_MAX_LENGTH, &buf[1]);
            return 1 + len;
        }
        case CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT:
            buf[1] = msg.inject_touch_event.action;
            buffer_write64be(&buf[2], msg.inject_touch_event.pointer_id);
            write_position(&buf[10], &msg.inject_touch_event.position);
            buffer_write16be(&buf[22], to_fixed_point_16(msg.inject_touch_event.pressure));
            buffer_write32be(&buf[24], msg.inject_touch_event.buttons);
            return 28;
        case CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT:
            write_position(&buf[1], &msg.inject_scroll_event.position);
            buffer_write32be(&buf[13],
                (uint32_t)msg.inject_scroll_event.hscroll);
            buffer_write32be(&buf[17],
                (uint32_t)msg.inject_scroll_event.vscroll);
            return 21;
        case CONTROL_MSG_TYPE_SET_CLIPBOARD: {
            buf[1] = !!msg.set_clipboard.paste;
            size_t len = write_string(msg.set_clipboard.text,
                CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH,
                &buf[2]);
            return 2 + len;
        }
        case CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE:
            buf[1] = msg.set_screen_power_mode.mode;
            return 2;
        case CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON:
        case CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL:
        case CONTROL_MSG_TYPE_COLLAPSE_NOTIFICATION_PANEL:
        case CONTROL_MSG_TYPE_GET_CLIPBOARD:
        case CONTROL_MSG_TYPE_ROTATE_DEVICE:
            // no additional data
            return 1;
        default:
            LOGW("Unknown message type: %u", (unsigned)msg.type);
            return 0;
        }
    }

    void destroyMsg() {
        switch (msg.type) {
        case CONTROL_MSG_TYPE_INJECT_TEXT:
            SDL_free(msg.inject_text.text);
            break;
        case CONTROL_MSG_TYPE_SET_CLIPBOARD:
            SDL_free(msg.set_clipboard.text);
            break;
        default:
            // do nothing
            break;
        }
    }
};

