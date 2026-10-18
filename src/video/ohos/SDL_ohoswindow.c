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

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <arkui/native_node_napi.h>
#include <window_manager/oh_display_info.h>
#include <window_manager/oh_display_manager.h>

#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"
#include "SDL_video.h"

typedef enum
{
    OHOS_POS_NORMAL = 0,
    OHOS_POS_CENTERED = 1,
    OHOS_POS_UNDEFINED = 2
} OHOS_PositionMode;

typedef struct
{
    Uint32 id;
    const char *title;

    OHOS_PositionMode x_mode;
    OHOS_PositionMode y_mode;
    int x;
    int y;

    int w;
    int h;

    Uint32 display_index;

    SDL_bool fullscreen;
    SDL_bool fullscreen_desktop;
    SDL_bool borderless;
    SDL_bool hidden;
    SDL_bool resizable;
    SDL_bool allow_highdpi;
} OHOS_RequestWindowContext;

typedef struct
{
    Uint32 id;
    SDL_sem *sem;
    int x;
    int y;
    int w;
    int h;
    OHNativeWindow *native_window;
    SDL_bool created_done;
    SDL_bool created_ok;
    SDL_bool surface_ready;
    SDL_bool posted;
} OHOS_CreateContext;

static SDL_mutex *g_ctx_lock = NULL;
static OHOS_CreateContext *g_ctx = NULL;
static napi_threadsafe_function call_create_window_tsf = NULL;

static void TSF_CallCreateWindow(napi_env env, napi_value cb, void *context, void *data)
{
    OHOS_RequestWindowContext *ctx = (OHOS_RequestWindowContext *)data;
    napi_value obj = NULL;
    napi_value undefined = NULL;

    if (ctx == NULL) {
        goto endfunc;
    }

    if (!(obj = Napi_CreateObj(env)) ||
        !(undefined = Napi_CreateUndefined(env)) ||
        (Napi_SetNamedProperty(env, obj, "id", Napi_CreateNumber(env, ctx->id)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "title", Napi_CreateString(env, ctx->title)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "xMode", Napi_CreateNumber(env, (double)ctx->x_mode)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "yMode", Napi_CreateNumber(env, (double)ctx->y_mode)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "x", Napi_CreateNumber(env, ctx->x)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "y", Napi_CreateNumber(env, ctx->y)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "w", Napi_CreateNumber(env, ctx->w)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "h", Napi_CreateNumber(env, ctx->h)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "displayIndex", Napi_CreateNumber(env, ctx->display_index)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "fullscreen", Napi_CreateBoolean(env, ctx->fullscreen)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "fullscreenDesktop", Napi_CreateBoolean(env, ctx->fullscreen_desktop)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "borderless", Napi_CreateBoolean(env, ctx->borderless)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "hidden", Napi_CreateBoolean(env, ctx->hidden)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "resizable", Napi_CreateBoolean(env, ctx->resizable)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "allowHighdpi", Napi_CreateBoolean(env, ctx->allow_highdpi)) != SDL_TRUE)) {
        goto endfunc;
    }

    Napi_CallFunction(env, cb, 1, &obj);

endfunc:
    Napi_ClearPending(env, "TSF_CallCreateWindow");
    SDL_free(ctx);
}

napi_value OHOS_ThreadSafe_RegisterCallCreateWindow(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value cb = NULL;
    napi_value name = NULL;
    napi_value yes = NULL;
    napi_valuetype type = napi_undefined;
    napi_status status;

    if (Napi_CheckTSFCallbackArg(env, info, &cb) != SDL_TRUE) {
        return NULL;
    }

    if (Napi_CreateTSF(env, cb, "OHOS_RequestWindow", TSF_CallCreateWindow, &call_create_window_tsf) != SDL_TRUE) {
        return NULL;
    }

    return Napi_CreateBoolean(env, SDL_TRUE);
}

static int SDL_OHOS_RequestWindow(OHOS_RequestWindowContext *data)
{
    return Napi_CallThreadSafeFunction(call_create_window_tsf, data);
}

int OHOS_CreateSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    OHOS_WindowData *data;
    OHOS_RequestWindowContext *params;
    int retval = 0;
    float density = 0.0f;

    if (!ohos_window_mutex) {
        ohos_window_mutex = SDL_CreateMutex();
    }

    SDL_LockMutex(ohos_window_mutex);

    data = (OHOS_WindowData *)SDL_calloc(1, sizeof(OHOS_WindowData));
    if (!data) {
        retval = SDL_OutOfMemory();
        goto endfunc;
    }

    params = (OHOS_RequestWindowContext *)SDL_calloc(1, sizeof(OHOS_RequestWindowContext));
    if (!params) {
        retval = SDL_OutOfMemory();
        SDL_free(data);
        goto endfunc;
    }

    g_ctx = (OHOS_CreateContext *)SDL_calloc(1, sizeof(OHOS_CreateContext));
    if (!g_ctx) {
        retval = SDL_OutOfMemory();
        SDL_free(data);
        SDL_free(params);
        goto endfunc;
    }
    g_ctx->sem = SDL_CreateSemaphore(0);
    if (!g_ctx->sem) {
        retval = SDL_OutOfMemory();
        SDL_free(g_ctx);
        g_ctx = NULL;
        SDL_free(data);
        SDL_free(params);
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

    if (SDL_WINDOWPOS_ISCENTERED(window->x)) {
        params->x_mode = OHOS_POS_CENTERED;
        params->display_index = window->x & ~SDL_WINDOWPOS_CENTERED_MASK;
        params->x = 0;
    } else if (SDL_WINDOWPOS_ISUNDEFINED(window->x)) {
        params->x_mode = OHOS_POS_UNDEFINED;
        params->display_index = window->x & ~SDL_WINDOWPOS_UNDEFINED_MASK;
        params->x = 0;
    } else {
        params->x_mode = OHOS_POS_NORMAL;
        params->display_index = 0;
        params->x = window->x;
    }
    if (SDL_WINDOWPOS_ISCENTERED(window->y)) {
        params->y_mode = OHOS_POS_CENTERED;
        params->display_index = window->y & ~SDL_WINDOWPOS_CENTERED_MASK;
        params->y = 0;
    } else if (SDL_WINDOWPOS_ISUNDEFINED(window->y)) {
        params->y_mode = OHOS_POS_UNDEFINED;
        params->display_index = window->y & ~SDL_WINDOWPOS_UNDEFINED_MASK;
        params->y = 0;
    } else {
        params->y_mode = OHOS_POS_NORMAL;
        params->display_index = 0;
        params->y = window->y;
    }

    params->w = window->w;
    params->h = window->h;

    params->fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? SDL_TRUE : SDL_FALSE;
    params->fullscreen_desktop = (window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? SDL_TRUE : SDL_FALSE;
    params->borderless = (window->flags & SDL_WINDOW_BORDERLESS) ? SDL_TRUE : SDL_FALSE;
    params->allow_highdpi = (window->flags & SDL_WINDOW_ALLOW_HIGHDPI) ? SDL_TRUE : SDL_FALSE;
    params->resizable = (window->flags & SDL_WINDOW_RESIZABLE) ? SDL_TRUE : SDL_FALSE;

    retval = SDL_OHOS_RequestWindow(params);

    if (SDL_SemWaitTimeout(g_ctx->sem, 5000) != 0) {
        retval = SDL_SetError("OHOS Window creation timed out");
        goto endfunc;
    }

#ifdef SDL_VIDEO_OPENGL_EGL
#endif

endfunc:
    SDL_UnlockMutex(ohos_window_mutex);
    return retval;
}

static void OHOS_SurfaceHolder_OnSurfaceCreated(OH_ArkUI_SurfaceHolder *holder)
{
    OHNativeWindow *window;

    window = OH_ArkUI_XComponent_GetNativeWindow(holder);
}

static void OHOS_SurfaceHolder_OnSurfaceChanged(OH_ArkUI_SurfaceHolder *holder, Uint64 width, Uint64 height) {}

static void OHOS_SurfaceHolder_OnSurfaceDestroyed(OH_ArkUI_SurfaceHolder *holder) {}

static void OHOS_SurfaceHolder_OnSurfaceShow(OH_ArkUI_SurfaceHolder *holder) {}

static void OHOS_SurfaceHolder_OnSurfaceHide(OH_ArkUI_SurfaceHolder *holder) {}

napi_value OHOS_NodeContent_BindNode(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL };
    ArkUI_NodeHandle handle = NULL;
    OH_ArkUI_SurfaceHolder *holder = NULL;
    OH_ArkUI_SurfaceCallback *callback = NULL;

    if (napi_get_cb_info(env, info, &argc, args, NULL, NULL) != napi_ok) {
    }

    OH_ArkUI_GetNodeHandleFromNapiValue(env, args[1], &handle);
    holder = OH_ArkUI_SurfaceHolder_Create(handle);
}

napi_value OHOS_NodeContent_UnbindNode(napi_env env, napi_callback_info info) {}

int OHOS_CreateSDLWindowFrom(SDL_VideoDevice *_this, SDL_Window *window, const void *data) {}

void OHOS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window) {}

void OHOS_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window) {}

void OHOS_SetWindowIcon(SDL_VideoDevice *_this, SDL_Window *window, SDL_Surface *icon) {}

void OHOS_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window) {}

void OHOS_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window) {}

#endif
/* vi: set ts=4 sw=4 expandtab: */