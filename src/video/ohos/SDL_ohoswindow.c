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

#include "SDL_ohosevents.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "SDL_ohosgles.h"
#endif

typedef struct OHOS_WindowRegistryEntry
{
    Uint32 id;
    SDL_WindowData *data;
    struct OHOS_WindowRegistryEntry *next;
} OHOS_WindowRegistryEntry;

static SDL_mutex *g_ohos_window_registry_lock = NULL;
static OHOS_WindowRegistryEntry *g_ohos_window_registry_list = NULL;

static void OHOS_WindowRegistry_Release(SDL_WindowData *data) {}
static int OHOS_WindowRegistry_Register(SDL_WindowData *data, Uint32 id) {}
static void OHOS_WindowRegistry_Unregister(Uint32 id) {}
static SDL_WindowData *OHOS_WindowRegistry_Acquire(Uint32 id) {}

static void OHOS_SurfaceHolder_OnCreatedCB(OH_ArkUI_SurfaceHolder *holder)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    OHOS_GLWindowSurface *surface;
    OHNativeWindow *native_window;
    SDL_Event event;
    Uint32 id;
    Uint32 generation;

    if (!holder) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        return;
    }

    native_window = OH_ArkUI_XComponent_GetNativeWindow(holder);
    if (!native_window) {
        return;
    }

    surface = OHOS_GLRegistry_Acquire(id);
    if (!surface) {
        return;
    }

    generation = (Uint32)SDL_AtomicAdd(&surface->surface_generation, 1) + 1;
    SDL_AtomicSet(&surface->native_window_valid, SDL_TRUE);
    OHOS_GLRegistry_Release(surface);

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_CREATED;
    event.user.windowID = id;
    event.user.data1 = (void *)(uintptr_t)generation;
    event.user.data2 = (void *)native_window;
    SDL_PushEvent(&event);
#endif
}

static void OHOS_SurfaceHolder_OnChangedCB(OH_ArkUI_SurfaceHolder *holder, Uint64 width, Uint64 height) {}
static void OHOS_SurfaceHolder_OnDestroyedCB(OH_ArkUI_SurfaceHolder *holder)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    OHOS_GLWindowSurface *surface;
    SDL_Event event;
    Uint32 id;
    Uint32 generation;

    if (!holder) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        return;
    }

    surface = OHOS_GLRegistry_Acquire(id);
    if (!surface) {
        return;
    }

    SDL_AtomicSet(&surface->native_window_valid, SDL_FALSE);
    generation = (Uint32)SDL_AtomicGet(&surface->surface_generation);
    OHOS_GLRegistry_Release(surface);

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_DESTROYED;
    event.user.windowID = id;
    event.user.data1 = (void *)(uintptr_t)generation;
    event.user.data2 = NULL;
    SDL_PushEvent(&event);
#endif
}
static void OHOS_SurfaceHolder_OnShowCB(OH_ArkUI_SurfaceHolder *holder)
{
    Uint32 id;
    SDL_WindowData *data = NULL;
    SDL_Event event;

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Incorrect ID");
        return;
    }
    SDL_LockMutex(g_ohos_window_registry_lock);
    data = OHOS_WindowRegistry_Acquire(id);
    SDL_UnlockMutex(g_ohos_window_registry_lock);
    if (data == NULL) {
        return;
    }
    SDL_AtomicSet(&data->visible, SDL_TRUE);
    OHOS_WindowRegistry_Release(data);
    data = NULL;
    SDL_PushEvent(&event);
}
static void OHOS_SurfaceHolder_OnHideCB(OH_ArkUI_SurfaceHolder *holder) {}

napi_value OHOS_Napi_AttachNode(napi_env env, napi_callback_info info) {}
napi_value OHOS_Napi_DetachNode(napi_env env, napi_callback_info info) {}

/* Window hooks, in SDL_VideoDevice declaration order. */

int OHOS_CreateWindow(_THIS, SDL_Window *window)
{
    return 0;
}

int OHOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    return 0;
}

void OHOS_SetWindowTitle(_THIS, SDL_Window *window)
{
}

void OHOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon)
{
}

void OHOS_SetWindowPosition(_THIS, SDL_Window *window)
{
}

void OHOS_SetWindowSize(_THIS, SDL_Window *window)
{
}

void OHOS_SetWindowMinimumSize(_THIS, SDL_Window *window)
{
}

void OHOS_SetWindowMaximumSize(_THIS, SDL_Window *window)
{
}

int OHOS_GetWindowBordersSize(_THIS, SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
    return 0;
}

void OHOS_GetWindowSizeInPixels(_THIS, SDL_Window *window, int *w, int *h)
{
}

int OHOS_SetWindowOpacity(_THIS, SDL_Window *window, float opacity)
{
    return 0;
}

int OHOS_SetWindowModalFor(_THIS, SDL_Window *modal_window, SDL_Window *parent_window)
{
    return 0;
}

int OHOS_SetWindowInputFocus(_THIS, SDL_Window *window)
{
    return 0;
}

void OHOS_ShowWindow(_THIS, SDL_Window *window)
{
}

void OHOS_HideWindow(_THIS, SDL_Window *window)
{
}

void OHOS_RaiseWindow(_THIS, SDL_Window *window)
{
}

void OHOS_MaximizeWindow(_THIS, SDL_Window *window)
{
}

void OHOS_MinimizeWindow(_THIS, SDL_Window *window)
{
}

void OHOS_RestoreWindow(_THIS, SDL_Window *window)
{
}

void OHOS_SetWindowBordered(_THIS, SDL_Window *window, SDL_bool bordered)
{
}

void OHOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable)
{
}

void OHOS_SetWindowAlwaysOnTop(_THIS, SDL_Window *window, SDL_bool on_top)
{
}

void OHOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
}

int OHOS_SetWindowGammaRamp(_THIS, SDL_Window *window, const Uint16 *ramp)
{
    return 0;
}

int OHOS_GetWindowGammaRamp(_THIS, SDL_Window *window, Uint16 *ramp)
{
    return 0;
}

void *OHOS_GetWindowICCProfile(_THIS, SDL_Window *window, size_t *size)
{
    return NULL;
}

int OHOS_GetWindowDisplayIndex(_THIS, SDL_Window *window)
{
    return 0;
}

void OHOS_SetWindowMouseRect(_THIS, SDL_Window *window)
{
}

void OHOS_SetWindowMouseGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
}

void OHOS_SetWindowKeyboardGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
}

void OHOS_DestroyWindow(_THIS, SDL_Window *window)
{
}

int OHOS_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    return 0;
}

int OHOS_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    return 0;
}

void OHOS_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
}

void OHOS_OnWindowEnter(_THIS, SDL_Window *window)
{
}

int OHOS_FlashWindow(_THIS, SDL_Window *window, SDL_FlashOperation operation)
{
    return 0;
}

/* Backend-internal hook used by OHOS_PumpEvents. */
void OHOS_Window_HandleEvents(_THIS, const SDL_Event *event)
{
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
