#pragma once
#include "ScrPlayer.h"
#include <SDL2/SDL_image.h>
#include "android/keycodes.h"

#define NUM_BUTTON 11 
#define BACK_WIDTH 36
class Buttons
{
private:
	SDL_Surface* sheet;
	SDL_Texture* buttonText;
	const SDL_Rect clipTextures[8] = { { 381, 221, 84, 98 },{ 385, 5, 84, 98 },{ 192, 221, 84, 98 },{ 5, 221, 84, 98 },
		{ 193, 113, 84, 98 },{ 5, 113, 84, 98 },{ 5, 5, 84, 98 },{ 195, 5, 84, 98 } };
	const SDL_Rect clipTextureHots[8] = { { 5, 329, 84, 98 },{ 381, 113, 84, 98 },{ 287, 221, 84, 98 },{ 99, 221, 84, 98 },
		{ 287, 113, 84, 98 },{ 99, 113, 84, 98 },{ 100, 5, 84, 98 },{ 290, 5, 84, 98 } };
	int hotBtnIdx = -1;
	const SDL_Rect clipBtns[8];
	const enum android_keycode keycodes[8] = { AKEYCODE_TV_NETWORK,AKEYCODE_VOLUME_UP,AKEYCODE_VOLUME_DOWN,AKEYCODE_POWER,
	AKEYCODE_R,AKEYCODE_MENU,AKEYCODE_BACK,AKEYCODE_HOME };

	inline SDL_Surface* getClip(int x, int y, int w, int h) {
		SDL_Surface destination;
		SDL_Rect clip;
		clip.x = x;
		clip.y = y;
		clip.w = w;
		clip.h = h;
		if (!sheet->locked) {
			SDL_BlitSurface(sheet, &clip, &destination, NULL);
		}
		return &destination;
	}
	inline static SDL_Surface* getSurface(const char* filename) {
		const char* base = SDL_GetBasePath();
		int len = strlen(base) + strlen(filename)+2;
		char* url = (char*)alloca(len);
		strcpy_s(url, strlen(base) + 1, base);
		strcpy_s(url + strlen(base), strlen(filename) + 1, filename);
		LOGI("icon path = %s", url);
		auto ret = IMG_Load(url);
		LOGI("Lockdata%d", ret->locked);
		return ret;
	}
	inline const SDL_Rect* getBtnTexture(int btnIdx) {
		if (hotBtnIdx == btnIdx) return &clipTextureHots[btnIdx];
		return &clipTextures[btnIdx];
	}
public:
	Buttons() {
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			LOGE("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		}
		else {
			int imgFlags = IMG_INIT_PNG;
			int initRet = IMG_Init(imgFlags);
			if (!(initRet & imgFlags))
			{
				LOGE("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
			}
			sheet = getSurface("spritesheet.png");
		}
	}
    void getHome(SDL_Surface* surface, SDL_Rect* dest) {
		SDL_Rect clip = { 5, 5, 72, 72 };
		SDL_BlitSurface(sheet, &clip, surface, dest);
	}
	static SDL_Surface* getIcon() {
		return getSurface("icon.png");
	}
	void storeBtnRect(int idx, SDL_Rect* rect) {
		memcpy((void*)&clipBtns[idx], rect, sizeof(SDL_Rect));
	}
	void init(SDL_Renderer* renderer) {
		buttonText = SDL_CreateTextureFromSurface(renderer, sheet);
	}
	void render(SDL_Renderer*  renderer, SDL_Rect* rect) {
		bool ishor = rect->w > rect->h;
		auto bx = ishor ? rect->x : rect->x + rect->w;
		auto by = ishor ? rect->y + rect->h : rect->y;
		auto gap = ishor ? rect->w / NUM_BUTTON : rect->h / NUM_BUTTON;
		SDL_Rect btnRect = { bx,by,42,49 };
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0.5);
		int btnIdx = 0;
		if (ishor) {
			SDL_Rect backRect = {bx,by,rect->w,BACK_WIDTH};
			SDL_RenderFillRect(renderer, &backRect);
			btnRect.x += gap * 2;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.x += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.x += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.x += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.x += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.x += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.x += gap*2;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.x += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
		}
		else {
			SDL_Rect backRect = { bx,by,BACK_WIDTH,rect->h };
			SDL_RenderFillRect(renderer, &backRect);
			btnRect.y += gap * 2;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.y += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.y += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.y += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.y += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.y += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.y += gap * 2;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
			btnRect.y += gap;
			SDL_RenderCopy(renderer, buttonText, getBtnTexture(btnIdx), &btnRect);
			storeBtnRect(btnIdx++, &btnRect);
		}
	}
	enum android_keycode getButtonAction(int x,int y) {
		for (int i = 0; i < 8; i++) {
			auto btn = clipBtns[i];
			if (btn.x < x && btn.y < y && x < btn.x + btn.w && y < btn.y + btn.h) {
				return keycodes[i];
			}
		}
		return AKEYCODE_UNKNOWN;
	}

	void getHotButtons(int x, int y) {
		for (int i = 0; i < 8; i++) {
			auto btn = clipBtns[i];
			if (btn.x < x && btn.y < y && x < btn.x + btn.w && y < btn.y + btn.h) {
				hotBtnIdx = i;
				return;
			}
		}
		hotBtnIdx = -1;
	}
};

