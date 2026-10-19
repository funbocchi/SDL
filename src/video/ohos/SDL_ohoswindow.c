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
#include <native_window/external_window.h>
#include <window_manager/oh_display_info.h>
#include <window_manager/oh_display_manager.h>
#include <window_manager/oh_window_comm.h>

#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"
#include "SDL_video.h"

typedef enum
{
    kNormalX,
    kNormalY,
    kCenteredX,
    kCenteredY,
    kUndefinedX,
    kUndefinedY
} OHOSWindowPosMode;

typedef struct
{
    Uint32 id;
    const char *title;

    int x;
    SDL_bool px_x_force;
    int y;
    SDL_bool px_y_force;

    int w;
    int h;

    SDL_bool borderless;
    SDL_bool hidden;
    SDL_bool resizable;
    SDL_bool allow_highdpi;
} OHOS_WindowRequestContext;

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

static SDL_mutex *ohos_window_mutex = NULL;
static OHOS_CreateContext *g_window_context = NULL;

static napi_threadsafe_function call_create_window_tsf = NULL;

static float OHOS_Tool_GetDensityByDisplayId(Uint32 id)
{
    NativeDisplayManager_DisplayInfo *info = NULL;
    NativeDisplayManager_ErrorCode ret;
    float density_pixels;

    ret = OH_NativeDisplayManager_CreateDisplayById(id, &info);
    if (ret != DISPLAY_MANAGER_OK || info == NULL) {
        return 1.0f;
    }
    density_pixels = info->densityPixels;
    OH_NativeDisplayManager_DestroyDisplay(&info);
    return density_pixels;
}

static int OHOS_Tool_GetPX(Uint32 id, int value)
{
    float density = OHOS_Tool_GetDensityByDisplayId(id);

    return (int)roundf((float)value * density);
}

static void TSF_CallCreateWindow(napi_env env, napi_value cb, void *context, void *data)
{
    OHOS_WindowRequestContext *ctx = (OHOS_WindowRequestContext *)data;
    napi_value obj = NULL;
    napi_value undefined = NULL;

    if (ctx == NULL) {
        goto endfunc;
    }

    if (!(obj = Napi_CreateObj(env)) ||
        !(undefined = Napi_CreateUndefined(env)) ||
        (Napi_SetNamedProperty(env, obj, "id", Napi_CreateNumber(env, ctx->id)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "title", Napi_CreateString(env, ctx->title)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "x", Napi_CreateNumber(env, ctx->x)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "y", Napi_CreateNumber(env, ctx->y)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "w", Napi_CreateNumber(env, ctx->w)) != SDL_TRUE) ||
        (Napi_SetNamedProperty(env, obj, "h", Napi_CreateNumber(env, ctx->h)) != SDL_TRUE) ||
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

static int SDL_OHOS_RequestWindow(OHOS_WindowRequestContext *data)
{
    return Napi_CallThreadSafeFunction(call_create_window_tsf, data);
}

static int OHOS_Tool_QueuePOSValue(Uint32 id, OHOSWindowPosMode mode)
{
    NativeDisplayManager_SourceMode source_mode;
    NativeDisplayManager_ErrorCode err_code;
    WindowManager_WindowProperties window_properties;
    int pos_x_value;
    int pos_y_value;
    int pos_value;

    err_code = OH_NativeDisplayManager_GetDisplaySourceMode(id, &source_mode);
    if (err_code != DISPLAY_MANAGER_OK) {
        pos_value = 0;
        goto endfunc;
    }

    if (source_mode != DISPLAY_SOURCE_MODE_MAIN && source_mode != DISPLAY_SOURCE_MODE_EXTEND) {
        pos_value = 0;
        goto endfunc;
    }

    err_code = OH_NativeDisplayManager_GetDisplayPosition(id, &pos_x_value, &pos_y_value);
    if (err_code != DISPLAY_MANAGER_OK) {
        pos_value = 0;
        goto endfunc;
    }

    if (mode == kUndefinedX) {
        pos_value = pos_x_value;
        goto endfunc;
    } else if (mode == kUndefinedY) {
        pos_value = pos_y_value;
        goto endfunc;
    }

    if (OH_WindowManager_GetWindowProperties(id, &window_properties) != OK) {
        pos_value = 0;
        goto endfunc;
    }

    if (mode == kCenteredX) {
        pos_value = pos_x_value + window_properties.windowRect.width / 2;
    } else if (mode == kCenteredY) {
        pos_value = pos_y_value + window_properties.windowRect.height / 2;
    } else {
        pos_value = 0;
    }

endfunc:
    return pos_value;
}

static int OHOS_Tool_ParseWindowRequestContext(SDL_Window *window, OHOS_WindowRequestContext *context)
{
    int retval = 0;
    int id = 0;
    OHOSWindowPosMode mode;

    context->title = SDL_strdup(window->title ? window->title : "");
    if (!context->title) {
        retval = SDL_OutOfMemory();
        goto endfunc;
    }

    context->allow_highdpi = (window->flags & SDL_WINDOW_ALLOW_HIGHDPI) ? SDL_TRUE : SDL_FALSE;

    context->id = window->id;

    context->w = window->w;
    context->h = window->h;

    if (SDL_WINDOWPOS_ISCENTERED(window->x)) {
        id = window->x & ~SDL_WINDOWPOS_CENTERED_MASK;
        mode = kCenteredX;
        context->x = OHOS_Tool_QueuePOSValue(id, mode) - ((context->allow_highdpi == SDL_TRUE) ? OHOS_Tool_GetPX(id, window->w) : window->w) / 2;
        context->px_x_force = SDL_TRUE;
    } else if (SDL_WINDOWPOS_ISUNDEFINED(window->x)) {
        id = window->x & ~SDL_WINDOWPOS_UNDEFINED_MASK;
        mode = kUndefinedX;
        context->x = OHOS_Tool_QueuePOSValue(id, mode);
        context->px_x_force = SDL_TRUE;
    } else {
        mode = kNormalX;
        id = 0;
        context->x = window->x;
        context->px_x_force = SDL_FALSE;
    }

    if (SDL_WINDOWPOS_ISCENTERED(window->y)) {
        id = window->y & ~SDL_WINDOWPOS_CENTERED_MASK;
        mode = kCenteredY;
        context->y = OHOS_Tool_QueuePOSValue(id, mode) - ((context->allow_highdpi == SDL_TRUE) ? OHOS_Tool_GetPX(id, window->h) : window->h) / 2;
        context->px_y_force = SDL_TRUE;
    } else if (SDL_WINDOWPOS_ISUNDEFINED(window->y)) {
        id = window->y & ~SDL_WINDOWPOS_UNDEFINED_MASK;
        mode = kUndefinedY;
        context->y = OHOS_Tool_QueuePOSValue(id, mode);
        context->px_y_force = SDL_TRUE;
    } else {
        mode = kNormalY;
        id = 0;
        context->y = window->y;
        context->px_y_force = SDL_FALSE;
    }

    context->borderless = (window->flags & SDL_WINDOW_BORDERLESS) ? SDL_TRUE : SDL_FALSE;
    context->resizable = (window->flags & SDL_WINDOW_RESIZABLE) ? SDL_TRUE : SDL_FALSE;

endfunc:
    return retval;
}

int OHOS_CreateWindow(_THIS, SDL_Window *window)
{
    OHOS_WindowData *data = NULL;
    OHOS_WindowRequestContext *request_context = NULL;
    int retval = 0;

    if (!ohos_window_mutex) {
        ohos_window_mutex = SDL_CreateMutex();
        if (!ohos_window_mutex) {
            retval = SDL_OutOfMemory();
            goto error;
        }
    }

    SDL_LockMutex(ohos_window_mutex);

    data = (OHOS_WindowData *)SDL_calloc(1, sizeof(OHOS_WindowData));
    if (!data) {
        retval = SDL_OutOfMemory();
        goto endfunc;
    }

    request_context = (OHOS_WindowRequestContext *)SDL_calloc(1, sizeof(OHOS_WindowRequestContext));
    if (!request_context) {
        retval = SDL_OutOfMemory();
        goto endfunc;
    }

    g_window_context = (OHOS_CreateContext *)SDL_calloc(1, sizeof(OHOS_CreateContext));
    if (!g_window_context) {
        retval = SDL_OutOfMemory();
        goto endfunc;
    }
    g_window_context->sem = SDL_CreateSemaphore(0);
    if (!g_window_context->sem) {
        retval = SDL_OutOfMemory();
        goto endfunc;
    }

    retval = OHOS_Tool_ParseWindowRequestContext(window, request_context);
    if (retval != 0) {
        goto endfunc;
    }

    retval = SDL_OHOS_RequestWindow(request_context);

    if (SDL_SemWaitTimeout(g_window_context->sem, 5000) != 0) {
        retval = SDL_SetError("OHOS Window creation timed out");
        goto endfunc;
    }

#ifdef SDL_VIDEO_OPENGL_EGL
#endif

endfunc:
    if (data) {
        SDL_free(data);
        data = NULL;
    }
    if (request_context) {
        if (request_context->title) {
            SDL_free(request_context->title);
        }
        SDL_free(request_context);
        request_context = NULL;
    }
    if (g_window_context) {
        if (g_window_context->sem) {
            SDL_DestroySemaphore(g_window_context->sem);
        }
        SDL_free(g_window_context);
    }
    SDL_UnlockMutex(ohos_window_mutex);
error:
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

int OHOS_CreateSDLWindowFrom(_THIS, SDL_Window *window, const void *data) {}

void OHOS_DestroyWindow(_THIS, SDL_Window *window) {}

void OHOS_SetWindowTitle(_THIS, SDL_Window *window) {}

void OHOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon) {}

void OHOS_SetWindowPosition(_THIS, SDL_Window *window) {}

void OHOS_SetWindowSize(_THIS, SDL_Window *window) {}

int OHOS_GetWindowBordersSize(_THIS, SDL_Window *window)
{
}

#endif
/* vi: set ts=4 sw=4 expandtab: */