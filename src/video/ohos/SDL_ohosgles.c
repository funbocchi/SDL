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
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "SDL_EGL_LoadLibrary failed: %s", SDL_GetError());
        return retval;
    }
    _this->egl_data->egl_surfacetype = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

    /* OHOS XComponent surface requires an alpha channel. */
    if (_this->gl_config.alpha_size < 8) {
        _this->gl_config.alpha_size = 8;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "EGL loaded: path=%s display=%p version=%d.%d surfacetype=0x%x "
                "color=%d/%d/%d/%d",
                path ? path : "(default)",
                (void *)_this->egl_data->egl_display,
                _this->egl_data->egl_version_major,
                _this->egl_data->egl_version_minor,
                (unsigned int)_this->egl_data->egl_surfacetype,
                _this->gl_config.red_size,
                _this->gl_config.green_size,
                _this->gl_config.blue_size,
                _this->gl_config.alpha_size);
    return retval;
}

void *OHOS_GL_GetProcAddress(_THIS, const char *proc)
{
    return SDL_EGL_GetProcAddress(_this, proc);
}

void OHOS_GL_UnloadLibrary(_THIS)
{
    SDL_EGL_UnloadLibrary(_this);
}

SDL_GLContext OHOS_GL_CreateContext(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    SDL_GLContext context = EGL_NO_CONTEXT;
    if (!_this || !window || !(data = (SDL_WindowData *)window->driverdata) || !data->egl_lock) {
        SDL_SetError("invalid params");
        return EGL_NO_CONTEXT;
    }

    SDL_LockMutex(data->egl_lock);
    if (data->current_context != EGL_NO_CONTEXT) {
        data->context_refcount++;
        OHOS_GL_ActivatePendingSurface(_this, window);
        SDL_UnlockMutex(data->egl_lock);
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Repeatedly creating EGLContext");
        return data->current_context;
    }
    if (OHOS_GL_PreparePendingSurface(_this, window)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                    "PreparePendingSurface failed: %s", SDL_GetError());
        goto endfunc;
    }
    {
        EGLSurface surface = data->pbuffer_surface;
        SDL_bool using_pending = SDL_FALSE;

        if (data->pending_egl_surface != EGL_NO_SURFACE &&
            data->pending_ready) {
            surface = data->pending_egl_surface;
            using_pending = SDL_TRUE;
        }

        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                    "CreateContext: pbuffer=%p pending=%p current_surface=%p "
                    "using=%p",
                    (void *)data->pbuffer_surface,
                    (void *)data->pending_egl_surface,
                    (void *)data->current_surface,
                    (void *)surface);
        context = SDL_EGL_CreateContext(_this, surface);
        if (context != EGL_NO_CONTEXT) {
            data->current_context = context;
            data->context_refcount++;
            data->current_surface = surface;
            if (using_pending) {
                data->pending_egl_surface = EGL_NO_SURFACE;
                data->pending_ready = SDL_FALSE;
            }
            SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                        "CreateContext OK: context=%p surface=%p",
                        (void *)context, (void *)surface);
            OHOS_GL_ActivatePendingSurface(_this, window);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "SDL_EGL_CreateContext failed: %s", SDL_GetError());
        }
    }

endfunc:
    SDL_UnlockMutex(data->egl_lock);
    return context;
}

int OHOS_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    SDL_WindowData *data;
    SDL_GLContext win_context = EGL_NO_CONTEXT;
    int retval;
    if (!_this) {
        return SDL_SetError("invalid params");
    }
    if (!window || context == EGL_NO_CONTEXT) {
        return SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, NULL);
    }
    if (!(data = (SDL_WindowData *)window->driverdata) || !data->egl_lock) {
        return SDL_SetError("window not ready");
    }
    SDL_LockMutex(data->egl_lock);
    if (context != data->current_context) {
        SDL_UnlockMutex(data->egl_lock);
        return SDL_SetError("multiple GL contexts per window are not supported");
    }
    if (data->pending_egl_surface != EGL_NO_SURFACE && data->pending_ready) {
        SDL_UnlockMutex(data->egl_lock);
        return OHOS_GL_ActivatePendingSurface(_this, window);
    }
    if (data->current_surface != EGL_NO_SURFACE) {
        retval = SDL_EGL_MakeCurrent(_this, data->current_surface, context);
        SDL_UnlockMutex(data->egl_lock);
        return retval;
    }
    SDL_UnlockMutex(data->egl_lock);
    return OHOS_GL_ActivatePendingSurface(_this, window);
}

void OHOS_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h)
{
    *w = window->w;
    *h = window->h;
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
    SDL_WindowData *data;

    if (!_this || !window || !(data = (SDL_WindowData *)window->driverdata) || !data->egl_lock) {
        return SDL_SetError("invalid params");
    }
    SDL_LockMutex(data->egl_lock);
    if (data->pending_egl_surface != EGL_NO_SURFACE && data->pending_ready) {
        SDL_UnlockMutex(data->egl_lock);
        OHOS_GL_ActivatePendingSurface(_this, window);
        SDL_LockMutex(data->egl_lock);
    }
    if (data->current_surface == data->pbuffer_surface) {
        SDL_UnlockMutex(data->egl_lock);
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "pbuffer, swap cancled");
        return -1;
    }
    if (data->current_surface != EGL_NO_SURFACE) {
        int retval = SDL_EGL_SwapBuffers(_this, data->current_surface);
        SDL_UnlockMutex(data->egl_lock);
        return retval;
    } else {
        SDL_UnlockMutex(data->egl_lock);
        return SDL_SetError("Surface not ready");
    }
}

void OHOS_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    SDL_Window *window;
    SDL_bool destroy_complete = SDL_FALSE;

    if (!_this || !_this->egl_data || !context || context == EGL_NO_CONTEXT) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "invalid params");
        return;
    }
    for (window = _this->windows; window; window = window->next) {
        SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

        if (!data || !data->egl_lock) {
            continue;
        }
        SDL_LockMutex(data->egl_lock);
        if (data->current_context == context) {
            data->current_context = EGL_NO_CONTEXT;
            if (data->current_surface != EGL_NO_SURFACE) {
                if (data->pending_egl_surface != EGL_NO_SURFACE && data->pending_ready) {
                    SDL_EGL_DestroySurface(_this, data->current_surface);
                } else {
                    data->pending_egl_surface = data->current_surface;
                    data->pending_ready = SDL_TRUE;
                }
                data->current_surface = EGL_NO_SURFACE;
            }
            SDL_UnlockMutex(data->egl_lock);
            SDL_EGL_DeleteContext(_this, context);
            return;
        } else {
            SDL_UnlockMutex(data->egl_lock);
            continue;
        }
    }
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "failed to located context window");
}

void OHOS_GL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor)
{
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
}

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
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "PreparePendingSurface: native_window=%p surface_ready=%d",
                (void *)data->native_window, data->surface_ready);
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
        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                    "eglCreateWindowSurface OK: surface=%p", (void *)surface);
    } else {
        EGLint red = -1, green = -1, blue = -1, alpha = -1;
        EGLint surface_type = -1, visual_id = -1;
        EGLDisplay display = _this->egl_data->egl_display;
        EGLConfig config = _this->egl_data->egl_config;

        _this->egl_data->eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red);
        _this->egl_data->eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green);
        _this->egl_data->eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue);
        _this->egl_data->eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &alpha);
        _this->egl_data->eglGetConfigAttrib(display, config, EGL_SURFACE_TYPE, &surface_type);
        _this->egl_data->eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visual_id);

        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                    "eglCreateWindowSurface failed: %s "
                    "(config rgba=%d/%d/%d/%d surface_type=0x%x visual=0x%x)",
                    SDL_GetError(), red, green, blue, alpha,
                    (unsigned int)surface_type, (unsigned int)visual_id);
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
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                    "eglCreatePbufferSurface failed: %s", SDL_GetError());
        SDL_UnlockMutex(data->egl_lock);
        return SDL_SetError("failed to create offscreenSurface");
    }
    data->pbuffer_surface = surface;
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "eglCreatePbufferSurface OK: surface=%p", (void *)surface);
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
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "ActivatePendingSurface: context=%p pending=%p current=%p pbuffer=%p",
                (void *)context,
                (void *)data->pending_egl_surface,
                (void *)data->current_surface,
                (void *)data->pbuffer_surface);
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
    if (data->current_surface != EGL_NO_SURFACE) {
        retval = SDL_EGL_MakeCurrent(_this, data->current_surface, context);
        SDL_UnlockMutex(data->egl_lock);
        return retval;
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