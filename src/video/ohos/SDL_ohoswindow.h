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

#ifndef SDL_OHOSWINDOW_H
#define SDL_OHOSWINDOW_H

#include <native_window/external_window.h>

#include "SDL_events.h"
#include "SDL_mutex.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "../SDL_egl_c.h"
#endif

#include "../../core/ohos/SDL_ohos.h"

/* Forward declaration to avoid pulling the full ArkUI header into every includer */
typedef struct OH_ArkUI_SurfaceHolder OH_ArkUI_SurfaceHolder;
typedef struct OH_ArkUI_SurfaceCallback OH_ArkUI_SurfaceCallback;
struct OHOS_WindowEntry;

typedef struct
{
    Uint32 ref_count;
    SDL_atomic_t destroying;
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface current_surface;
    EGLSurface pending_surface;
    EGLSurface pbuffer_surface;
#endif
    Uint32 window_id;
    OHNativeWindow *native_window;

    SDL_mutex *create_lock;
    SDL_cond *create_cond;
    SDL_bool attach_complete;
    SDL_bool create_complete;
    SDL_bool done;
} SDL_WindowData;

extern void OHOS_Window_HandleEvents(_THIS, const SDL_Event *event);

/* Hooks for SDL2 */
extern int OHOS_CreateWindow(_THIS, SDL_Window *window);
extern int OHOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data);
extern void OHOS_SetWindowTitle(_THIS, SDL_Window *window);
extern void OHOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon);
extern void OHOS_SetWindowPosition(_THIS, SDL_Window *window);
extern void OHOS_SetWindowSize(_THIS, SDL_Window *window);
extern void OHOS_SetWindowMinimumSize(_THIS, SDL_Window *window);
extern void OHOS_SetWindowMaximumSize(_THIS, SDL_Window *window);
extern int OHOS_GetWindowBordersSize(_THIS, SDL_Window *window, int *top, int *left, int *bottom, int *right);
extern void OHOS_GetWindowSizeInPixels(_THIS, SDL_Window *window, int *w, int *h);
extern int OHOS_SetWindowOpacity(_THIS, SDL_Window *window, float opacity);
extern int OHOS_SetWindowModalFor(_THIS, SDL_Window *modal_window, SDL_Window *parent_window);
extern int OHOS_SetWindowInputFocus(_THIS, SDL_Window *window);
extern void OHOS_ShowWindow(_THIS, SDL_Window *window);
extern void OHOS_HideWindow(_THIS, SDL_Window *window);
extern void OHOS_RaiseWindow(_THIS, SDL_Window *window);
extern void OHOS_MaximizeWindow(_THIS, SDL_Window *window);
extern void OHOS_MinimizeWindow(_THIS, SDL_Window *window);
extern void OHOS_RestoreWindow(_THIS, SDL_Window *window);
extern void OHOS_SetWindowBordered(_THIS, SDL_Window *window, SDL_bool bordered);
extern void OHOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable);
extern void OHOS_SetWindowAlwaysOnTop(_THIS, SDL_Window *window, SDL_bool on_top);
extern void OHOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen);
extern int OHOS_SetWindowGammaRamp(_THIS, SDL_Window *window, const Uint16 *ramp);
extern int OHOS_GetWindowGammaRamp(_THIS, SDL_Window *window, Uint16 *ramp);
extern void *OHOS_GetWindowICCProfile(_THIS, SDL_Window *window, size_t *size);
extern int OHOS_GetWindowDisplayIndex(_THIS, SDL_Window *window);
extern void OHOS_SetWindowMouseRect(_THIS, SDL_Window *window);
extern void OHOS_SetWindowMouseGrab(_THIS, SDL_Window *window, SDL_bool grabbed);
extern void OHOS_SetWindowKeyboardGrab(_THIS, SDL_Window *window, SDL_bool grabbed);
extern void OHOS_DestroyWindow(_THIS, SDL_Window *window);
extern int OHOS_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch);
extern int OHOS_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects);
extern void OHOS_DestroyWindowFramebuffer(_THIS, SDL_Window *window);
extern void OHOS_OnWindowEnter(_THIS, SDL_Window *window);
extern int OHOS_FlashWindow(_THIS, SDL_Window *window, SDL_FlashOperation operation);

/* Napi */
extern napi_value OHOS_ThreadSafe_RegisterCallWindowRequest(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_AttachWindowSurface(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_DetachWindowSurface(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowReady(napi_env env, napi_callback_info info);

/* * * */
/**
 * Others
 */

extern SDL_bool OHOS_Tool_HasFlag(Uint32 flags, Uint32 mask);

extern int OHOS_Tool_LockWindowData(SDL_WindowData *data);

extern int OHOS_Tool_UnlockWidnowData(SDL_WindowData *data);

extern int OHOS_Window_WaitSurfaceReady(SDL_WindowData *data, Sint32 timeout_ms);

extern void OHOS_Window_PumpSurfaceState(_THIS);

#endif /* SDL_OHOSWINDOW_H */

/* vi: set ts=4 sw=4 expandtab: */