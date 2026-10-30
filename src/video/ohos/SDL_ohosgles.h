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

#ifndef SDL_OHOSGLES_H
#define SDL_OHOSGLES_H

#if defined(SDL_VIDEO_DRIVER_OHOS) && defined(SDL_VIDEO_OPENGL_EGL)

#include "../SDL_egl_c.h"
#include "../SDL_sysvideo.h"
#include "SDL_atomic.h"
#include "SDL_events.h"

typedef struct OHOS_GLWindowSurface
{
    Uint32 window_id;

    int ref_count;
    SDL_bool destroying;

    SDL_atomic_t surface_generation;
    SDL_atomic_t surface_visible;
    SDL_atomic_t window_surface_ready;
    SDL_atomic_t drawable_width;
    SDL_atomic_t drawable_height;

    EGLSurface offscreen_surface;
    EGLSurface window_surface;
    Uint32 window_surface_generation;

    struct OHOS_GLWindowSurface *next;
} OHOS_GLWindowSurface;

/* OpenGL hooks, in SDL_VideoDevice declaration order. */

extern int OHOS_GL_LoadLibrary(_THIS, const char *path);
extern void *OHOS_GL_GetProcAddress(_THIS, const char *proc);
extern void OHOS_GL_UnloadLibrary(_THIS);
extern SDL_GLContext OHOS_GL_CreateContext(_THIS, SDL_Window *window);
extern int OHOS_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context);
extern void OHOS_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h);
extern int OHOS_GL_SetSwapInterval(_THIS, int interval);
extern int OHOS_GL_GetSwapInterval(_THIS);
extern int OHOS_GL_SwapWindow(_THIS, SDL_Window *window);
extern void OHOS_GL_DeleteContext(_THIS, SDL_GLContext context);
extern void OHOS_GL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor);

typedef enum OHOS_GLSurfaceEventCode
{
    OHOS_GL_EVENT_SURFACE_CREATED = 0,
    OHOS_GL_EVENT_SURFACE_DESTROYED,
    OHOS_GL_EVENT_SURFACE_CHANGED,
    OHOS_GL_EVENT_SURFACE_SHOWN,
    OHOS_GL_EVENT_SURFACE_HIDDEEN
} OHOS_GLSurfaceEventCode;

extern void OHOS_GL_HandleEvents(_THIS, const SDL_Event *event);

/* Surface registry */

extern int OHOS_GLRegistry_Register(Uint32 id);
extern void OHOS_GLRegistry_Unregister(Uint32 id);
extern OHOS_GLWindowSurface *OHOS_GLRegistry_Acquire(Uint32 id);
extern void OHOS_GLRegistry_Release(OHOS_GLWindowSurface *surface);

#endif /* SDL_VIDEO_DRIVER_OHOS && SDL_VIDEO_OPENGL_EGL */

#endif /* SDL_OHOSGLES_H */

/* vi: set ts=4 sw=4 expandtab: */
