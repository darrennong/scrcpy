// ScrPlayer.cpp : 定义应用程序的入口点。
//
#include "ScrPlayer.h"

#include "Server.hpp"
#include "Controller.hpp"
#include "InputManager.hpp"
#include "FileHandler.hpp"
#include "Screen.hpp"
#include "Decoder.hpp"
#include "Recorder.hpp"
#include "VideoStream.hpp"
#include "Buttons.hpp"

BOOL WINAPI windows_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT) {
        SDL_Event event;
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
        return TRUE;
    }
    return FALSE;
}

FpsCounter fpsCounter;
VideoBuffer videoBuffer;
Decoder decoder;
Recorder recorder;
Screen screen;
Server server;
VideoStream stream;
Controller controller;
InputManager inputManager;


static SDL_LogPriority sdl_priority_from_av_level(int level) {
    switch (level) {
    case AV_LOG_PANIC:
    case AV_LOG_FATAL:
        return SDL_LOG_PRIORITY_CRITICAL;
    case AV_LOG_ERROR:
        return SDL_LOG_PRIORITY_ERROR;
    case AV_LOG_WARNING:
        return SDL_LOG_PRIORITY_WARN;
    case AV_LOG_INFO:
    default:
        return SDL_LOG_PRIORITY_INFO;
    }
}
static void av_log_callback(void* avcl, int level, const char* fmt, va_list vl) {
    (void)avcl;
    SDL_LogPriority priority = sdl_priority_from_av_level(level);
    if (priority == 0) {
        return;
    }
    int len = strlen(fmt)+1;
    char* local_fmt = (char*)SDL_malloc(len + 10);
    if (!local_fmt) {
        LOGC("Could not allocate string");
        return;
    }
    // strcpy is safe here, the destination is large enough
    strcpy_s(local_fmt,10, "[FFmpeg] ");
    strcpy_s(local_fmt + 9, len,fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_VIDEO, priority, local_fmt, vl);
    SDL_free(local_fmt);
}


static enum event_result handle_event(SDL_Event* event) {
    switch (event->type) {
    case EVENT_STREAM_STOPPED:
        LOGD("Video stream stopped");
        return EVENT_RESULT_STOPPED_BY_EOS;
    case SDL_QUIT:
        LOGD("User requested to quit");
        return EVENT_RESULT_STOPPED_BY_USER;
    case EVENT_NEW_FRAME:
        if (!screen.has_frame) {
            screen.has_frame = true;
            // this is the very first frame, show the window
            screen.show_window();
        }
        if (!screen.update_frame(&videoBuffer)) {
            return EVENT_RESULT_CONTINUE;
        }
        break;
    case SDL_WINDOWEVENT:
        screen.handle_window_event(&event->window);
        break;
    case SDL_TEXTINPUT:
        inputManager.process_text_input(&event->text);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        // some key events do not interact with the device, so process the
        // event even if control is disabled
        inputManager.process_key(&event->key);
        break;
    case SDL_MOUSEMOTION:
        inputManager.process_mouse_motion(&event->motion);
        break;
    case SDL_MOUSEWHEEL:
        inputManager.process_mouse_wheel(&event->wheel);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        // some mouse events do not interact with the device, so process
        // the event even if control is disabled
        inputManager.process_mouse_button(&event->button);
        break;
    case SDL_FINGERMOTION:
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
        inputManager.process_touch(&event->tfinger);
        break;
    case SDL_DROPFILE: {
        file_handler_action_t action;
        if (is_apk(event->drop.file)) {
            action = ACTION_INSTALL_APK;
        }
        else {
            action = ACTION_PUSH_FILE;
        }
        //file_handler_request(&file_handler, action, event->drop.file);
        break;
    }
    }
    return EVENT_RESULT_CONTINUE;
}

static bool event_loop() {
#ifdef CONTINUOUS_RESIZING_WORKAROUND
    SDL_AddEventWatch(event_watcher, NULL);
#endif
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        enum event_result result = handle_event(&event);
        switch (result) {
        case EVENT_RESULT_STOPPED_BY_USER:
            return true;
        case EVENT_RESULT_STOPPED_BY_EOS:
            LOGW("Device disconnected");
            return false;
        case EVENT_RESULT_CONTINUE:
            break;
        }
    }
    return false;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    //启动SDL
    SDL_Init(SDL_INIT_EVERYTHING);
    bool ok = SetConsoleCtrlHandler(windows_ctrl_handler, TRUE);
    ok = SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d");
    ok = SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    ok = SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    ok = SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    ok = SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_DisableScreenSaver();
    fpsCounter.init();
    videoBuffer.init(&fpsCounter, false);
    decoder.init(&videoBuffer);
    if (!server.start()) {
        LOGE("服务器连接失败....");
        return 1;
    }
    char device_name[DEVICE_NAME_FIELD_LENGTH];
    struct size frame_size;
    server.device_read_info(device_name, &frame_size);
    av_log_set_callback(av_log_callback);

    stream.init(server.videoSocket, &decoder, nullptr);
    // now we consumed the header values, the socket receives the video stream
    // start the stream
    if (!stream.start()) {
        return 2;
    }
    controller.init(server.controlSocket);
    controller.start();
    screen.init_rendering(device_name, frame_size, true, 40, 40, 860, 540, false, 0, NULL);
    inputManager.init(&controller, &videoBuffer, &fpsCounter, &screen);
    event_loop();
    //退出SDL 
    SDL_Quit();

    return 0;
}