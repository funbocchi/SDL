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

#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohoswindow.h"

/* Hook */

int OHOS_GL_LoadLibrary(_THIS, const char *path)
{
    int retval = 0;

    if ((retval = SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType)EGL_DEFAULT_DISPLAY, 0))) {
        return retval;
    }
    _this->egl_data->egl_surfacetype = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
    return retval;
}

void *OHOS_GL_GetProcAddress(_THIS, const char *proc)
{
    SDL_EGL_GetProcAddress(_this, proc);
}

void OHOS_GL_UnloadLibrary(_THIS)
{
    SDL_EGL_UnloadLibrary(_this);
}

SDL_GLContext OHOS_GL_CreateContext(_THIS, SDL_Window *window)
{
}

int OHOS_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
}

void OHOS_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h) {}

int OHOS_GL_SetSwapInterval(_THIS, int interval)
{
    return SDL_EGL_SetSwapInterval(_this, interval);
}

int OHOS_GL_GetSwapInterval(_THIS)
{
    return SDL_EGL_GetSwapInterval(_this);
}

int OHOS_GL_SwapWindow(_THIS, SDL_Window *window) {}

void OHOS_GL_DeleteContext(_THIS, SDL_GLContext context) {}

void OHOS_GL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor) {}

/* Private */

#define OHOS_GL_PBUFFER_WIDTH  1280
#define OHOS_GL_PBUFFER_HEIGHT 720

static EGLSurface OHOS_GL_CreatePbufferSurface(_THIS)
{
    if (!_this || !_this->egl_data) {
        return EGL_NO_SURFACE;
    }

    return SDL_EGL_CreateOffscreenSurface(_this,
                                          OHOS_GL_PBUFFER_WIDTH,
                                          OHOS_GL_PBUFFER_HEIGHT);
}

int OHOS_GL_PreparePendingSurface(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    EGLSurface surface;

    if (!_this || !window || !(data = (SDL_WindowData *)window->driverdata) || !(data->egl_lock) || !(data->create_lock)) {
        return SDL_SetError("invalid params");
    }
    SDL_LockMutex(data->create_lock);
    if (!data->native_window || !data->surface_ready) {
        SDL_UnlockMutex(data->create_lock);
        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "OHNativeWindow not ready");
        goto pbuffer;
    }
    SDL_LockMutex(data->egl_lock);
    surface = data->pending_egl_surface;
    if (surface != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, surface);
        data->pending_egl_surface = EGL_NO_SURFACE;
    }
    data->pending_ready = SDL_FALSE;
    surface = SDL_EGL_CreateSurface(_this, (NativeWindowType)data->native_window);
    if (surface != EGL_NO_SURFACE) {
        data->pending_ready = SDL_TRUE;
        data->pending_egl_surface = surface;
    }
    SDL_UnlockMutex(data->egl_lock);
    SDL_UnlockMutex(data->create_lock);

pbuffer:
    SDL_LockMutex(data->egl_lock);
    surface = data->pbuffer_surface;
    if (surface != EGL_NO_SURFACE) {
        SDL_UnlockMutex(data->egl_lock);
        return 0;
    }
    surface = SDL_EGL_CreateOffscreenSurface(_this, OHOS_GL_PBUFFER_WIDTH, OHOS_GL_PBUFFER_HEIGHT);
    if (surface == EGL_NO_SURFACE) {
        SDL_UnlockMutex(data->egl_lock);
        return SDL_SetError("failed to create offscreenSurface");
    }
    data->pbuffer_surface = surface;
    SDL_UnlockMutex(data->egl_lock);
    return 0;
}

int OHOS_GL_ActivatePendingSurface(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    SDL_GLContext context;
    EGLSurface surface;
    int retval = 0;

    if (!_this || !window || !(data = (SDL_WindowData *)window->driverdata) || !(data->egl_lock)) {
        return SDL_SetError("invalid params");
    }

    SDL_LockMutex(data->egl_lock);
    if ((context = data->current_context) == EGL_NO_CONTEXT) {
        SDL_UnlockMutex(data->egl_lock);
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "eglContext not ready");
        return 0;
    }
    surface = data->pending_egl_surface;
    if (surface != EGL_NO_SURFACE && data->pending_ready) {
        if (SDL_EGL_MakeCurrent(_this, surface, context)) {
            data->pending_ready = SDL_FALSE;
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "%s", SDL_GetError());
            goto pbuffer;
        } else {
            surface = data->current_surface;
            data->current_surface = data->pending_egl_surface;
            data->pending_egl_surface = EGL_NO_SURFACE;
            data->pending_ready = SDL_FALSE;
            if (surface != data->pbuffer_surface) {
                SDL_EGL_DestroySurface(_this, surface);
            }
            SDL_UnlockMutex(data->egl_lock);
            SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "pending_surface in effected");
            return 0;
        }
    }
    SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "pending_egl_surface not ready");

pbuffer:
    surface = data->pbuffer_surface;
    if (surface == EGL_NO_SURFACE) {
        SDL_UnlockMutex(data->egl_lock);
        return SDL_SetError("pbufferSurface not initialized");
    }
    if (!(retval = SDL_EGL_MakeCurrent(_this, surface, context))) {
        data->current_surface = surface;
    }
    SDL_UnlockMutex(data->egl_lock);
    return retval;
}

int OHOS_GL_DeactivatePendingSurface(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    SDL_GLContext context;
    EGLSurface surface;
    int retval = 0;

    if (!_this || !window || !(data = (SDL_WindowData *)window->driverdata) || !(data->egl_lock)) {
        return SDL_SetError("invalid params");
    }
    SDL_LockMutex(data->egl_lock);
    context = data->current_context;

    if (data->pending_egl_surface != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, data->pending_egl_surface);
        data->pending_egl_surface = EGL_NO_SURFACE;
        data->pending_ready = SDL_FALSE;
    }

    if (context == EGL_NO_CONTEXT) {
        SDL_UnlockMutex(data->egl_lock);
        return SDL_SetError("invalid call");
    }
    surface = data->pbuffer_surface;
    if (surface == EGL_NO_SURFACE) {
        SDL_UnlockMutex(data->egl_lock);
        return SDL_SetError("pbufferSurface invalid");
    }
    retval = SDL_EGL_MakeCurrent(_this, surface, context);
    if (retval) {
        SDL_UnlockMutex(data->egl_lock);
        return retval;
    }
    surface = data->current_surface;
    data->current_surface = data->pbuffer_surface;
    if (surface != data->pbuffer_surface) {
        SDL_EGL_DestroySurface(_this, surface);
    }
    SDL_UnlockMutex(data->egl_lock);
    return retval;
}

#endif

/* vi: set ts=4 sw=4 expandtab: */