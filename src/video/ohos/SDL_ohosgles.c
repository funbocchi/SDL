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
#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"

/* Tools */

#define OHOS_GL_PBUFFER_WIDTH  1280
#define OHOS_GL_PBUFFER_HEIGHT 720

static EGLSurface OHOS_GL_CreatePbufferSurface(_THIS)
{
    EGLSurface surface = SDL_EGL_CreateOffscreenSurface(_this,
                                                        OHOS_GL_PBUFFER_WIDTH,
                                                        OHOS_GL_PBUFFER_HEIGHT);

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] pbuffer create: surface=%p size=%dx%d error=%s",
                 (void *)surface, OHOS_GL_PBUFFER_WIDTH, OHOS_GL_PBUFFER_HEIGHT,
                 surface == EGL_NO_SURFACE ? SDL_GetError() : "none");
    return surface;
}

int OHOS_GL_PreparePendingSurface(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;

    SDL_assert(_this);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "[OHOS-GL] prepare enter: window=%p", (void *)window);
    if (!window || !(data = (SDL_WindowData *)window->driverdata) || !data->egl_lock) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "[OHOS-GL] prepare: invalid window");
        return SDL_InvalidParamError("window");
    }

    OHOS_Tool_LockWindowData(data);
    SDL_LockMutex(data->egl_lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] prepare: window=%u native=%p current=%p pending=%p pbuffer=%p",
                 window->id, (void *)data->native_window,
                 (void *)data->current_surface, (void *)data->pending_surface,
                 (void *)data->pbuffer_surface);
    if (data->pending_surface != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, data->pending_surface);
        data->pending_surface = EGL_NO_SURFACE;
    }
    if (data->native_window) {
        data->pending_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType)data->native_window);
    }
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] prepare done: window=%u pending=%p error=%s",
                 window->id, (void *)data->pending_surface,
                 data->pending_surface == EGL_NO_SURFACE ? SDL_GetError() : "none");
    SDL_UnlockMutex(data->egl_lock);
    OHOS_Tool_UnlockWidnowData(data);
    return 0;
}

int OHOS_GL_ActivatePendingSurface(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    int retval = 0;
    SDL_GLContext tls_context;

    SDL_assert(_this);
    if (!window || !(data = (SDL_WindowData *)window->driverdata) || !data->egl_lock) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "[OHOS-GL] activate: invalid window");
        return SDL_InvalidParamError("window");
    }

    SDL_LockMutex(data->egl_lock);
    tls_context = SDL_GL_GetCurrentContext();
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] activate: window=%u ctx=%p tls_ctx=%p current=%p pending=%p pbuffer=%p",
                 window->id, (void *)data->current_context, (void *)tls_context,
                 (void *)data->current_surface, (void *)data->pending_surface,
                 (void *)data->pbuffer_surface);

    if (data->current_context == EGL_NO_CONTEXT) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] activate: no context, skip make-current");
    } else if (data->current_context != tls_context) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] activate: context is not current on this thread, skip make-current");
    } else if (data->pending_surface != EGL_NO_SURFACE) {
        retval = SDL_EGL_MakeCurrent(_this, data->pending_surface, data->current_context);
        if (retval) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                         "[OHOS-GL] activate: eglMakeCurrent failed: %s", SDL_GetError());
            SDL_UnlockMutex(data->egl_lock);
            return retval;
        }
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] activate: eglMakeCurrent ok surface=%p",
                     (void *)data->pending_surface);
    } else {
        SDL_UnlockMutex(data->egl_lock);
        return 0;
    }

    SDL_EGL_DestroySurface(_this, data->current_surface);
    data->current_surface = data->pending_surface;
    data->pending_surface = EGL_NO_SURFACE;
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] activate done: window=%u current=%p pending=%p",
                 window->id, (void *)data->current_surface, (void *)data->pending_surface);
    SDL_UnlockMutex(data->egl_lock);
    return 0;
}

int OHOS_GL_DeactivatePendingSurface(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;

    SDL_assert(_this);
    if (!window || !(data = (SDL_WindowData *)window->driverdata) || !data->egl_lock) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "[OHOS-GL] deactivate: invalid window");
        return SDL_InvalidParamError("window");
    }

    SDL_LockMutex(data->egl_lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] deactivate: window=%u ctx=%p tls_ctx=%p current=%p pending=%p pbuffer=%p",
                 window->id, (void *)data->current_context,
                 (void *)SDL_GL_GetCurrentContext(),
                 (void *)data->current_surface, (void *)data->pending_surface,
                 (void *)data->pbuffer_surface);
    if (data->current_context != EGL_NO_CONTEXT && data->current_context == SDL_GL_GetCurrentContext()) {
        if (data->current_surface != EGL_NO_SURFACE) {
            if (SDL_EGL_MakeCurrent(_this, data->pbuffer_surface, data->current_context)) {
                SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                             "[OHOS-GL] deactivate: make pbuffer current failed: %s",
                             SDL_GetError());
            }
        }
    }
    SDL_EGL_DestroySurface(_this, data->current_surface);
    SDL_EGL_DestroySurface(_this, data->pending_surface);
    data->current_surface = EGL_NO_SURFACE;
    data->pending_surface = EGL_NO_SURFACE;
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "[OHOS-GL] deactivate done: window=%u", window->id);
    SDL_UnlockMutex(data->egl_lock);
    return 0;
}

/* Hook */

int OHOS_GL_LoadLibrary(_THIS, const char *path)
{
    int retval = 0;

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] load library: path=%s profile=0x%x version=%d.%d color=%d/%d/%d/%d depth=%d",
                 path ? path : "(default)",
                 _this->gl_config.profile_mask,
                 _this->gl_config.major_version,
                 _this->gl_config.minor_version,
                 _this->gl_config.red_size,
                 _this->gl_config.green_size,
                 _this->gl_config.blue_size,
                 _this->gl_config.alpha_size,
                 _this->gl_config.depth_size);

    if ((retval = SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType)EGL_DEFAULT_DISPLAY, 0))) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "SDL_EGL_LoadLibrary failed: %s", SDL_GetError());
        return retval;
    }
    _this->egl_data->egl_surfacetype = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

    /* OHOS XComponent surface requires an alpha channel. */
    if (_this->gl_config.alpha_size < 8) {
        _this->gl_config.alpha_size = 8;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] load library done: display=%p egl=%d.%d surfacetype=0x%x color=%d/%d/%d/%d",
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
    SDL_WindowData *data = NULL;

    SDL_assert(_this != NULL);
    if (!window || !(data = (SDL_WindowData *)window->driverdata) || !data->egl_lock) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] create context: invalid params window=%p driverdata=%p data=%p egl_lock=%p",
                     (void *)window,
                     window ? window->driverdata : NULL,
                     (void *)data,
                     data ? (void *)data->egl_lock : NULL);
        SDL_SetError("invalid params");
        return EGL_NO_CONTEXT;
    }

    SDL_LockMutex(data->egl_lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] create context: window=%u ctx=%p current=%p pending=%p pbuffer=%p tls_ctx=%p",
                 window->id, (void *)data->current_context,
                 (void *)data->current_surface, (void *)data->pending_surface,
                 (void *)data->pbuffer_surface,
                 (void *)SDL_GL_GetCurrentContext());

    if (data->pbuffer_surface == EGL_NO_SURFACE) {
        data->pbuffer_surface = OHOS_GL_CreatePbufferSurface(_this);
        SDL_assert(data->pbuffer_surface != EGL_NO_SURFACE);
    }
    if (data->current_context == EGL_NO_CONTEXT) {
        data->current_context = SDL_EGL_CreateContext(_this, data->pbuffer_surface);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] create context: SDL_EGL_CreateContext -> %p error=%s",
                     (void *)data->current_context,
                     data->current_context == EGL_NO_CONTEXT ? SDL_GetError() : "none");
    }
    SDL_UnlockMutex(data->egl_lock);

    OHOS_GL_ActivatePendingSurface(_this, window);

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] create context done: window=%u ctx=%p current=%p pending=%p tls_ctx=%p",
                 window->id, (void *)data->current_context,
                 (void *)data->current_surface, (void *)data->pending_surface,
                 (void *)SDL_GL_GetCurrentContext());

    return data->current_context;
}

int OHOS_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    SDL_WindowData *data;
    SDL_GLContext backup_context = EGL_NO_CONTEXT;
    int retval = 0;
    SDL_assert(_this != NULL);

    backup_context = SDL_GL_GetCurrentContext();
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "[OHOS-GL] make current: window=%u ctx=%p tls_ctx=%p",
                 window ? (unsigned int)window->id : 0,
                 (void *)context, (void *)backup_context);

    if (context == EGL_NO_CONTEXT) {
        retval = SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] make current: release context result=%d error=%s",
                     retval, retval ? SDL_GetError() : "none");
        return retval;
    } else if (!window) {
        SDL_Window *temp;
        for (temp = _this->windows; temp; temp = temp->next) {
            data = (SDL_WindowData *)temp->driverdata;
            if (!data) {
                continue;
            }
            SDL_LockMutex(data->egl_lock);
            if (context != data->current_context) {
                SDL_UnlockMutex(data->egl_lock);
                data = NULL;
                continue;
            }
            SDL_UnlockMutex(data->egl_lock);
            return OHOS_GL_MakeCurrent(_this, temp, context);
        }
        retval = SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] make current: no window for ctx=%p, release result=%d",
                     (void *)context, retval);
        return retval;
    } else {
        EGLSurface surface;
        data = (SDL_WindowData *)window->driverdata;
        if (!data) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                         "[OHOS-GL] make current: no driverdata for window=%u",
                         window->id);
            return SDL_InvalidParamError("window");
        }
        SDL_LockMutex(data->egl_lock);
        if (context != data->current_context) {
            SDL_UnlockMutex(data->egl_lock);
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                         "[OHOS-GL] make current: ctx=%p does not match window ctx=%p",
                         (void *)context, (void *)data->current_context);
            return OHOS_GL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        if (data->pending_surface != EGL_NO_SURFACE) {
            if (data->current_surface != EGL_NO_SURFACE) {
                SDL_EGL_DestroySurface(_this, data->current_surface);
            }
            data->current_surface = data->pending_surface;
            data->pending_surface = EGL_NO_SURFACE;
            surface = data->current_surface;
        } else if (data->current_surface != EGL_NO_SURFACE) {
            surface = data->current_surface;
        } else {
            surface = data->pbuffer_surface;
        }
        retval = SDL_EGL_MakeCurrent(_this, surface, context);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "[OHOS-GL] make current: bind pbuffer=%p ctx=%p result=%d error=%s",
                     (void *)data->pbuffer_surface, (void *)data->current_context,
                     retval, retval ? SDL_GetError() : "none");
        SDL_UnlockMutex(data->egl_lock);
        return retval;
    }
}

void OHOS_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h)
{
    SDL_WindowData *data;

    SDL_assert(_this != NULL);
    data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return;
    }
    *w = data->real_surface_w;
    *h = data->real_surface_h;
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
    int retval;

    SDL_assert(_this != NULL);
    data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return SDL_InvalidParamError("window");
    }

    SDL_LockMutex(data->egl_lock);
    if (data->current_context != EGL_NO_CONTEXT && data->current_context == SDL_GL_GetCurrentContext() && data->current_surface != EGL_NO_SURFACE) {
        retval = SDL_EGL_SwapBuffers(_this, data->current_surface);
        SDL_UnlockMutex(data->egl_lock);
        return retval;
    }
    SDL_UnlockMutex(data->egl_lock);
    return SDL_SetError("Not current context");
}

void OHOS_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    SDL_assert(_this != NULL);
    if (context == EGL_NO_CONTEXT) {
        return;
    } else {
        SDL_Window *temp;
        SDL_WindowData *data;
        for (temp = _this->windows; temp; temp = temp->next) {
            data = (SDL_WindowData *)temp->driverdata;
            if (!data) {
                continue;
            }
            SDL_LockMutex(data->egl_lock);
            if (context == data->current_context) {
                SDL_UnlockMutex(data->egl_lock);
                break;
            }
            SDL_UnlockMutex(data->egl_lock);
        }
        SDL_EGL_DeleteContext(_this, context);
        if (!data) {
            SDL_EGL_DestroySurface(_this, data->pbuffer_surface);
            data->pbuffer_surface = EGL_NO_SURFACE;
        }
    }
}

void OHOS_GL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor)
{
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
}

#endif

/* vi: set ts=4 sw=4 expandtab: */