#pragma once
#include "ScrPlayer.h"
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include "SDL2/SDL_image.h"

class FloatButtonBar
{
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* winSurface;
    SDL_Event event;
    SDL_Rect homeRect = { 0,0,36,36 };
    SDL_Rect rectRect = { 0,0,960,72 };
    SDL_Rect backRect = {};
    SDL_SysWMinfo info;
    HWND hwnd;
    Buttons pbuttons;
    bool quit = false;
public:
	void init() {
        window = SDL_CreateWindow("Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,960 ,72 , SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
        SDL_VERSION(&info.version);
        if (SDL_GetWindowWMInfo(window, &info))
        {
            hwnd = info.info.win.window;
        }
        /*设置窗口colorkey*/
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED| WS_EX_TOOLWINDOW);
        SetLayeredWindowAttributes(hwnd, RGB(0,0 ,0 ),0 , LWA_COLORKEY);
        /*设置窗口为悬浮窗 */
        SetWindowPos(hwnd, HWND_TOPMOST,0 ,0 ,0 ,0 , SWP_NOMOVE | SWP_NOSIZE);
        /*--------------*/
        winSurface = SDL_GetWindowSurface(window);
        SDL_GetWindowSize(window, &backRect.w, &backRect.h);
        UINT32 keyColor = SDL_MapRGB(winSurface->format,0 , 0,0 );
        SDL_SetSurfaceBlendMode(winSurface, SDL_BLENDMODE_NONE);

        //while (!quit) {
        //    while (SDL_PollEvent(&event))
        //    {
        //        if (event.type == SDL_QUIT)
        //            quit = true;
        //    }
        //    SDL_FillRect(winSurface, &backRect, keyColor);
        //    rectRect.x += 1;
        //    SDL_FillRect(winSurface, &rectRect, SDL_MapRGB(winSurface->format, 0xff, 0x00, 0x00));
        //    SDL_UpdateWindowSurface(window);
        //    SDL_Delay(1000);
        //}
        //SDL_Quit();
        return;
	}
    void render() {
        SDL_FillRect(winSurface, &rectRect, 0x33F9F9F9);
        pbuttons.getHome(winSurface, &homeRect);
        SDL_UpdateWindowSurface(window);
        SDL_Delay(100);
    }
};

