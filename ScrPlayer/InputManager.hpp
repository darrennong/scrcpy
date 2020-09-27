#pragma once
#include "ScrPlayer.h"
#include "common.h"
#include "event_converter.h"
#include "Controller.hpp"
#include "VideoBuffer.hpp"
#include "Screen.hpp"
#include "android/input.h"
#include "android/keycodes.h"

#define SC_SDL_SHORTCUT_MODS_MASK (KMOD_CTRL | KMOD_ALT | KMOD_GUI)

static inline uint16_t
to_sdl_mod(unsigned mod) {
    uint16_t sdl_mod = 0;
    if (mod & SC_MOD_LCTRL) {
        sdl_mod |= KMOD_LCTRL;
    }
    if (mod & SC_MOD_RCTRL) {
        sdl_mod |= KMOD_RCTRL;
    }
    if (mod & SC_MOD_LALT) {
        sdl_mod |= KMOD_LALT;
    }
    if (mod & SC_MOD_RALT) {
        sdl_mod |= KMOD_RALT;
    }
    if (mod & SC_MOD_LSUPER) {
        sdl_mod |= KMOD_LGUI;
    }
    if (mod & SC_MOD_RSUPER) {
        sdl_mod |= KMOD_RGUI;
    }
    return sdl_mod;
}

class InputManager
{
private:
    Controller* controller;
    VideoBuffer* video_buffer;
    FpsCounter* fps_counter;
    Screen* screen;

    // SDL reports repeated events as a boolean, but Android expects the actual
    // number of repetitions. This variable keeps track of the count.
    unsigned repeat;

    bool control;
    bool forward_key_repeat;
    bool prefer_text;

    struct {
        unsigned data[SC_MAX_SHORTCUT_MODS];
        unsigned count;
    } sdl_shortcut_mods;

    bool vfinger_down;

    bool is_shortcut_mod(uint16_t sdl_mod) {
        // keep only the relevant modifier keys
        sdl_mod &= SC_SDL_SHORTCUT_MODS_MASK;

        assert(sdl_shortcut_mods.count);
        assert(sdl_shortcut_mods.count < SC_MAX_SHORTCUT_MODS);
        for (unsigned i = 0; i < sdl_shortcut_mods.count; ++i) {
            if (sdl_shortcut_mods.data[i] == sdl_mod) {
                return true;
            }
        }

        return false;
    }
public:

    void init(Controller* controller,VideoBuffer* video_buffer,FpsCounter* fps_counter,Screen* screen) {
        this->controller = controller;
        this->video_buffer = video_buffer;
        this->fps_counter = fps_counter;
        this->screen = screen;
        control = true;
        forward_key_repeat = true;
        prefer_text = true;
        sdl_shortcut_mods.count = 3;
        sdl_shortcut_mods.data[0] = SC_MOD_LCTRL;
        sdl_shortcut_mods.data[1] = SC_MOD_LALT;
        sdl_shortcut_mods.data[2] = SC_MOD_LSUPER;
        vfinger_down = false;
    }


    void send_keycode(enum android_keycode keycode, int actions, const char* name) {
        // send DOWN event
        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_INJECT_KEYCODE;
        msg.inject_keycode.keycode = keycode;
        msg.inject_keycode.metastate = AMETA_NONE;
        msg.inject_keycode.repeat = 0;

        if (actions & ACTION_DOWN) {
            msg.inject_keycode.action = AKEY_EVENT_ACTION_DOWN;
            if (!controller->push_msg(&msg)) {
                LOGW("Could not request 'inject %s (DOWN)'", name);
                return;
            }
        }

        if (actions & ACTION_UP) {
            msg.inject_keycode.action = AKEY_EVENT_ACTION_UP;
            if (!controller->push_msg(&msg)) {
                LOGW("Could not request 'inject %s (UP)'", name);
            }
        }
    }

    inline void action_home(int actions) {
        send_keycode( AKEYCODE_HOME, actions, "HOME");
    }

    inline void action_back(int actions) {
        send_keycode(AKEYCODE_BACK, actions, "BACK");
    }

    inline void action_app_switch(int actions) {
        send_keycode(AKEYCODE_APP_SWITCH, actions, "APP_SWITCH");
    }

    inline void action_power(int actions) {
        send_keycode(AKEYCODE_POWER, actions, "POWER");
    }

    inline void action_volume_up(int actions) {
        send_keycode(AKEYCODE_VOLUME_UP, actions, "VOLUME_UP");
    }

    inline void action_volume_down(int actions) {
        send_keycode(AKEYCODE_VOLUME_DOWN, actions, "VOLUME_DOWN");
    }

    inline void action_menu(int actions) {
        send_keycode(AKEYCODE_MENU, actions, "MENU");
    }

    inline void action_copy(int actions) {
        send_keycode(AKEYCODE_COPY, actions, "COPY");
    }

    inline void action_cut(int actions) {
        send_keycode(AKEYCODE_CUT, actions, "CUT");
    }

    // turn the screen on if it was off, press BACK otherwise
    void press_back_or_turn_screen_on() {
        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON;

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request 'press back or turn screen on'");
        }
    }

    void expand_notification_panel() {
        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL;

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request 'expand notification panel'");
        }
    }

    void collapse_notification_panel() {
        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_COLLAPSE_NOTIFICATION_PANEL;

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request 'collapse notification panel'");
        }
    }

    void set_device_clipboard(bool paste) {
        char* text = SDL_GetClipboardText();
        if (!text) {
            LOGW("Could not get clipboard text: %s", SDL_GetError());
            return;
        }
        if (!*text) {
            // empty text
            SDL_free(text);
            return;
        }

        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_SET_CLIPBOARD;
        msg.set_clipboard.text = text;
        msg.set_clipboard.paste = paste;

        if (!controller->push_msg(&msg)) {
            SDL_free(text);
            LOGW("Could not request 'set device clipboard'");
        }
    }

    void set_screen_power_mode(enum screen_power_mode mode) {
        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE;
        msg.set_screen_power_mode.mode = mode;

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request 'set screen power mode'");
        }
    }

    void switch_fps_counter_state() {
        // the started state can only be written from the current thread, so there
        // is no ToCToU issue
        if (fps_counter->isStarted()) {
            fps_counter->stop();
            LOGI("FPS counter stopped");
        }
        else {
            if (fps_counter->start()) {
                LOGI("FPS counter started");
            }
            else {
                LOGE("FPS counter starting failed");
            }
        }
    }

    void clipboard_paste() {
        char* text = SDL_GetClipboardText();
        if (!text) {
            LOGW("Could not get clipboard text: %s", SDL_GetError());
            return;
        }
        if (!*text) {
            // empty text
            SDL_free(text);
            return;
        }

        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_INJECT_TEXT;
        msg.inject_text.text = text;
        if (!controller->push_msg(&msg)) {
            SDL_free(text);
            LOGW("Could not request 'paste clipboard'");
        }
    }

    void rotate_device() {
        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_ROTATE_DEVICE;

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request device rotation");
        }
    }

    void process_text_input(const SDL_TextInputEvent* event) {
        if (is_shortcut_mod(SDL_GetModState())) {
            // A shortcut must never generate text events
            return;
        }
        if (!prefer_text) {
            char c = event->text[0];
            if (isalpha(c) || c == ' ') {
                assert(event->text[1] == '\0');
                // letters and space are handled as raw key event
                return;
            }
        }

        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_INJECT_TEXT;
        msg.inject_text.text = SDL_strdup(event->text);
        if (!msg.inject_text.text) {
            LOGW("Could not strdup input text");
            return;
        }
        if (!controller->push_msg(&msg)) {
            SDL_free(msg.inject_text.text);
            LOGW("Could not request 'inject text'");
        }
    }

    bool simulate_virtual_finger(enum android_motionevent_action action, struct point point) {
        bool up = action == AMOTION_EVENT_ACTION_UP;

        struct control_msg msg;
        msg.type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
        msg.inject_touch_event.action = action;
        msg.inject_touch_event.position.screen_size = screen->frame_size;
        msg.inject_touch_event.position.point = point;
        msg.inject_touch_event.pointer_id = POINTER_ID_VIRTUAL_FINGER;
        msg.inject_touch_event.pressure = up ? 0.0f : 1.0f;
        msg.inject_touch_event.buttons = (android_motionevent_buttons)0;

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request 'inject virtual finger event'");
            return false;
        }

        return true;
    }

    static bool convert_input_key(const SDL_KeyboardEvent* from, struct control_msg* to,
            bool prefer_text, uint32_t repeat) {
        to->type = CONTROL_MSG_TYPE_INJECT_KEYCODE;

        if (!convert_keycode_action((SDL_EventType)from->type, &to->inject_keycode.action)) {
            return false;
        }

        uint16_t mod = from->keysym.mod;
        if (!convert_keycode(from->keysym.sym, &to->inject_keycode.keycode, mod,
            prefer_text)) {
            return false;
        }

        to->inject_keycode.repeat = repeat;
        to->inject_keycode.metastate = convert_meta_state((SDL_Keymod)mod);

        return true;
    }

    void process_key(const SDL_KeyboardEvent* event) {
        // control: indicates the state of the command-line option --no-control

        bool smod = is_shortcut_mod(event->keysym.mod);

        SDL_Keycode keycode = event->keysym.sym;
        bool down = event->type == SDL_KEYDOWN;
        bool ctrl = event->keysym.mod & KMOD_CTRL;
        bool shift = event->keysym.mod & KMOD_SHIFT;
        bool evtRepeat = event->repeat;

        // The shortcut modifier is pressed //Debug if(smod)
        if (ctrl) {
            int action = down ? ACTION_DOWN : ACTION_UP;
            switch (keycode) {
            case SDLK_h:
                if (control && !shift && !evtRepeat) {
                    action_home(action);
                }
                return;
            case SDLK_b: // fall-through
            case SDLK_BACKSPACE:
                if (control && !shift && !evtRepeat) {
                    action_back(action);
                }
                return;
            case SDLK_s:
                if (control && !shift && !evtRepeat) {
                    action_app_switch(action);
                }
                return;
            case SDLK_m:
                if (control && !shift && !evtRepeat) {
                    action_menu(action);
                }
                return;
            case SDLK_p:
                if (control && !shift && !evtRepeat) {
                    action_power(action);
                }
                return;
            case SDLK_o:
                if (control && !evtRepeat && down) {
                    enum screen_power_mode mode = shift
                        ? SCREEN_POWER_MODE_NORMAL
                        : SCREEN_POWER_MODE_OFF;
                    set_screen_power_mode(mode);
                }
                return;
            case SDLK_DOWN:
                if (control && !shift) {
                    // forward repeated events
                    action_volume_down(action);
                }
                return;
            case SDLK_UP:
                if (control && !shift) {
                    // forward repeated events
                    action_volume_up(action);
                }
                return;
            case SDLK_LEFT:
                if (!shift && !evtRepeat && down) {
                    screen->rotate_client_left();
                }
                return;
            case SDLK_RIGHT:
                if (!shift && !evtRepeat && down) {
                    screen->rotate_client_right();
                }
                return;
            case SDLK_c:
                if (control && !shift && !evtRepeat) {
                    action_copy(action);
                }
                return;
            case SDLK_x:
                if (control && !shift && !evtRepeat) {
                    action_cut(action);
                }
                return;
            case SDLK_v:
                if (control && !evtRepeat && down) {
                    if (shift) {
                        // inject the text as input events
                        clipboard_paste();
                    }
                    else {
                        // store the text in the device clipboard and paste
                        set_device_clipboard(true);
                    }
                }
                return;
            case SDLK_f:
                if (!shift && !evtRepeat && down) {
                    screen->switch_fullscreen();
                }
                return;
            case SDLK_w:
                if (!shift && !evtRepeat && down) {
                    screen->resize_to_fit();
                }
                return;
            case SDLK_g:
                if (!shift && !evtRepeat && down) {
                    screen->resize_to_pixel_perfect();
                }
                return;
            case SDLK_i:
                if (!shift && !evtRepeat && down) {
                    fps_counter->switch_fps_counter_state();
                }
                return;
            case SDLK_n:
                if (control && !evtRepeat && down) {
                    if (shift) {
                        collapse_notification_panel();
                    }
                    else {
                        expand_notification_panel();
                    }
                }
                return;
            case SDLK_r:
                if (control && !shift && !evtRepeat && down) {
                    rotate_device();
                }
                return;
            }

            return;
        }

        if (!control) {
            return;
        }

        if (evtRepeat) {
            if (!forward_key_repeat) {
                return;
            }
            ++repeat;
        }
        else {
            repeat = 0;
        }

        if (ctrl && !shift && keycode == SDLK_v && down && !evtRepeat) {
            // Synchronize the computer clipboard to the device clipboard before
            // sending Ctrl+v, to allow seamless copy-paste.
            set_device_clipboard(false);
        }

        struct control_msg msg;
        if (convert_input_key(event, &msg, prefer_text, repeat)) {
            if (!controller->push_msg(&msg)) {
                LOGW("Could not request 'inject keycode'");
            }
        }
    }

    bool convert_mouse_motion(const SDL_MouseMotionEvent* from,struct control_msg* to) {
        to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
        to->inject_touch_event.action = AMOTION_EVENT_ACTION_MOVE;
        to->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
        to->inject_touch_event.position.screen_size = screen->frame_size;
        to->inject_touch_event.position.point = screen->convert_window_to_frame_coords(from->x, from->y);
        to->inject_touch_event.pressure = 1.f;
        to->inject_touch_event.buttons = convert_mouse_buttons(from->state);
        return true;
    }

    void process_mouse_motion(const SDL_MouseMotionEvent* event) {
        screen->getHotButton(event->x, event->y);
        if (!event->state) {
            // do not send motion events when no button is pressed
            return;
        }
        if (event->which == SDL_TOUCH_MOUSEID) {
            // simulated from touch events, so it's a duplicate
            return;
        }
        struct control_msg msg;
        if (!convert_mouse_motion(event, &msg)) {
            return;
        }

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request 'inject mouse motion event'");
        }

        if (vfinger_down) {
            struct point mouse = msg.inject_touch_event.position.point;
            struct point vfinger = inverse_point(mouse, screen->frame_size);
            simulate_virtual_finger(AMOTION_EVENT_ACTION_MOVE, vfinger);
        }
    }

    bool convert_touch(const SDL_TouchFingerEvent* from, struct control_msg* to) {
        to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

        if (!convert_touch_action((SDL_EventType)from->type, &to->inject_touch_event.action)) {
            return false;
        }

        to->inject_touch_event.pointer_id = from->fingerId;
        to->inject_touch_event.position.screen_size = screen->frame_size;

        int dw;
        int dh;
        SDL_GL_GetDrawableSize(screen->window, &dw, &dh);

        // SDL touch event coordinates are normalized in the range [0; 1]
        int32_t x = from->x * dw;
        int32_t y = from->y * dh;
        to->inject_touch_event.position.point = screen->convert_drawable_to_frame_coords(x, y);

        to->inject_touch_event.pressure = from->pressure;
        to->inject_touch_event.buttons = (android_motionevent_buttons)0;
        return true;
    }

    void process_touch(const SDL_TouchFingerEvent* event) {
        struct control_msg msg;
        if (convert_touch(event, &msg)) {
            if (!controller->push_msg(&msg)) {
                LOGW("Could not request 'inject touch event'");
            }
        }
    }

    bool convert_mouse_button(const SDL_MouseButtonEvent* from, struct control_msg* to) {
        to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

        if (!convert_mouse_action((SDL_EventType)from->type, &to->inject_touch_event.action)) {
            return false;
        }
        to->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
        to->inject_touch_event.position.screen_size = screen->frame_size;
        to->inject_touch_event.position.point =
            screen->convert_window_to_frame_coords(from->x, from->y);
        to->inject_touch_event.pressure =
            from->type == SDL_MOUSEBUTTONDOWN ? 1.f : 0.f;
        to->inject_touch_event.buttons =
            convert_mouse_buttons(SDL_BUTTON(from->button));
        return true;
    }

    void process_mouse_button(const SDL_MouseButtonEvent* event) {
        if (event->which == SDL_TOUCH_MOUSEID) {
            // simulated from touch events, so it's a duplicate
            return;
        }
        int32_t x = event->x;
        int32_t y = event->y;
        auto btn = screen->getButtonAction(x, y);
        if(btn) {
            auto action = event->type == SDL_MOUSEBUTTONDOWN ? ACTION_DOWN : ACTION_UP;
            switch (btn) {
            case AKEYCODE_VOLUME_UP:
                action_volume_up(action);
                break; 
            case AKEYCODE_VOLUME_DOWN:
                action_volume_down(action);
                break;
            case AKEYCODE_MENU:
                action_app_switch(action);
                break;
            case AKEYCODE_BACK:
                action_back(action);
                break;
            case AKEYCODE_HOME:
                action_home(action);
                break;
            case AKEYCODE_POWER:
                if (action == ACTION_DOWN) {
                    send_keycode(AKEYCODE_POWER, action, "截图组合键");
                    send_keycode(AKEYCODE_VOLUME_DOWN, action, "截图组合键");
                }
                else {
                    send_keycode(AKEYCODE_VOLUME_DOWN, action, "截图组合键");
                    send_keycode(AKEYCODE_POWER, action, "截图组合键");
                }
                break;
            case AKEYCODE_R:
                rotate_device();
                break;
            }
        }
        LOGI("btnclick result:%d", btn);
        bool down = event->type == SDL_MOUSEBUTTONDOWN;
        if (down) {
            if (control && event->button == SDL_BUTTON_RIGHT) {
                press_back_or_turn_screen_on();
                return;
            }
            if (control && event->button == SDL_BUTTON_MIDDLE) {
                action_home(ACTION_DOWN | ACTION_UP);
                return;
            }
        }

        if (!control) {
            return;
        }

        struct control_msg msg;
        if (!convert_mouse_button(event, &msg)) {
            return;
        }

        if (!controller->push_msg(&msg)) {
            LOGW("Could not request 'inject mouse button event'");
            return;
        }

        // Pinch-to-zoom simulation.
        //
        // If Ctrl is hold when the left-click button is pressed, then
        // pinch-to-zoom mode is enabled: on every mouse event until the left-click
        // button is released, an additional "virtual finger" event is generated,
        // having a position inverted through the center of the screen.
        //
        // In other words, the center of the rotation/scaling is the center of the
        // screen.
#define CTRL_PRESSED (SDL_GetModState() & (KMOD_LCTRL | KMOD_RCTRL))
        if ((down && !vfinger_down && CTRL_PRESSED)
            || (!down && vfinger_down)) {
            struct point mouse = msg.inject_touch_event.position.point;
            struct point vfinger = inverse_point(mouse, screen->frame_size);
            enum android_motionevent_action action = down
                ? AMOTION_EVENT_ACTION_DOWN
                : AMOTION_EVENT_ACTION_UP;
            if (!simulate_virtual_finger(action, vfinger)) {
                return;
            }
            vfinger_down = down;
        }
    }

    bool convert_mouse_wheel(const SDL_MouseWheelEvent* from, struct control_msg* to) {

        // mouse_x and mouse_y are expressed in pixels relative to the window
        int mouse_x;
        int mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);

        position position;
        position.screen_size = screen->frame_size;
        position.point = screen->convert_window_to_frame_coords(mouse_x, mouse_y);

        to->type = CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT;

        to->inject_scroll_event.position = position;
        to->inject_scroll_event.hscroll = from->x;
        to->inject_scroll_event.vscroll = from->y;

        return true;
    }

    void process_mouse_wheel(const SDL_MouseWheelEvent* event) {
        struct control_msg msg;
        if (convert_mouse_wheel(event,&msg)) {
            if (!controller->push_msg(&msg)) {
                LOGW("Could not request 'inject mouse wheel event'");
            }
        }
    }

};

