#pragma once
#include "ScrPlayer.h"
#include <stdbool.h>
#include <libavformat/avformat.h>
#include "common.h"
#include "util/log.h"
#include <cassert>
#include "VideoBuffer.hpp"
#include "util/lock.h"
#include "Buttons.hpp"
#include "SDL2/SDL_image.h"
#include "FloatButtonBar.hpp"
#define DISPLAY_MARGINS 96
#define SC_WINDOW_POSITION_UNDEFINED (-0x8000)
#define BUTTON_WIDTH 42
#define BUTTON_HEIGHT 49
class Screen 
{
private:
    bool resize_pending; // resize requested while fullscreen or maximized
    // The content size the last time the window was not maximized or
    // fullscreen (meaningful only when resize_pending is true)
    size_w windowed_content_size;
    static size_w  get_rotated_size(size_w org_size, int rotation) {
        size_w rotated_size;
        if (rotation & 1) {
            rotated_size.width = org_size.height;
            rotated_size.height = org_size.width;
        }
        else {
            rotated_size.width = org_size.width;
            rotated_size.height = org_size.height;
        }
        return rotated_size;
    }

    size_w get_window_size() {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        return size_w{(uint16_t)width,(uint16_t)height};
    }

    // set the window size to be applied when fullscreen is disabled
    void set_window_size(size_w new_size) {
        assert(!fullscreen);
        assert(!maximized);
        SDL_SetWindowSize(window, new_size.width, new_size.height);
    }
    void set_window_size(int w, int h) {
        if (fullscreen || maximized)return;
        SDL_SetWindowSize(window, w,h);
    }
    // get the preferred display bounds (i.e. the screen bounds with some margins)
    bool get_preferred_display_bounds(size_w* bounds) {
        SDL_Rect rect;
#ifdef SCRCPY_SDL_HAS_GET_DISPLAY_USABLE_BOUNDS
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayUsableBounds((i), (r))
#else
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayBounds((i), (r))
#endif
        if (GET_DISPLAY_BOUNDS(0, &rect)) {
            LOGW("Could not get display usable bounds: %s", SDL_GetError());
            return false;
        }

        bounds->width = MAX(0, rect.w - DISPLAY_MARGINS);
        bounds->height = MAX(0, rect.h - DISPLAY_MARGINS);
        return true;
    }

    bool is_optimal_size(size_w current_size, size_w content_size) {
        // The size is optimal if we can recompute one dimension of the current
        // size from the other
        return current_size.height == current_size.width * content_size.height
            / content_size.width
            || current_size.width == current_size.height * content_size.width
            / content_size.height;
    }

    // return the optimal size of the window, with the following constraints:
    //  - it attempts to keep at least one dimension of the current_size (i.e. it
    //    crops the black borders)
    //  - it keeps the aspect ratio
    //  - it scales down to make it fit in the display_size
    size_w get_optimal_size(size_w current_size, size_w content_size) {
        if (content_size.width == 0 || content_size.height == 0) {
            // avoid division by 0
            return current_size;
        }

        size_w window_size;

        size_w display_size;
        if (!get_preferred_display_bounds(&display_size)) {
            // could not get display bounds, do not constraint the size
            window_size.width = current_size.width;
            window_size.height = current_size.height;
        }
        else {
            window_size.width = MIN(current_size.width, display_size.width);
            window_size.height = MIN(current_size.height, display_size.height);
        }

        if (is_optimal_size(window_size, content_size)) {
            return window_size;
        }

        bool keep_width = content_size.width * window_size.height
                    > content_size.height * window_size.width;
        if (keep_width) {
            // remove black borders on top and bottom
            window_size.height = content_size.height * window_size.width/ content_size.width + BUTTON_HEIGHT;
        }
        else {
            // remove black borders on left and right (or none at all if it already
            // fits)
            window_size.width = content_size.width * window_size.height / content_size.height + BUTTON_WIDTH;
        }

        return window_size;
    }
    // same as get_optimal_size(), but read the current size from the window
    size_w get_optimal_window_size(size_w content_size) {
        size_w window_size = get_window_size();
        return get_optimal_size(window_size, content_size);
    }
    size_w get_initial_optimal_size(size_w content_size, uint16_t req_width, uint16_t req_height) {
        size_w window_size;
        if (!req_width && !req_height) {
            window_size = get_optimal_size(content_size, content_size);
        }
        else {
            if (req_width) {
                window_size.width = req_width;
            }
            else {
                // compute from the requested height
                window_size.width = (uint32_t)req_height * content_size.width
                    / content_size.height;
            }
            if (req_height) {
                window_size.height = req_height;
            }
            else {
                // compute from the requested width
                window_size.height = (uint32_t)req_width * content_size.height
                    / content_size.width;
            }
        }
        return window_size;
    }
    void update_content_rect() {
        int dw;
        int dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);

        // The drawable size_w is the window size * the HiDPI scale
        size_w drawable_size = { dw, dh };

        if (is_optimal_size(drawable_size, content_size)) {
            rect.x = 0;
            rect.y = 0;
            rect.w = drawable_size.width;
            rect.h = drawable_size.height;
            return;
        }
        bool keep_width = content_size.width * drawable_size.height
                    > content_size.height * drawable_size.width;
        if (keep_width) {
            rect.x = 0;
            rect.y = 0;
            rect.w = drawable_size.width;
            rect.h = drawable_size.width * content_size.height / content_size.width;
//            rect.y = (drawable_size.height - BUTTIN_HEIGHT - rect.h) / 2;
            set_window_size(rect.w, rect.h + BUTTON_WIDTH);

        }
        else {
            rect.x = 0;
            rect.y = 0;
            rect.h = drawable_size.height;
            rect.w = drawable_size.height * content_size.width / content_size.height;
//            rect.x = (drawable_size.width - BUTTON_WIDTH - rect.w) / 2;
            set_window_size(rect.w + BUTTON_WIDTH, rect.h);
        }
    }

public:
    Buttons buttons;
    bool has_frame;
    bool fullscreen;
    bool maximized;
    bool no_window;
    bool mipmaps;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    size_w frame_size;
    size_w content_size; // rotated frame_size
    // client rotation: 0, 1, 2 or 3 (x90 degrees counterclockwise)
    unsigned rotation;
    // rectangle of the content (excluding black borders)
    struct SDL_Rect rect;
    void init() {
        window = NULL;
        renderer = NULL;
        texture = NULL;
        frame_size = {0,0};
        content_size = {0,0};
        resize_pending = false;
        windowed_content_size = {0,0};
        rotation = 0;
        rect = {0,0,0,0};
        has_frame = false;
        fullscreen = false;
        maximized = false;
        no_window = false;
        mipmaps = false;
    }
    void create_texture() {
          texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
              SDL_TEXTUREACCESS_STREAMING,
              frame_size.width, frame_size.height);
          if (!texture) {
              LOGC("Could not create texture: %s", SDL_GetError());
              destroy();
          }
      }
      // initialize screen, create window, renderer and texture (window is hidden)
      // window_x and window_y accept SC_WINDOW_POSITION_UNDEFINED
      bool init_rendering(const char* window_title,
          size_w frame_size, bool always_on_top,
          int16_t window_x, int16_t window_y, uint16_t window_width,
          uint16_t window_height, bool window_borderless,
          uint8_t rotation, SDL_Surface* icon) {
          this->frame_size = frame_size;
          this->rotation = rotation;
          if (rotation) {
              LOGI("Initial display rotation set to %u", rotation);
          }
          content_size = get_rotated_size(frame_size, rotation);

          size_w window_size =
              get_initial_optimal_size(content_size, window_width, window_height);
          uint32_t window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
#ifdef HIDPI_SUPPORT
          window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
          if (always_on_top) {
#ifdef SCRCPY_SDL_HAS_WINDOW_ALWAYS_ON_TOP
              window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
#else
              LOGW("The 'always on top' flag is not available "
                  "(compile with SDL >= 2.0.5 to enable it)");
#endif
          }
          if (window_borderless) {
              window_flags |= SDL_WINDOW_BORDERLESS;
          }

          int x = window_x != SC_WINDOW_POSITION_UNDEFINED
              ? window_x : (int)SDL_WINDOWPOS_UNDEFINED;
          int y = window_y != SC_WINDOW_POSITION_UNDEFINED
              ? window_y : (int)SDL_WINDOWPOS_UNDEFINED;
          window = SDL_CreateWindow(window_title, x, y,
              window_size.width, window_size.height,
              window_flags);
          if (!window) {
              LOGC("Could not create window: %s", SDL_GetError());
              return false;
          }
          renderer = SDL_CreateRenderer(window, -1,SDL_RENDERER_ACCELERATED);
          if (!renderer) {
              LOGC("Could not create renderer: %s", SDL_GetError());
              this->destroy();
              return false;
          }
          buttons.init(renderer);
          SDL_RendererInfo renderer_info;
          int r = SDL_GetRendererInfo(renderer, &renderer_info);
          const char* renderer_name = r ? NULL : renderer_info.name;
          LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");
          /*this->bars.init();*/
          icon = Buttons::getIcon();
          if(icon){
              SDL_SetWindowIcon(window, icon);
              SDL_FreeSurface(icon);
          }
          else {
              LOGW("Could not load icon");
          }

          LOGI("Initial texture: %" PRIu16 "x%" PRIu16, frame_size.width,
              frame_size.height);
          create_texture();

          // Reset the window size to trigger a SIZE_CHANGED event, to workaround
          // HiDPI issues with some SDL renderers when several displays having
          // different HiDPI scaling are connected
          SDL_SetWindowSize(window, window_size.width, window_size.height);

          update_content_rect();

          return true;
      }

      // show the window
      void show_window() {
          SDL_ShowWindow(window);
      }

      // destroy window, renderer and texture (if any)
      void destroy() {
          if (texture) {
              SDL_DestroyTexture(texture);
          }
          if (renderer) {
              SDL_DestroyRenderer(renderer);
          }
          if (window) {
              SDL_DestroyWindow(window);
          }
      }


      void resize_for_content(size_w old_content_size,size_w new_content_size) {
           size_w window_size = get_window_size();
           size_w target_size = {(uint32_t)window_size.width * new_content_size.width
                      / old_content_size.width,
              (uint32_t)window_size.height * new_content_size.height
                      / old_content_size.height,
          };
          target_size = get_optimal_size(target_size, new_content_size);
          set_window_size(target_size);
      }

      void set_content_size( size_w new_content_size) {
          if (!fullscreen && !maximized) {
              resize_for_content(content_size, new_content_size);
          }
          else if (!resize_pending) {
              // Store the windowed size to be able to compute the optimal size once
              // fullscreen and maximized are disabled
              windowed_content_size = content_size;
              resize_pending = true;
          }

          content_size = new_content_size;
      }

      void apply_pending_resize() {
          assert(!fullscreen);
          assert(!maximized);
          if (resize_pending) {
              resize_for_content(windowed_content_size,content_size);
              resize_pending = false;
          }
      }

      // set the display rotation (0, 1, 2 or 3, x90 degrees counterclockwise)
      void set_rotation(unsigned rotation) {
          assert(rotation < 4);
          if (rotation == rotation) {
              return;
          }

           size_w new_content_size = get_rotated_size(frame_size, rotation);

          set_content_size(new_content_size);

          rotation = rotation;
          LOGI("Display rotation set to %u", rotation);

          render(true);
      }

      // recreate the texture and resize the window if the frame size has changed
      bool prepare_for_frame( size_w new_frame_size) {
          if (frame_size.width != new_frame_size.width
              || frame_size.height != new_frame_size.height) {
              // frame dimension changed, destroy texture
              SDL_DestroyTexture(texture);

              frame_size = new_frame_size;

              size_w new_content_size = get_rotated_size(new_frame_size, rotation);
              set_content_size(new_content_size);
              update_content_rect();

              LOGI("New texture: %" PRIu16 "x%" PRIu16,
                  frame_size.width, frame_size.height);
              create_texture();
          }

          return true;
      }

      // write the frame into the texture
      void update_texture(const AVFrame* frame) {
          SDL_UpdateYUVTexture(texture, NULL,
              frame->data[0], frame->linesize[0],
              frame->data[1], frame->linesize[1],
              frame->data[2], frame->linesize[2]);
      }

      void render(bool update_content_rect) {
          if (update_content_rect) {
              this->update_content_rect();
          }

          SDL_RenderClear(renderer);
          if (rotation == 0) {
              SDL_RenderCopy(renderer, texture, NULL, &rect);
          }
          else {
              // rotation in RenderCopyEx() is clockwise, while rotation is
              // counterclockwise (to be consistent with --lock-video-orientation)
              int cw_rotation = (4 - rotation) % 4;
              double angle = 90 * cw_rotation;

              SDL_Rect dstrect;
              if (rotation & 1) {
                  dstrect.x = rect.x + (rect.w - rect.h) / 2;
                  dstrect.y = rect.y + (rect.h - rect.w) / 2;
                  dstrect.w = rect.h;
                  dstrect.h = rect.w;
              }
              else {
                  assert(rotation == 2);
                  dstrect = rect;
              }

              SDL_RenderCopyEx(renderer, texture, NULL, &dstrect,
                  angle, NULL, SDL_FLIP_NONE);
          }
          buttons.render(renderer,&rect);
          SDL_RenderPresent(renderer);
      }

      void switch_fullscreen() {
          uint32_t new_mode = fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
          if (SDL_SetWindowFullscreen(window, new_mode)) {
              LOGW("Could not switch fullscreen mode: %s", SDL_GetError());
              return;
          }

          fullscreen = !fullscreen;
          if (!fullscreen && !maximized) {
              apply_pending_resize();
          }

          LOGD("Switched to %s mode", fullscreen ? "fullscreen" : "windowed");
          render(true);
      }

      // resize if necessary and write the rendered frame into the texture
      bool update_frame(VideoBuffer* vb) {
          vb->lock();
          const AVFrame* frame = vb->consume_rendered_frame();
          size_w new_frame_size = { frame->width, frame->height };
          if (!prepare_for_frame(new_frame_size)) {
              vb->unlock();
              return false;
          }
          update_texture(frame);
          vb->unlock();

          render(false);
          return true;
      }

      // resize window to optimal size (remove black borders)
      void resize_to_fit() {
          if (fullscreen || maximized) {
              return;
          }

          size_w optimal_size =
              get_optimal_window_size(content_size);
          SDL_SetWindowSize(window, optimal_size.width, optimal_size.height);
          LOGD("Resized to optimal size: %ux%u", optimal_size.width,optimal_size.height);
      }

      // resize window to 1:1 (pixel-perfect)
      void resize_to_pixel_perfect() {
          if (fullscreen) {
              return;
          }

          if (maximized) {
              SDL_RestoreWindow(window);
              maximized = false;
          }
          SDL_SetWindowSize(window, content_size.width, content_size.height);
          LOGD("Resized to pixel-perfect: %ux%u", content_size.width,content_size.height);
      }

      // react to window events
      void handle_window_event(const SDL_WindowEvent* event) {

          switch (event->event) {
          case SDL_WINDOWEVENT_EXPOSED:
              render(true);
              break;
          case SDL_WINDOWEVENT_SIZE_CHANGED:
              render(true);
              break;
          case SDL_WINDOWEVENT_MAXIMIZED:
              maximized = true;
              break;
          case SDL_WINDOWEVENT_RESTORED:
              if (fullscreen) {
                  // On Windows, in maximized+fullscreen, disabling fullscreen
                  // mode unexpectedly triggers the "restored" then "maximized"
                  // events, leaving the window in a weird state (maximized
                  // according to the events, but not maximized visually).
                  break;
              }
              maximized = false;
              apply_pending_resize();
              break;
          }
      }

      // convert point from drawable coordinates to frame coordinates
      // x and y are expressed in pixels
      point convert_drawable_to_frame_coords(int32_t x, int32_t y) {
          assert(rotation < 4);

          int32_t w = content_size.width;
          int32_t h = content_size.height;


          x = (int64_t)(x - rect.x) * w / rect.w;
          y = (int64_t)(y - rect.y) * h / rect.h;

          // rotate
          point result;
          switch (rotation) {
          case 0:
              result.x = x;
              result.y = y;
              break;
          case 1:
              result.x = h - y;
              result.y = x;
              break;
          case 2:
              result.x = w - x;
              result.y = h - y;
              break;
          default:
              assert(rotation == 3);
              result.x = y;
              result.y = w - x;
              break;
          }
          return result;
      }
      // convert point from window coordinates to frame coordinates
      // x and y are expressed in pixels
      point convert_window_to_frame_coords(int32_t x, int32_t y) {
          hidpi_scale_coords(&x, &y);
          return convert_drawable_to_frame_coords(x, y);
      }

      // Convert coordinates from window to drawable.
      // Events are expressed in window coordinates, but content is expressed in
      // drawable coordinates. They are the same if HiDPI scaling is 1, but differ
      // otherwise.
      void hidpi_scale_coords(int32_t* x, int32_t* y) {
          // take the HiDPI scaling (dw/ww and dh/wh) into account
          int ww, wh, dw, dh;
          SDL_GetWindowSize(window, &ww, &wh);
          SDL_GL_GetDrawableSize(window, &dw, &dh);

          // scale for HiDPI (64 bits for intermediate multiplications)
          *x = (int64_t)*x * dw / ww;
          *y = (int64_t)*y * dh / wh;
      }


      void rotate_client_left() {
          unsigned new_rotation = (rotation + 1) % 4;
          set_rotation(new_rotation);
      }

      void rotate_client_right() {
          unsigned new_rotation = (rotation + 3) % 4;
          set_rotation(new_rotation);
      }

      enum android_keycode getButtonAction(int x, int y) {
          bool ishor = rect.w > rect.h;
          if ((ishor && y < rect.h)||(!ishor&&x<rect.w))return AKEYCODE_UNKNOWN;
          return buttons.getButtonAction(x, y);
      }
      void getHotButton(int x, int y) {
          bool ishor = rect.w > rect.h;
          if ((ishor && y < rect.h) || (!ishor && x < rect.w))return;
          buttons.getHotButtons(x, y);
      }
};

