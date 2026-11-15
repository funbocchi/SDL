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

#if defined(SDL_VIDEO_DRIVER_OHOS) && defined(SDL_VIDEO_OPENGL_EGL)

#include "SDL_ohosgles.h"

#include "../SDL_egl_c.h"
#include "SDL_ohoswindow.h"
#include "../../core/ohos/SDL_ohos.h"

/* GL context table */

#define OHOS_GL_OFFSCREEN_WIDTH  1280
#define OHOS_GL_OFFSCREEN_HEIGHT 720

typedef struct OHOS_GLContextEntry
{
    SDL_GLContext              context;
    EGLSurface                 pbuffer;
    struct OHOS_GLContextEntry *next;
} OHOS_GLContextEntry;

static OHOS_GLContextEntry *g_ohos_gl_context_entries = NULL;

static OHOS_GLContextEntry *OHOS_GLContext_Alloc(_THIS)
{
    OHOS_GLContextEntry *entry = NULL;

    entry = (OHOS_GLContextEntry *)SDL_calloc(1, sizeof(*entry));
    if (entry == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }
    entry->pbuffer = SDL_EGL_CreateOffscreenSurface(_this, OHOS_GL_OFFSCREEN_WIDTH, OHOS_GL_OFFSCREEN_HEIGHT);
    if (entry->pbuffer == EGL_NO_SURFACE) {
        SDL_free(entry);
        return NULL;
    }
    return entry;
}

static void OHOS_GLContext_Link(OHOS_GLContextEntry *entry, SDL_GLContext context)
{
    entry->context = context;
    entry->next = g_ohos_gl_context_entries;
    g_ohos_gl_context_entries = entry;
}

static EGLSurface OHOS_GLContext_GetPbuffer(SDL_GLContext context)
{
    OHOS_GLContextEntry *entry = g_ohos_gl_context_entries;

    while (entry->context != context) {
        entry = entry->next;
    }
    return entry->pbuffer;
}

static void OHOS_GLContext_Destroy(_THIS, SDL_GLContext context)
{
    OHOS_GLContextEntry *entry = g_ohos_gl_context_entries;
    OHOS_GLContextEntry *prev = NULL;

    while (entry->context != context) {
        prev = entry;
        entry = entry->next;
    }
    if (prev == NULL) {
        g_ohos_gl_context_entries = entry->next;
    } else {
        prev->next = entry->next;
    }
    SDL_EGL_DestroySurface(_this, entry->pbuffer);
    SDL_free(entry);
}

static void OHOS_GL_HandleSurfaceCreated(_THIS, const SDL_Event *event)
{
    SDL_Window *window = NULL;
    SDL_WindowData *data = NULL;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }
    data = (SDL_WindowData *)window->driverdata;
    if (data == NULL) {
        return;
    }
    data->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType)event->user.data1);
    SDL_Log("OHOS surface created: window=%u native=%p egl=%p", event->user.windowID,
            event->user.data1, (void *)data->egl_surface);

    /* The context usually exists by now and is current with the pbuffer, so move it
     * over to the real surface right away instead of waiting for a resize. */
    if (data->egl_surface != EGL_NO_SURFACE && SDL_GL_GetCurrentWindow() == window) {
        if (SDL_EGL_MakeCurrent(_this, data->egl_surface, SDL_GL_GetCurrentContext()) == 0) {
            data->using_pbuffer = SDL_FALSE;
        }
    }
}

static void OHOS_GL_HandleSurfaceChanged(_THIS, const SDL_Event *event)
{
    SDL_Window *window = NULL;
    SDL_WindowData *data = NULL;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }
    data = (SDL_WindowData *)window->driverdata;
    if (data == NULL) {
        return;
    }
    data->drawable_width = (int)(uintptr_t)event->user.data1;
    data->drawable_height = (int)(uintptr_t)event->user.data2;
    if (SDL_GL_GetCurrentWindow() != window) {
        return;
    }
    if (data->using_pbuffer) {
        SDL_EGL_MakeCurrent(_this, data->egl_surface, SDL_GL_GetCurrentContext());
        data->using_pbuffer = SDL_FALSE;
    }
}

static void OHOS_GL_HandleSurfaceDestroyed(_THIS, const SDL_Event *event)
{
    SDL_Window *window = NULL;
    SDL_WindowData *data = NULL;
    SDL_GLContext context = NULL;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }
    data = (SDL_WindowData *)window->driverdata;
    if (data == NULL) {
        return;
    }
    if (SDL_GL_GetCurrentWindow() == window) {
        context = SDL_GL_GetCurrentContext();
        if (context != NULL) {
            SDL_EGL_MakeCurrent(_this, OHOS_GLContext_GetPbuffer(context), context);
            data->using_pbuffer = SDL_TRUE;
        }
    }
    if (data->egl_surface != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, data->egl_surface);
        data->egl_surface = EGL_NO_SURFACE;
    }
}

void OHOS_GL_HandleEvent(_THIS, const SDL_Event *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->user.code) {
    case OHOS_GL_EVENT_SURFACE_CREATED:
        OHOS_GL_HandleSurfaceCreated(_this, event);
        break;
    case OHOS_GL_EVENT_SURFACE_CHANGED:
        OHOS_GL_HandleSurfaceChanged(_this, event);
        break;
    case OHOS_GL_EVENT_SURFACE_DESTROYED:
        OHOS_GL_HandleSurfaceDestroyed(_this, event);
        break;
    default:
        break;
    }
}

/* OpenGL hooks, in SDL_VideoDevice declaration order. */

int OHOS_GL_LoadLibrary(_THIS, const char *path)
{
    int retval = SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType)EGL_DEFAULT_DISPLAY, EGL_PLATFORM_OHOS_KHR);

    if (retval != 0) {
        return retval;
    }

    _this->egl_data->egl_surfacetype = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

    if (_this->gl_config.depth_size < 8) {
        _this->gl_config.depth_size = 8;
    }

    return 0;
}

void *OHOS_GL_GetProcAddress(_THIS, const char *proc)
{
    return SDL_EGL_GetProcAddress(_this, proc);
}

void OHOS_GL_UnloadLibrary(_THIS)
{
    SDL_EGL_UnloadLibrary(_this);
}

int OHOS_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    SDL_WindowData *data = NULL;
    EGLSurface pbuffer = EGL_NO_SURFACE;
    EGLSurface surface = EGL_NO_SURFACE;

    if (window == NULL || context == NULL) {
        return SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    data = (SDL_WindowData *)window->driverdata;
    if (data == NULL) {
        return SDL_SetError("OHOS GL: no window data");
    }
    pbuffer = OHOS_GLContext_GetPbuffer(context);
    surface = (data->egl_surface != EGL_NO_SURFACE) ? data->egl_surface : pbuffer;
    if (surface == EGL_NO_SURFACE) {
        return SDL_SetError("OHOS GL: no surface to make current");
    }
    if (SDL_EGL_MakeCurrent(_this, surface, context) < 0) {
        return -1;
    }
    data->using_pbuffer = (surface == pbuffer);
    return 0;
}

SDL_GLContext OHOS_GL_CreateContext(_THIS, SDL_Window *window)
{
    SDL_GLContext context = NULL;
    OHOS_GLContextEntry *entry = NULL;

    /* SDL_EGL_CreateContext makes the context current and probes GL extensions
     * right away, and a context without a surface reports none of them, so the
     * pbuffer has to exist before the context does. */
    entry = OHOS_GLContext_Alloc(_this);
    if (entry == NULL) {
        return NULL;
    }
    context = SDL_EGL_CreateContext(_this, entry->pbuffer);
    if (context == NULL) {
        SDL_EGL_DestroySurface(_this, entry->pbuffer);
        SDL_free(entry);
        return NULL;
    }
    OHOS_GLContext_Link(entry, context);
    if (OHOS_GL_MakeCurrent(_this, window, context) < 0) {
        OHOS_GLContext_Destroy(_this, context);
        SDL_EGL_DeleteContext(_this, context);
        return NULL;
    }
    return context;
}

void OHOS_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

    (void)_this;

    if (w != NULL) {
        *w = data->drawable_width;
    }
    if (h != NULL) {
        *h = data->drawable_height;
    }
}

int OHOS_GL_SetSwapInterval(_THIS, int interval)
{
    return SDL_EGL_SetSwapInterval(_this, interval);
}

int OHOS_GL_GetSwapInterval(_THIS)
{
    return SDL_EGL_GetSwapInterval(_this);
}

int OHOS_GL_SwapWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

    if (data->egl_surface == EGL_NO_SURFACE) {
        /* Nothing to present to yet: keep running against the pbuffer. */
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OHOS swap without a window surface");
        return 0;
    }
    if (data->using_pbuffer) {
        data->using_pbuffer = SDL_FALSE;
        SDL_EGL_MakeCurrent(_this, data->egl_surface, SDL_GL_GetCurrentContext());
    }
    return SDL_EGL_SwapBuffers(_this, data->egl_surface);
}

void OHOS_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    OHOS_GLContext_Destroy(_this, context);
    SDL_EGL_DeleteContext(_this, context);
}

void OHOS_GL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor)
{
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
}

#endif /* SDL_VIDEO_DRIVER_OHOS && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
