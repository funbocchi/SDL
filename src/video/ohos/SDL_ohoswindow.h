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

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_egl_c.h"

#include <native_window/external_window.h>

extern SDL_Window *OHOS_Window;

extern int OHOS_CreateSDLWindow(_THIS, SDL_Window *window);
/* extern int OHOS_CreateSDLWindowFrom(_THIS, SDL_Window *window, const void *data); */
extern void OHOS_SetWindowTitle(_THIS, SDL_Window *window);
/**
* @brief 仅针对2in1设备生效
*/
extern void OHOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon);
extern void OHOS_SetWindowPosition(_THIS, SDL_Window *window);
extern void OHOS_SetWindowSize(_THIS, SDL_Window *window);
/**
* @brief window.setWindowLayoutFrozen
*/
extern void OHOS_SetWindowMinimumSize(_THIS, SDL_Window *window);
/**
* @brief window.setWindowLayoutFrozen
*/
extern void OHOS_SetWindowMaximumSize(_THIS, SDL_Window *window);
extern int OHOS_GetWindowBordersSize(_THIS, SDL_Window *window, int *top, int *left, int *bottom, int *right);
extern void OHOS_GetWindowSizeInPixels(_THIS, SDL_Window *window, int *w, int *h);
/* extern int OHOS_SetWindowOpacity(_THIS, SDL_Window *window, float opacity); */
/**
* @brief 将一个窗口设为另一个的模态窗口
*/
extern int OHOS_SetWindowModalFor(_THIS, SDL_Window *modal_window, SDL_Window *parent_window);
/**
* @brief 设置为输入焦点
*/
extern int OHOS_SetWindowInputFocus(_THIS, SDL_Window *window);

extern void OHOS_ShowWindow(_THIS, SDL_Window *window);
extern void OHOS_HideWindow(_THIS, SDL_Window *window);
extern void OHOS_RaiseWindow(_THIS, SDL_Window *window);
extern void OHOS_MaximizeWindow(_THIS, SDL_Window *window);
extern void OHOS_MinimizeWindow(_THIS, SDL_Window *window);
/**
* @brief 还原
*/
extern void OHOS_RestoreWindow(_THIS, SDL_Window *window);
/**
* @brief 控制是否现实标题栏
*/
extern void OHOS_SetWindowBordered(_THIS, SDL_Window *window, SDL_bool bordered);
/**
* @brief 控制是否可以拖拽缩放
*/
extern void OHOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable);
/**
* @brief 是否置顶
*/
extern void OHOS_SetWindowAlwaysOnTop(_THIS, SDL_Window *window, SDL_bool on_top);
/**
* @brief 全屏|窗口切换
*/
extern void OHOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen);
/* extern int OHOS_SetWindowGammaRamp(_THIS, SDL_Window *window, const Uint16 *ramp); */
/* extern int OHOS_GetWindowGammaRamp(_THIS, SDL_Window *window, Uint16 *ramp); */
/* extern void *OHOS_GetWindowICCProfile(_THIS, SDL_Window *window, size_t *size); */
/**
* @brief 获取当前窗口索引
*/
extern int OHOS_GetWindowDisplayIndex(_THIS, SDL_Window *window);
/**
* @brief 限制鼠标活动范围
*/
extern void OHOS_SetWindowMouseRect(_THIS, SDL_Window *window);
/**
* @brief 捕获鼠标到窗口内
*/
extern void OHOS_SetWindowMouseGrab(_THIS, SDL_Window *window, SDL_bool grabbed);
/**
* @brief 捕获键盘输入到窗口
*/
extern void OHOS_SetWindowKeyboardGrab(_THIS, SDL_Window *window, SDL_bool grabbed);
extern void OHOS_DestroyWindow(_THIS, SDL_Window *window);
extern int OHOS_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch);
extern int OHOS_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects);
extern void OHOS_DestroyWindowFramebuffer(_THIS, SDL_Window *window);
/**
* @brief 鼠标进入窗口区域时通知
*/
extern void OHOS_OnWindowEnter(_THIS, SDL_Window *window);
/**
* @brief 任务栏图标闪烁
*/
extern int OHOS_FlashWindow(_THIS, SDL_Window *window, SDL_FlashOperation operation);

typedef struct
{
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
    EGLContext egl_context;
#endif
    SDL_bool backup_done;
    OHNativeWindow *native_window;
} SDL_WindowData;

#endif