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

#include <napi/native_api.h>

#include "../SDL_sysvideo.h"
#include "SDL_egl.h"
#include "SDL_events.h"
#include "SDL_mutex.h"

typedef enum OHOS_ArkUIWindowEventType
{
    OHOS_ARKUI_WINDOW_EVENT_SHOWN = 1,
    OHOS_ARKUI_WINDOW_EVENT_ACTIVE = 2,
    OHOS_ARKUI_WINDOW_EVENT_INACTIVE = 3,
    OHOS_ARKUI_WINDOW_EVENT_HIDDEN = 4,
    OHOS_ARKUI_WINDOW_EVENT_DESTROYED = 7
} OHOS_ArkUIWindowEventType;

typedef enum OHOS_ArkUIWindowStatusType
{
    OHOS_ARKUI_WINDOW_STATUS_UNDEFINED = 0,
    OHOS_ARKUI_WINDOW_STATUS_FULL_SCREEN = 1,
    OHOS_ARKUI_WINDOW_STATUS_MAXIMIZE = 2,
    OHOS_ARKUI_WINDOW_STATUS_MINIMIZE = 3,
    OHOS_ARKUI_WINDOW_STATUS_FLOATING = 4,
    OHOS_ARKUI_WINDOW_STATUS_SPLIT_SCREEN = 5
} OHOS_ArkUIWindowStatusType;

/* Placeholder until the OHOS window backend is rebuilt. */
typedef struct
{
    int ref_count;
    SDL_bool destroying;

    SDL_atomic_t create_done;
    SDL_atomic_t ohos_window_id;
    SDL_sem *create_sem;

    SDL_atomic_t sdl_destroying;
    SDL_atomic_t platform_destroyed;
} SDL_WindowData;

extern napi_value OHOS_Napi_AttachWindowSurface(napi_env env, napi_callback_info info);
extern napi_value OHOS_WindowRequest_RegisterCallback(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_DetachWindowSurface(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowReady(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowDestroyed(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowEvent(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowStatusChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowStatusDidChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowSizeChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowRectChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowVisibilityChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowAvoidAreaChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowSystemAvoidAreaChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowKeyboardHeightChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowKeyboardWillShow(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowKeyboardDidShow(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowKeyboardWillHide(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowKeyboardDidHide(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowTouchOutside(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowDisplayIdChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowSystemDensityChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowNoInteractionDetected(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowScreenshot(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowScreenshotAppEvent(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowDialogTargetTouch(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowHighlightChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowTitleButtonRectChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowFreeWindowModeChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowOcclusionStateChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowFrameMetricsMeasured(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowRotationChange(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowWillClose(napi_env env, napi_callback_info info);
extern napi_value OHOS_Napi_NotifyWindowSubWindowClose(napi_env env, napi_callback_info info);

/* Window hooks, in SDL_VideoDevice declaration order. */
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

/* Backend-internal hook used by OHOS_PumpEvents. */
extern void OHOS_Window_HandleEvents(_THIS, const SDL_Event *event);

#endif /* SDL_OHOSWINDOW_H */

/* vi: set ts=4 sw=4 expandtab: */
