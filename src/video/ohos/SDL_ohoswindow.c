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

#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"

int OHOS_CreateSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    OHOS_WindowData *data;
    OHOSWindowRequestParam *params;
    int retval = 0;

    if (!ohos_window_mutex) {
        ohos_window_mutex = SDL_CreateMutex();
    }

    SDL_LockMutex(ohos_window_mutex);

    data = (OHOS_WindowData *)SDL_calloc(1, sizeof(OHOS_WindowData));
    if (!data) {
        retval = SDL_OutOfMemory();
        goto endfunc;
    }

    params = (OHOSWindowRequestParam *)SDL_calloc(1, sizeof(OHOSWindowRequestParam));
    if (!params) {
        retval = SDL_OutOfMemory();
        SDL_free(data);
        goto endfunc;
    }

    params->title = SDL_strdup(window->title ? window->title : "");
    if (!params->title) {
        SDL_free(params);
        SDL_free(data);
        retval = SDL_OutOfMemory();
        goto endfunc;
    }
    params->id = window->id;
    params->x = window->x;
    params->y = window->y;
    params->w = window->w;
    params->h = window->h;

    params->fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? SDL_TRUE : SDL_FALSE;
    params->borderless = (window->flags & SDL_WINDOW_BORDERLESS) ? SDL_TRUE : SDL_FALSE;

    retval = SDL_OHOS_RequestWindow(params);

#ifdef SDL_VIDEO_OPENGL_EGL
#endif

endfunc:
    SDL_UnlockMutex(ohos_window_mutex);
    return retval;
}

int OHOS_CreateSDLWindowFrom(SDL_VideoDevice *_this, SDL_Window *window, const void *data) {}

void OHOS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window) {}

void OHOS_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window) {}

void OHOS_SetWindowIcon(SDL_VideoDevice *_this, SDL_Window *window, SDL_Surface *icon) {}

void OHOS_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window) {}

void OHOS_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window) {}

#endif
/* vi: set ts=4 sw=4 expandtab: */