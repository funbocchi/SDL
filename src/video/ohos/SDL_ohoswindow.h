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

extern int OHOS_CreateWindow(_THIS, SDL_Window *window);
extern void OHOS_SetWindowTitle(_THIS, SDL_Window *window);
extern void OHOS_SetWindowFullScreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen);
extern void OHOS_SetWindowSize(_THIS, SDL_Window *window);
extern void OHOS_MinimizeWindow(_THIS,SDL_Window *window);
extern void OHOS_SetWindowResizable(_THIS<SDL_Window *window,SDL_bool resizable);

extern void OHOS_DestroyWindow(_THIS,SDL_Window*window);
extern SDL_bool OHOS_GetWindowMInfo(_THIS,SDL_Window *window,struct SDL_SysWMinfo *info);
extern SDL_Window *OHOS_Window;

typedef struct{
    #ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
    EGLContext egl_context;
    #endif
    SDL_bool backup_done;
    OHNativeWindow *native_window;
}SDL_WindowData;

#endif