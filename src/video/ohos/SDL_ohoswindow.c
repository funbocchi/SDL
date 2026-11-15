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
#include "../../events/SDL_windowevents_c.h"
#include "SDL_ohosvideo.h"

#include <multimedia/image_framework/image/pixelmap_native.h>
#include <window_manager/oh_window_comm.h>
#include <window_manager/oh_window.h>
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

    /* The real drawable size only arrives once the ArkUI surface reports back, but
     * renderers query it during creation, so start from the window size. */
    data->drawable_width = window->w;
    data->drawable_height = window->h;

    return OHOS_Window_Create(window->id,
                              (window->flags & SDL_WINDOW_BORDERLESS) ? SDL_FALSE : SDL_TRUE,
                              window->w, window->h, window->x, window->y,
                              (window->flags & SDL_WINDOW_RESIZABLE) ? SDL_TRUE : SDL_FALSE);
}

int OHOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    (void)_this;
    (void)window;
    (void)data;
    return SDL_Unsupported();
}

void OHOS_SetWindowTitle(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_SetTitle(window->id, window->title);
}

void OHOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon)
{
    (void)_this;
    (void)window;
    (void)icon;
}

void OHOS_SetWindowPosition(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_SetPos(window->id, window->x, window->y);
}

void OHOS_SetWindowSize(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_SetSize(window->id, window->w, window->h);
}

void OHOS_SetWindowMinimumSize(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_SetMinimumSize(window->id, window->min_w, window->min_h);
}

void OHOS_SetWindowMaximumSize(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_SetMaximumSize(window->id, window->max_w, window->max_h);
}

int OHOS_GetWindowBordersSize(_THIS, SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    WindowManager_WindowProperties properties;
    int height_offset, width_offset;

    (void)_this;

    *top = 0;
    *left = 0;
    *bottom = 0;
    *right = 0;

    if (data == NULL || data->ohos_window_id == 0) {
        return 0;
    }

    if (OH_WindowManager_GetWindowProperties(data->ohos_window_id, &properties) != 0) {
        return SDL_SetError("OH_WindowManager_GetWindowProperties failed for window %u", window->id);
    }

    /* windowRect covers the decoration, drawableRect is offset from the window
       origin, so the difference is split between the two opposite edges. */
    height_offset = (int)properties.windowRect.height - (int)properties.drawableRect.height;
    width_offset = (int)properties.windowRect.width - (int)properties.drawableRect.width;

    *top = (int)properties.drawableRect.posY;
    *left = (int)properties.drawableRect.posX;
    *bottom = height_offset - *top;
    *right = width_offset - *left;

    return 0;
}

void OHOS_GetWindowSizeInPixels(_THIS, SDL_Window *window, int *w, int *h)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    *w = data->drawable_width;
    *h = data->drawable_height;
}

int OHOS_SetWindowOpacity(_THIS, SDL_Window *window, float opacity)
{
    (void)_this;

    return OHOS_Window_SetOpacity(window->id, opacity);
}

int OHOS_SetWindowModalFor(_THIS, SDL_Window *modal_window, SDL_Window *parent_window)
{
    (void)_this;

    return OHOS_Window_SetModalFor(modal_window->id, parent_window->id);
}

int OHOS_SetWindowInputFocus(_THIS, SDL_Window *window)
{
    (void)_this;

    return OHOS_Window_SetInputFocus(window->id);
}

void OHOS_ShowWindow(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_Show(window->id);
}

void OHOS_HideWindow(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_Hide(window->id);
}

void OHOS_RaiseWindow(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_Raise(window->id);
}

void OHOS_MaximizeWindow(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_Maximize(window->id);
}

void OHOS_MinimizeWindow(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_Minimize(window->id);
}

void OHOS_RestoreWindow(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_Restore(window->id);
}

void OHOS_SetWindowBordered(_THIS, SDL_Window *window, SDL_bool bordered)
{
    (void)_this;

    OHOS_Window_SetBordered(window->id, bordered, window->w, window->h, window->x, window->y);
}

void OHOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable)
{
    (void)_this;

    OHOS_Window_SetResizable(window->id, resizable);
}

void OHOS_SetWindowAlwaysOnTop(_THIS, SDL_Window *window, SDL_bool on_top)
{
    (void)_this;

    OHOS_Window_SetAlwaysOnTop(window->id, on_top);
}

void OHOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    (void)_this;
    (void)display;

    OHOS_Window_SetFullscreen(window->id, fullscreen);
}

int OHOS_SetWindowGammaRamp(_THIS, SDL_Window *window, const Uint16 *ramp)
{
    (void)_this;

    return OHOS_Window_SetGammaRamp(window->id, ramp);
}

int OHOS_GetWindowGammaRamp(_THIS, SDL_Window *window, Uint16 *ramp)
{
    (void)_this;

    return OHOS_Window_GetGammaRamp(window->id, ramp);
}

void *OHOS_GetWindowICCProfile(_THIS, SDL_Window *window, size_t *size)
{
    (void)_this;

    return OHOS_Window_GetICCProfile(window->id, size);
}

int OHOS_GetWindowDisplayIndex(_THIS, SDL_Window *window)
{
    (void)_this;

    return (int)window->display_index;
}

void OHOS_SetWindowMouseRect(_THIS, SDL_Window *window)
{
    (void)_this;

    OHOS_Window_SetMouseRect(window->id);
}

void OHOS_SetWindowMouseGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
    (void)_this;

    OHOS_Window_SetMouseGrab(window->id, grabbed);
}

void OHOS_SetWindowKeyboardGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
    (void)_this;

    OHOS_Window_SetKeyboardGrab(window->id, grabbed);
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
    return SDL_Unsupported();
}

int OHOS_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    (void)_this;
    (void)window;
    (void)rects;
    (void)numrects;
    return SDL_Unsupported();
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

    return OHOS_Window_Flash(window->id, operation);
}

static void OHOS_Window_HandleStateChanged(_THIS, const SDL_Event *event)
{
    SDL_Window *window = NULL;
    int32_t event_type = (int32_t)(intptr_t)event->user.data1;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    switch (event_type) {
    case 1: /* WINDOW_SHOWN */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_SHOWN, 0, 0);
        break;
    case 4: /* WINDOW_HIDDEN */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_HIDDEN, 0, 0);
        break;
    case 7: /* WINDOW_DESTROYED */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

static void OHOS_Window_HandleFocusChanged(_THIS, const SDL_Event *event)
{
    SDL_Window *window;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    if ((SDL_bool)(intptr_t)event->user.data1) {
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
    } else {
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
    }
}

static void OHOS_Window_HandleRectChanged(_THIS, const SDL_Event *event)
{
    SDL_Window *window = NULL;
    SDL_VideoDisplay *display = NULL;
    SDL_DisplayData *display_data = NULL;
    SDL_Rect *rect = NULL;
    Uint32 display_id = 0;
    int i;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }
    rect = (SDL_Rect *)event->user.data1;
    display_id = (Uint32)(uintptr_t)event->user.data2;

    /* The reported rectangle is relative to the display origin. */
    for (i = 0; i < SDL_GetNumVideoDisplays(); ++i) {
        display = SDL_GetDisplay(i);
        display_data = (SDL_DisplayData *)display->driverdata;
        if (display_data->id == display_id) {
            rect->x += display_data->x_offset;
            rect->y += display_data->y_offset;
            break;
        }
    }

    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MOVED, rect->x, rect->y);
    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, rect->w, rect->h);
}

static void OHOS_Window_HandleStatusChanged(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    int32_t status;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    status = (int32_t)(intptr_t)event->user.data1;

    switch (status) {
    case 2: /* MAXIMIZE */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MAXIMIZED, 0, 0);
        break;
    case 3: /* MINIMIZE */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
        break;
    case 4: /* FLOATING */
    case 5: /* SPLIT_SCREEN */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESTORED, 0, 0);
        break;
    default:
        /* UNDEFINED and FULL_SCREEN are left to SDL's own fullscreen bookkeeping. */
        break;
    }
}

static void OHOS_Window_HandleDisplayChanged(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    SDL_VideoDisplay *display;
    SDL_DisplayData *display_data;
    Uint32 display_id;
    int i;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    display_id = (Uint32)(uintptr_t)event->user.data1;

    for (i = 0; i < SDL_GetNumVideoDisplays(); ++i) {
        display = SDL_GetDisplay(i);
        display_data = (SDL_DisplayData *)display->driverdata;
        if (display_data->id == display_id) {
            window->display_index = (Uint32)i;
            break;
        }
    }
}

static void OHOS_Window_HandleVisibilityChanged(_THIS, const SDL_Event *event)
{
    SDL_Window *window;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    SDL_SendWindowEvent(window,
                        (SDL_bool)(intptr_t)event->user.data1 ? SDL_WINDOWEVENT_SHOWN
                                                              : SDL_WINDOWEVENT_HIDDEN,
                        0, 0);
}

static void OHOS_Window_HandleIdAssigned(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    SDL_WindowData *data;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    data = (SDL_WindowData *)window->driverdata;
    if (data == NULL) {
        return;
    }

    data->ohos_window_id = (int32_t)(intptr_t)event->user.data1;
}

void OHOS_Window_HandleEvent(_THIS, const SDL_Event *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->user.code) {
    case OHOS_WINDOW_EVENT_STATE_CHANGED:
        OHOS_Window_HandleStateChanged(_this, event);
        break;
    case OHOS_WINDOW_EVENT_RECT_CHANGED:
        OHOS_Window_HandleRectChanged(_this, event);
        break;
    case OHOS_WINDOW_EVENT_STATUS_CHANGED:
        OHOS_Window_HandleStatusChanged(_this, event);
        break;
    case OHOS_WINDOW_EVENT_DISPLAY_CHANGED:
        OHOS_Window_HandleDisplayChanged(_this, event);
        break;
    case OHOS_WINDOW_EVENT_VISIBILITY_CHANGED:
        OHOS_Window_HandleVisibilityChanged(_this, event);
        break;
    case OHOS_WINDOW_EVENT_FOCUS_CHANGED:
        OHOS_Window_HandleFocusChanged(_this, event);
        break;
    case OHOS_WINDOW_EVENT_ID_ASSIGNED:
        OHOS_Window_HandleIdAssigned(_this, event);
        break;
    default:
        break;
    }
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
