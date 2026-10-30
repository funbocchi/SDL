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

#define OHOS_GL_OFFSCREEN_WIDTH  1280
#define OHOS_GL_OFFSCREEN_HEIGHT 720

/* Surface registry */

static SDL_mutex *g_ohos_gl_registry_lock = NULL;
static OHOS_GLWindowSurface *g_ohos_gl_window_surfaces = NULL;

static void OHOS_GLRegistry_FreeEntry(OHOS_GLWindowSurface *surface);

int OHOS_GLRegistry_Register(Uint32 id)
{
    OHOS_GLWindowSurface *entry;
    OHOS_GLWindowSurface *it;

    if (id == 0) {
        return SDL_InvalidParamError("id");
    }
    if (!g_ohos_gl_registry_lock) {
        return SDL_SetError("GL registry not initialized");
    }

    entry = (OHOS_GLWindowSurface *)SDL_calloc(1, sizeof(*entry));
    if (!entry) {
        return SDL_OutOfMemory();
    }

    SDL_LockMutex(g_ohos_gl_registry_lock);

    for (it = g_ohos_gl_window_surfaces; it; it = it->next) {
        if (it->window_id == id) {
            SDL_UnlockMutex(g_ohos_gl_registry_lock);
            SDL_free(entry);
            return SDL_SetError("GL registry id %u already registered", id);
        }
    }

    entry->window_id = id;
    entry->ref_count = 1;
    entry->destroying = SDL_FALSE;
    entry->offscreen_surface = EGL_NO_SURFACE;
    entry->window_surface = EGL_NO_SURFACE;
    entry->window_surface_generation = 0;
    SDL_AtomicSet(&entry->surface_generation, 0);
    SDL_AtomicSet(&entry->surface_visible, SDL_FALSE);
    SDL_AtomicSet(&entry->window_surface_ready, SDL_FALSE);
    SDL_AtomicSet(&entry->drawable_width, 0);
    SDL_AtomicSet(&entry->drawable_height, 0);
    entry->next = g_ohos_gl_window_surfaces;
    g_ohos_gl_window_surfaces = entry;

    SDL_UnlockMutex(g_ohos_gl_registry_lock);
    return 0;
}

void OHOS_GLRegistry_Unregister(Uint32 id)
{
    OHOS_GLWindowSurface *surface;
    OHOS_GLWindowSurface *prev = NULL;

    if (id == 0 || !g_ohos_gl_registry_lock) {
        return;
    }

    SDL_LockMutex(g_ohos_gl_registry_lock);

    for (surface = g_ohos_gl_window_surfaces; surface; surface = surface->next) {
        if (surface->window_id == id) {
            break;
        }
        prev = surface;
    }

    if (!surface) {
        SDL_UnlockMutex(g_ohos_gl_registry_lock);
        return;
    }

    surface->destroying = SDL_TRUE;

    if (prev) {
        prev->next = surface->next;
    } else {
        g_ohos_gl_window_surfaces = surface->next;
    }

    SDL_UnlockMutex(g_ohos_gl_registry_lock);

    OHOS_GLRegistry_Release(surface);
}

OHOS_GLWindowSurface *OHOS_GLRegistry_Acquire(Uint32 id)
{
    OHOS_GLWindowSurface *surface;

    if (id == 0 || !g_ohos_gl_registry_lock) {
        return NULL;
    }

    SDL_LockMutex(g_ohos_gl_registry_lock);

    for (surface = g_ohos_gl_window_surfaces; surface; surface = surface->next) {
        if (surface->window_id == id) {
            if (surface->destroying) {
                SDL_UnlockMutex(g_ohos_gl_registry_lock);
                return NULL;
            }

            surface->ref_count++;
            SDL_UnlockMutex(g_ohos_gl_registry_lock);
            return surface;
        }
    }

    SDL_UnlockMutex(g_ohos_gl_registry_lock);
    return NULL;
}

void OHOS_GLRegistry_Release(OHOS_GLWindowSurface *surface)
{
    SDL_bool should_free = SDL_FALSE;

    if (!surface || !g_ohos_gl_registry_lock) {
        return;
    }

    SDL_LockMutex(g_ohos_gl_registry_lock);

    if (surface->ref_count == 0) {
        SDL_UnlockMutex(g_ohos_gl_registry_lock);
        SDL_assert(!"GL surface ref_count underflow");
        return;
    }

    surface->ref_count--;

    if (surface->ref_count == 0 && surface->destroying) {
        should_free = SDL_TRUE;
    }

    SDL_UnlockMutex(g_ohos_gl_registry_lock);

    if (should_free) {
        OHOS_GLRegistry_FreeEntry(surface);
    }
}

static void OHOS_GLRegistry_FreeEntry(OHOS_GLWindowSurface *surface)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (!surface) {
        return;
    }

    SDL_assert(surface->ref_count == 0);

    if (_this && _this->egl_data) {
        SDL_Window *window = SDL_GetWindowFromID(surface->window_id);

        if (window && SDL_GL_GetCurrentWindow() == window) {
            SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }

        if (surface->window_surface != EGL_NO_SURFACE &&
            surface->window_surface != surface->offscreen_surface) {
            SDL_EGL_DestroySurface(_this, surface->window_surface);
        }

        if (surface->offscreen_surface != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(_this, surface->offscreen_surface);
        }
    }

    surface->window_surface = EGL_NO_SURFACE;
    surface->offscreen_surface = EGL_NO_SURFACE;
    surface->window_surface_generation = 0;

    SDL_AtomicSet(&surface->surface_visible, SDL_FALSE);
    SDL_AtomicSet(&surface->window_surface_ready, SDL_FALSE);
    SDL_AtomicSet(&surface->surface_generation, 0);

    SDL_free(surface);
}

static void OHOS_GL_HandleSurfaceDestroyed(_THIS, const SDL_Event *event)
{
    OHOS_GLWindowSurface *surface;
    SDL_Window *window;
    SDL_GLContext context;
    Uint32 generation;

    if (!event) {
        return;
    }

    generation = (Uint32)(uintptr_t)event->user.data1;

    surface = OHOS_GLRegistry_Acquire(event->user.windowID);
    if (!surface) {
        return;
    }

    if (generation != (Uint32)SDL_AtomicGet(&surface->surface_generation)) {
        OHOS_GLRegistry_Release(surface);
        return;
    }
    
    SDL_AtomicSet(&surface->window_surface_ready, SDL_FALSE);

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window && SDL_GL_GetCurrentWindow() == window) {
        context = SDL_GL_GetCurrentContext();
        if (context) {
            if (surface->offscreen_surface != EGL_NO_SURFACE) {
                SDL_EGL_MakeCurrent(_this, surface->offscreen_surface, context);
            } else {
                SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
        }
    }

    if (surface->window_surface != EGL_NO_SURFACE &&
        surface->window_surface != surface->offscreen_surface) {
        SDL_EGL_DestroySurface(_this, surface->window_surface);
    }

    surface->window_surface = EGL_NO_SURFACE;
    surface->window_surface_generation = 0;
    SDL_AtomicSet(&surface->surface_visible, SDL_FALSE);

    OHOS_GLRegistry_Release(surface);
}

static void OHOS_GL_HandleSurfaceCreated(_THIS, const SDL_Event *event)
{
    OHOS_GLWindowSurface *surface;
    EGLSurface new_surface;
    NativeWindowType native_window;
    Uint32 generation;

    if (!event) {
        return;
    }

    generation = (Uint32)(uintptr_t)event->user.data1;
    native_window = (NativeWindowType)event->user.data2;

    surface = OHOS_GLRegistry_Acquire(event->user.windowID);
    if (!surface) {
        return;
    }

    if (generation != (Uint32)SDL_AtomicGet(&surface->surface_generation)) {
        OHOS_GLRegistry_Release(surface);
        return;
    }

    new_surface = SDL_EGL_CreateSurface(_this, native_window);
    if (new_surface == EGL_NO_SURFACE) {
        OHOS_GLRegistry_Release(surface);
        return;
    }

    if (generation != (Uint32)SDL_AtomicGet(&surface->surface_generation)) {
        SDL_EGL_DestroySurface(_this, new_surface);
        OHOS_GLRegistry_Release(surface);
        return;
    }

    if (surface->window_surface != EGL_NO_SURFACE &&
        surface->window_surface != surface->offscreen_surface) {
        SDL_EGL_DestroySurface(_this, surface->window_surface);
    }

    surface->window_surface = new_surface;
    surface->window_surface_generation = generation;
    SDL_AtomicSet(&surface->window_surface_ready, SDL_FALSE);
    SDL_AtomicSet(&surface->surface_visible, SDL_TRUE);

    OHOS_GLRegistry_Release(surface);
}

static void OHOS_GL_HandleSurfaceChanged(_THIS, const SDL_Event *event)
{
    OHOS_GLWindowSurface *surface;
    SDL_Window *window;
    SDL_GLContext context;
    Uint32 generation;

    if (!event) {
        return;
    }

    generation = (Uint32)(uintptr_t)event->user.data1;

    surface = OHOS_GLRegistry_Acquire(event->user.windowID);
    if (!surface) {
        return;
    }

    if (generation != (Uint32)SDL_AtomicGet(&surface->surface_generation) ||
        !SDL_AtomicGet(&surface->window_surface_ready) ||
        surface->window_surface == EGL_NO_SURFACE ||
        surface->window_surface_generation != generation) {
        OHOS_GLRegistry_Release(surface);
        return;
    }

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window && SDL_GL_GetCurrentWindow() == window) {
        context = SDL_GL_GetCurrentContext();
        if (context && SDL_EGL_MakeCurrent(_this, surface->window_surface, context) < 0) {
            OHOS_GLRegistry_Release(surface);
            return;
        }
    }

    OHOS_GLRegistry_Release(surface);
}

void OHOS_GL_HandleEvents(_THIS, const SDL_Event *event)
{
    if (!event) {
        return;
    }

    switch (event->user.code) {
    case OHOS_GL_EVENT_SURFACE_CREATED:
        OHOS_GL_HandleSurfaceCreated(_this, event);
        break;
    case OHOS_GL_EVENT_SURFACE_DESTROYED:
        OHOS_GL_HandleSurfaceDestroyed(_this, event);
        break;
    case OHOS_GL_EVENT_SURFACE_CHANGED:
        OHOS_GL_HandleSurfaceChanged(_this, event);
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

    if (!g_ohos_gl_registry_lock) {
        g_ohos_gl_registry_lock = SDL_CreateMutex();
        if (!g_ohos_gl_registry_lock) {
            SDL_EGL_UnloadLibrary(_this);
            return SDL_OutOfMemory();
        }
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

    if (g_ohos_gl_registry_lock) {
        SDL_DestroyMutex(g_ohos_gl_registry_lock);
        g_ohos_gl_registry_lock = NULL;
    }
}

SDL_GLContext OHOS_GL_CreateContext(_THIS, SDL_Window *window)
{
    OHOS_GLWindowSurface *entry = NULL;
    EGLSurface surface = EGL_NO_SURFACE;
    SDL_GLContext context = NULL;

    entry = OHOS_GLRegistry_Acquire(window->id);
    if (!entry) {
        SDL_SetError("OHOS GL window surface not registered");
        return NULL;
    }
    if (entry->window_surface != EGL_NO_SURFACE &&
        entry->window_surface_generation == (Uint32)SDL_AtomicGet(&entry->surface_generation) &&
        SDL_AtomicGet(&entry->window_surface_ready)) {
        surface = entry->window_surface;
    } else {
        if (entry->offscreen_surface == EGL_NO_SURFACE) {
            surface = SDL_EGL_CreateOffscreenSurface(_this, OHOS_GL_OFFSCREEN_WIDTH, OHOS_GL_OFFSCREEN_HEIGHT);
            if (surface == EGL_NO_SURFACE) {
                OHOS_GLRegistry_Release(entry);
                return EGL_NO_CONTEXT;
            }
            entry->offscreen_surface = surface;
        }
        surface = entry->offscreen_surface;
    }
    context = SDL_EGL_CreateContext(_this, surface);
    OHOS_GLRegistry_Release(entry);
    return context;
}

int OHOS_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    EGLSurface surface = EGL_NO_SURFACE;
    int retval = 0;

    SDL_assert(_this != NULL);
    if (!_this->egl_data) {
        if (SDL_EGL_LoadLibrary(_this, NULL, EGL_DEFAULT_DISPLAY, 0) < 0) {
            SDL_EGL_UnloadLibrary(_this);
            return -1;
        }
        _this->gl_config.driver_loaded = 1;
    }

    if (window != NULL) {
        OHOS_GLWindowSurface *entry = NULL;
        entry = OHOS_GLRegistry_Acquire(window->id);
        if (!entry) {
            return SDL_SetError("OHOS GL entry not found");
        }
        if (entry->window_surface != EGL_NO_SURFACE &&
            SDL_AtomicGet(&entry->window_surface_ready) &&
            entry->window_surface_generation == (Uint32)SDL_AtomicGet(&entry->surface_generation)) {
            surface = entry->window_surface;
        } else {
            if (entry->offscreen_surface == EGL_NO_SURFACE) {
                surface = SDL_EGL_CreateOffscreenSurface(_this, OHOS_GL_OFFSCREEN_WIDTH, OHOS_GL_OFFSCREEN_HEIGHT);
                if (surface == EGL_NO_SURFACE) {
                    OHOS_GLRegistry_Release(entry);
                    return -1;
                }
                entry->offscreen_surface = surface;
            } else {
                surface = entry->offscreen_surface;
            }
        }
        OHOS_GLRegistry_Release(entry);
    }
    retval = SDL_EGL_MakeCurrent(_this, surface, context);
    return retval;
}

void OHOS_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h)
{
    OHOS_GLWindowSurface *surface;
    int width = 0;
    int height = 0;

    if (window) {
        surface = OHOS_GLRegistry_Acquire(window->id);
        if (surface) {
            width = SDL_AtomicGet(&surface->drawable_width);
            height = SDL_AtomicGet(&surface->drawable_height);
            OHOS_GLRegistry_Release(surface);
        }
    }

    if (width == 0 && window) {
        width = window->w;
    }
    if (height == 0 && window) {
        height = window->h;
    }

    if (w) {
        *w = width;
    }
    if (h) {
        *h = height;
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
    OHOS_GLWindowSurface *entry;
    int retval;

    entry = OHOS_GLRegistry_Acquire(window->id);
    if (!entry) {
        return -1;
    }

    if (entry->window_surface == EGL_NO_SURFACE ||
        !SDL_AtomicGet(&entry->window_surface_ready) ||
        SDL_AtomicGet(&entry->surface_generation) != entry->window_surface_generation ||
        !SDL_AtomicGet(&entry->surface_visible)) {
        OHOS_GLRegistry_Release(entry);
        return 0;
    }
    retval = SDL_EGL_SwapBuffers(_this, entry->window_surface);
    OHOS_GLRegistry_Release(entry);
    return retval;
}

void OHOS_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    if (context) {
        SDL_EGL_DeleteContext(_this, context);
    }
}

void OHOS_GL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor)
{
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
}

#endif /* SDL_VIDEO_DRIVER_OHOS && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
