/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_OHOS

#include "SDL_ohoswindow.h"

#include "../../core/ohos/SDL_ohos.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "../SDL_egl_c.h"
#endif

int OHOS_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = NULL;

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (data == NULL) {
        return SDL_OutOfMemory();
    }
    window->driverdata = (void *)data;
    return OHOS_Window_Create(window->id, (window->flags & SDL_WINDOW_BORDERLESS) ? SDL_FALSE : SDL_TRUE, window->w, window->h, window->x, window->y);
}

int OHOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    (void)_this;
    (void)window;
    (void)data;
    return 0;
}

void OHOS_SetWindowTitle(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon)
{
    (void)_this;
    (void)window;
    (void)icon;
}

void OHOS_SetWindowPosition(_THIS, SDL_Window *window)
{
    OHOS_Window_SetPos(window->id, window->x, window->y);
}

void OHOS_SetWindowSize(_THIS, SDL_Window *window)
{
    OHOS_Window_SetSize(window->id, window->w, window->h);
}

void OHOS_SetWindowMinimumSize(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_SetWindowMaximumSize(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

int OHOS_GetWindowBordersSize(_THIS, SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
    (void)_this;
    (void)window;
    (void)top;
    (void)left;
    (void)bottom;
    (void)right;
    return 0;
}

void OHOS_GetWindowSizeInPixels(_THIS, SDL_Window *window, int *w, int *h)
{
    (void)_this;
    (void)window;
    (void)w;
    (void)h;
}

int OHOS_SetWindowOpacity(_THIS, SDL_Window *window, float opacity)
{
    (void)_this;
    (void)window;
    (void)opacity;
    return 0;
}

int OHOS_SetWindowModalFor(_THIS, SDL_Window *modal_window, SDL_Window *parent_window)
{
    (void)_this;
    (void)modal_window;
    (void)parent_window;
    return 0;
}

int OHOS_SetWindowInputFocus(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
    return 0;
}

void OHOS_ShowWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_HideWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_RaiseWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_MaximizeWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_MinimizeWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_RestoreWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_SetWindowBordered(_THIS, SDL_Window *window, SDL_bool bordered)
{
    (void)_this;
    (void)window;
    (void)bordered;
}

void OHOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable)
{
    (void)_this;
    (void)window;
    (void)resizable;
}

void OHOS_SetWindowAlwaysOnTop(_THIS, SDL_Window *window, SDL_bool on_top)
{
    (void)_this;
    (void)window;
    (void)on_top;
}

void OHOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    (void)_this;
    (void)window;
    (void)display;
    (void)fullscreen;
}

int OHOS_SetWindowGammaRamp(_THIS, SDL_Window *window, const Uint16 *ramp)
{
    (void)_this;
    (void)window;
    (void)ramp;
    return 0;
}

int OHOS_GetWindowGammaRamp(_THIS, SDL_Window *window, Uint16 *ramp)
{
    (void)_this;
    (void)window;
    (void)ramp;
    return 0;
}

void *OHOS_GetWindowICCProfile(_THIS, SDL_Window *window, size_t *size)
{
    (void)_this;
    (void)window;
    (void)size;
    return NULL;
}

int OHOS_GetWindowDisplayIndex(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
    return 0;
}

void OHOS_SetWindowMouseRect(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_SetWindowMouseGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
    (void)_this;
    (void)window;
    (void)grabbed;
}

void OHOS_SetWindowKeyboardGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
    (void)_this;
    (void)window;
    (void)grabbed;
}

void OHOS_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

    OHOS_Window_Destroy(window->id);

    if (data == NULL) {
        return;
    }
#ifdef SDL_VIDEO_OPENGL_EGL
    if (data->egl_surface != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, data->egl_surface);
        data->egl_surface = EGL_NO_SURFACE;
    }
#else
    (void)_this;
#endif
    SDL_free(data);
    window->driverdata = NULL;
}

int OHOS_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    (void)_this;
    (void)window;
    (void)format;
    (void)pixels;
    (void)pitch;
    return 0;
}

int OHOS_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    (void)_this;
    (void)window;
    (void)rects;
    (void)numrects;
    return 0;
}

void OHOS_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_OnWindowEnter(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

int OHOS_FlashWindow(_THIS, SDL_Window *window, SDL_FlashOperation operation)
{
    (void)_this;
    (void)window;
    (void)operation;
    return 0;
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
