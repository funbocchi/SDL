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

#include "SDL_ohoswindow.h"

typedef struct OH_PixelmapNative OH_PixelmapNative;

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <arkui/native_interface.h>
#include <arkui/native_node.h>
#include <arkui/native_node_napi.h>
#include <limits.h>

#include "../SDL_sysvideo.h"
#include "SDL_events.h"
#include "SDL_timer.h"

#include "../../events/SDL_windowevents_c.h"

#include "SDL_ohosevents.h"
#include "SDL_ohosvideo.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "SDL_ohosgles.h"
#endif

typedef enum
{
    OHOS_WINDOW_CMD_CREATE = 0,
    OHOS_WINDOW_CMD_DESTROY = 1,
    OHOS_WINDOW_CMD_SET_TITLE = 2,
    OHOS_WINDOW_CMD_SET_ICON = 3,
    OHOS_WINDOW_CMD_SET_POSITION = 4,
    OHOS_WINDOW_CMD_SET_SIZE = 5,
    OHOS_WINDOW_CMD_SET_MINIMUM_SIZE = 6,
    OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE = 7,
    OHOS_WINDOW_CMD_SHOW = 8,
    OHOS_WINDOW_CMD_HIDE = 9,
    OHOS_WINDOW_CMD_RAISE = 10,
    OHOS_WINDOW_CMD_MAXIMIZE = 11,
    OHOS_WINDOW_CMD_MINIMIZE = 12,
    OHOS_WINDOW_CMD_RESTORE = 13,
    OHOS_WINDOW_CMD_SET_BORDERED = 14,
    OHOS_WINDOW_CMD_SET_RESIZABLE = 15,
    OHOS_WINDOW_CMD_SET_ALWAYS_ON_TOP = 16,
    OHOS_WINDOW_CMD_SET_FULLSCREEN = 17,
    OHOS_WINDOW_CMD_SET_OPACITY = 18,
    OHOS_WINDOW_CMD_SET_MODAL_FOR = 19,
    OHOS_WINDOW_CMD_SET_INPUT_FOCUS = 20,
    OHOS_WINDOW_CMD_SET_MOUSE_RECT = 21,
    OHOS_WINDOW_CMD_SET_MOUSE_GRAB = 22,
    OHOS_WINDOW_CMD_SET_KEYBOARD_GRAB = 23,
    OHOS_WINDOW_CMD_COUNT
} OHOS_WindowCommand;

typedef enum
{
    OHOS_NATIVE_WINDOW_NONE = 0,
    OHOS_NATIVE_WINDOW_CREATED,
    OHOS_NATIVE_WINDOW_SHOWN,
    OHOS_NATIVE_WINDOW_HIDDEN,
    OHOS_NATIVE_WINDOW_DESTROYED
} OHOS_NativeWindowState;

typedef enum
{
    OHOS_WINDOW_EVENT_SURFACE_CREATED,
    OHOS_WINDOW_EVENT_SURFACE_CHANGED,
    OHOS_WINDOW_EVENT_SURFACE_DESTROYED
} OHOS_WindowEventCode;

typedef struct
{
    OHOS_WindowCommand cmd;

    Uint32 sdl_window_id;
    const char *title;
    int x, y;
    int w, h;
    int min_w, min_h;
    int max_w, max_h;
    Uint32 display_index;

    SDL_bool borderless;
    SDL_bool resizable;
    SDL_bool allow_highdpi;
    SDL_bool always_on_top;
    SDL_bool skip_taskbar;
    SDL_bool utility;
    SDL_bool tooltip;
    SDL_bool popup_menu;
    SDL_bool fullscreen;
    SDL_bool fullscreen_desktop;

    SDL_bool shown;
    SDL_bool hidden;

    float opacity;
    float brightness;
} OHOS_WindowRequest;

typedef struct OHOS_WindowRegistryEntry
{
    Uint32 id;
    SDL_WindowData *data;
    struct OHOS_WindowRegistryEntry *next;
} OHOS_WindowRegistryEntry;

static SDL_mutex *g_ohos_window_registry_lock = NULL;
static OHOS_WindowRegistryEntry *g_ohos_window_registry = NULL;

static napi_threadsafe_function window_request_tsf;
static ArkUI_NativeNodeAPI_1 *g_node_api;

static void OHOS_WindowData_Free(SDL_WindowData *data)
{
    if (!data) {
        SDL_InvalidParamError("data");
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%s", SDL_GetError());
        return;
    }
    if (data->create_cond) {
        SDL_DestroyCond(data->create_cond);
    }
    if (data->create_lock) {
        SDL_DestroyMutex(data->create_lock);
    }
    SDL_free(data);
}

static void OHOS_WindowRegistry_Release(SDL_WindowData *data)
{
    SDL_bool should_free = SDL_FALSE;

    SDL_assert(g_ohos_window_registry_lock != NULL);
    if (!data) {
        SDL_InvalidParamError("data");
        return;
    }
    SDL_LockMutex(g_ohos_window_registry_lock);
    if (data->ref_count == 0) {
        SDL_UnlockMutex(g_ohos_window_registry_lock);
        return;
    }
    data->ref_count--;
    if (data->ref_count == 0 && SDL_AtomicGet(&data->destroying)) {
        should_free = SDL_TRUE;
    }

    SDL_UnlockMutex(g_ohos_window_registry_lock);
    if (should_free) {
        OHOS_WindowData_Free(data);
    }
}

static int OHOS_WindowRegistry_Register(SDL_WindowData *data, Uint32 id)
{
    OHOS_WindowRegistryEntry *entry = NULL;
    OHOS_WindowRegistryEntry *prev = NULL;
    OHOS_WindowRegistryEntry *new = NULL;

    SDL_assert(g_ohos_window_registry_lock != NULL);
    if (!data || id == 0) {
        return SDL_InvalidParamError("data || id");
    }

    new = (OHOS_WindowRegistryEntry *)SDL_calloc(1, sizeof(OHOS_WindowRegistryEntry));
    if (!new) {
        return SDL_OutOfMemory();
    }
    new->id = id;
    new->data = data;
    SDL_LockMutex(g_ohos_window_registry_lock);
    if (SDL_AtomicGet(&data->destroying)) {
        SDL_UnlockMutex(g_ohos_window_registry_lock);
        SDL_free(new);
        return SDL_SetError("WindowRegistry: data is being destroyed");
    }
    for (entry = g_ohos_window_registry; entry; entry = entry->next) {
        if (data == entry->data) {
            SDL_UnlockMutex(g_ohos_window_registry_lock);
            SDL_free(new);
            if (id == entry->id) {
                return 0;
            } else {
                return SDL_SetError("WindowRegistry: Duplicate registration");
            }
        } else if (id == entry->id) {
            return SDL_SetError("WindowRegistry: Duplicate registration");
        }
        if (data == entry->data || id == entry->id) {
        }
        prev = entry;
    }
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "WindowRegistry: The detection is complete, and no duplicate data was found.");
    data->ref_count = 1;
    SDL_AtomicSet(&data->destroying, SDL_FALSE);
    if (prev) {
        prev->next = new;
    } else {
        g_ohos_window_registry = new;
    }
    SDL_UnlockMutex(g_ohos_window_registry_lock);
    return 0;
}

static void OHOS_WindowRegistry_Unregister(Uint32 id)
{
    OHOS_WindowRegistryEntry *entry = NULL;
    OHOS_WindowRegistryEntry *prev = NULL;

    SDL_assert(g_ohos_window_registry_lock);
    if (id == 0) {
        return;
    }
    SDL_LockMutex(g_ohos_window_registry_lock);
    for (entry = g_ohos_window_registry; entry; entry = entry->next) {
        if (entry->id == id) {
            break;
        }
        prev = entry;
    }
    if (!entry) {
        SDL_UnlockMutex(g_ohos_window_registry_lock);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "WindowRegistry: No corresponding data found.");
        return;
    }
    SDL_AtomicSet(&entry->data->destroying, SDL_TRUE);
    if (prev) {
        prev->next = entry->next;
    } else {
        g_ohos_window_registry = entry->next;
    }
    SDL_UnlockMutex(g_ohos_window_registry_lock);
    SDL_free(entry);
    OHOS_WindowRegistry_Release(data);
}

static SDL_WindowData *OHOS_WindowRegistry_Acquire(Uint32 id)
{
    if (id == 0) {
        SDL_InvalidParamError("id");
        return NULL;
    }
    SDL_LockMutex(g_ohos_window_registry_lock);
    for (OHOS_WindowRegistryEntry *entry = g_ohos_window_registry; entry; entry = entry->next) {
        if (!entry) {
            break;
        }
        if (entry->id == id) {
            entry->data->ref_count++;
            SDL_UnlockMutex(g_ohos_window_registry_lock);
            return entry->data;
        }
    }
    SDL_UnlockMutex(g_ohos_window_registry_lock);
    SDL_SetError("No data matched.");
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "WindowRegistry: %s", SDL_GetError());
    return NULL;
}

/* Registery Tools end */

int OHOS_Tool_LockWindowData(SDL_WindowData *data)
{
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "LockWindowData: wait global data=%p", (void *)data);
    SDL_LockMutex(g_window_registry_lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "LockWindowData: got global data=%p entry=%p", (void *)data,
                 data ? (void *)data->registry_entry : NULL);
    if (!data || !data->registry_entry) {
        SDL_UnlockMutex(g_window_registry_lock);
        return SDL_InvalidParamError("data");
    }
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "LockWindowData: wait entry data=%p", (void *)data);
    SDL_LockMutex(data->registry_entry->lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "LockWindowData: got entry data=%p", (void *)data);
    SDL_UnlockMutex(g_window_registry_lock);
    return 0;
}

int OHOS_Tool_UnlockWidnowData(SDL_WindowData *data)
{
    SDL_LockMutex(g_window_registry_lock);
    if (!data || !data->registry_entry) {
        SDL_UnlockMutex(g_window_registry_lock);
        return SDL_InvalidParamError("data");
    }
    SDL_UnlockMutex(g_window_registry_lock);
    SDL_UnlockMutex(data->registry_entry->lock);
    return 0;
}

SDL_bool OHOS_Tool_HasFlag(Uint32 flags, Uint32 mask)
{
    return (flags & mask) ? SDL_TRUE : SDL_FALSE;
}

static int OHOS_Tool_ParseWindowRequest(SDL_Window *window, OHOS_WindowCommand cmd, OHOS_WindowRequest *request)
{
    int index;
    SDL_VideoDisplay *display = NULL;
    SDL_DisplayData *display_data = NULL;

    if (!window || !request) {
        return SDL_InvalidParamError("window / request");
    }

    if (cmd < OHOS_WINDOW_CMD_CREATE || cmd >= OHOS_WINDOW_CMD_COUNT) {
        return SDL_SetError("OHOS_Tool_ParseWindowRequest: invalid window command %d", (int)cmd);
    }

    SDL_zerop(request);

    request->cmd = cmd;

    request->sdl_window_id = window->id;

    request->title = SDL_strdup(window->title ? window->title : "");
    if (!request->title) {
        return SDL_OutOfMemory();
    }

    request->w = window->w;
    request->h = window->h;
    request->x = window->x;
    request->y = window->y;
    request->min_w = window->min_w;
    request->min_h = window->min_h;
    request->max_w = window->max_w;
    request->max_h = window->max_h;
    request->display_index = window->display_index;

    request->borderless = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_BORDERLESS);
    request->resizable = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_RESIZABLE);
    request->allow_highdpi = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_ALLOW_HIGHDPI);
    request->always_on_top = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_ALWAYS_ON_TOP);
    request->skip_taskbar = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_SKIP_TASKBAR);
    request->utility = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_UTILITY);
    request->tooltip = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_TOOLTIP);
    request->popup_menu = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_POPUP_MENU);
    request->fullscreen = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_FULLSCREEN);
    request->fullscreen_desktop = ((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) ? SDL_TRUE : SDL_FALSE;

    request->shown = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_SHOWN);
    request->hidden = OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_HIDDEN);

    request->opacity = window->opacity;
    request->brightness = window->brightness;

    return 0;
}

static void OHOS_Tool_DestroyWindowRequest(OHOS_WindowRequest *req)
{
    if (!req) {
        return;
    }
    if (req->title) {
        SDL_free((void *)req->title);
    }
    SDL_free(req);
}

static void OHOS_Tool_DestroyWindowData(SDL_WindowData *data)
{
    if (!data) {
        return;
    }
    if (data->registry_entry) {
        SDL_LockMutex(g_window_registry_lock);
        OHOS_Tool_UnregisterWindowData(data);
        SDL_UnlockMutex(g_window_registry_lock);
    }
    if (data->create_cond) {
        SDL_DestroyCond(data->create_cond);
    }
#ifdef SDL_VIDEO_OPENGL_EGL
    if (data->egl_lock) {
        SDL_DestroyMutex(data->egl_lock);
    }
#endif
    SDL_free(data);
}

static void TSF_CallWindowRequest(napi_env env, napi_value cb, void *context, void *data)
{
    OHOS_WindowRequest *req = (OHOS_WindowRequest *)data;
    napi_value obj = NULL;

    (void)context;

    if (!req) {
        return;
    }

    obj = Napi_CreateObj(env);
    if (!obj) {
        goto endfunc;
    }

    if (Napi_SetNamedProperty(env, obj, "cmd", Napi_CreateNumber(env, req->cmd)) ||
        Napi_SetNamedProperty(env, obj, "id", Napi_CreateNumber(env, req->sdl_window_id)) ||
        Napi_SetNamedProperty(env, obj, "title", Napi_CreateString(env, req->title ? req->title : ""))) {
        goto endfunc;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "title: %s", req->title);

    if (Napi_SetNamedProperty(env, obj, "x", Napi_CreateNumber(env, req->x)) ||
        Napi_SetNamedProperty(env, obj, "y", Napi_CreateNumber(env, req->y)) ||
        Napi_SetNamedProperty(env, obj, "w", Napi_CreateNumber(env, req->w)) ||
        Napi_SetNamedProperty(env, obj, "h", Napi_CreateNumber(env, req->h))) {
        goto endfunc;
    }

    if (Napi_SetNamedProperty(env, obj, "minW", Napi_CreateNumber(env, req->min_w)) ||
        Napi_SetNamedProperty(env, obj, "minH", Napi_CreateNumber(env, req->min_h)) ||
        Napi_SetNamedProperty(env, obj, "maxW", Napi_CreateNumber(env, req->max_w)) ||
        Napi_SetNamedProperty(env, obj, "maxH", Napi_CreateNumber(env, req->max_h)) ||
        Napi_SetNamedProperty(env, obj, "displayIndex", Napi_CreateNumber(env, req->display_index))) {
        goto endfunc;
    }

    if (Napi_SetNamedProperty(env, obj, "borderless", Napi_CreateBoolean(env, req->borderless)) ||
        Napi_SetNamedProperty(env, obj, "resizable", Napi_CreateBoolean(env, req->resizable)) ||
        Napi_SetNamedProperty(env, obj, "allowHighdpi", Napi_CreateBoolean(env, req->allow_highdpi)) ||
        Napi_SetNamedProperty(env, obj, "alwaysOnTop", Napi_CreateBoolean(env, req->always_on_top)) ||
        Napi_SetNamedProperty(env, obj, "skipTaskbar", Napi_CreateBoolean(env, req->skip_taskbar)) ||
        Napi_SetNamedProperty(env, obj, "utility", Napi_CreateBoolean(env, req->utility)) ||
        Napi_SetNamedProperty(env, obj, "tooltip", Napi_CreateBoolean(env, req->tooltip)) ||
        Napi_SetNamedProperty(env, obj, "popupMenu", Napi_CreateBoolean(env, req->popup_menu)) ||
        Napi_SetNamedProperty(env, obj, "fullscreen", Napi_CreateBoolean(env, req->fullscreen)) ||
        Napi_SetNamedProperty(env, obj, "fullscreenDesktop", Napi_CreateBoolean(env, req->fullscreen_desktop)) ||
        Napi_SetNamedProperty(env, obj, "shown", Napi_CreateBoolean(env, req->shown)) ||
        Napi_SetNamedProperty(env, obj, "hidden", Napi_CreateBoolean(env, req->hidden))) {
        goto endfunc;
    }

    if (Napi_SetNamedProperty(env, obj, "opacity", Napi_CreateNumber(env, req->opacity)) ||
        Napi_SetNamedProperty(env, obj, "brightness", Napi_CreateNumber(env, req->brightness))) {
        goto endfunc;
    }

    Napi_CallFunction(env, cb, 1, &obj);

endfunc:
    Napi_ClearPending(env, "TSF_CallWindowRequest");
}

napi_value OHOS_ThreadSafe_RegisterCallWindowRequest(napi_env env, napi_callback_info info)
{
    napi_value callback = NULL;

    if (Napi_CheckTSFCallbackArg(env, info, &callback)) {
        return NULL;
    }

    if (Napi_CreateTSF(env, callback, "OHOS_WindowRequest", TSF_CallWindowRequest, &window_request_tsf)) {
        return NULL;
    }

    return Napi_CreateBoolean(env, SDL_TRUE);
}

/* Napi */

static void OHOS_Napi_EnsureNodeAPI(void)
{
    if (!g_node_api) {
        OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE, ArkUI_NativeNodeAPI_1, g_node_api);
    }
}

static void OHOS_Napi_TryCompleteCreate(SDL_WindowData *data)
{
    if (!data) {
        SDL_InvalidParamError("data");
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%s", SDL_GetError());
        return;
    }

    SDL_LockMutex(data->comm_lock);
    if (!data->create_done && data->window_ready && data->attach_ready) {
        data->create_done = SDL_TRUE;
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "TryCompleteCreate: signaled");
        SDL_CondSignal(data->create_cond);
    }
    SDL_UnlockMutex(data->comm_lock);
}

static void OHOS_Napi_MarkAttachReady(SDL_WindowData *data)
{
    if (!data || data->destroying) {
        return;
    }
    SDL_LockMutex(data->comm_lock);
    data->attach_ready = SDL_TRUE;
    SDL_UnlockMutex(data->comm_lock);
    OHOS_Napi_TryCompleteCreate(data);
}

static void OHOS_Napi_MarkWindowReady(SDL_WindowData *data)
{
    if (!data) {
        SDL_InvalidParamError("data");
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%s", SDL_GetError());
        return;
    }
    SDL_LockMutex(data->comm_lock);
    data->window_ready = SDL_TRUE;
    SDL_UnlockMutex(data->comm_lock);

    OHOS_Napi_TryCompleteCreate(data);
}

napi_value OHOS_Napi_NotifyWindowReady(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    Uint32 id = 0;
    Uint32 window_id = 0;
    napi_status status;
    SDL_WindowData *data = NULL;

    if ((status = napi_get_cb_info(env, info, &argc, args, NULL, NULL)) != napi_ok) {
        Napi_ThrowError(env, status, "notifyWindowReady: get_cb_info failed");
        return NULL;
    }
    if (argc < 2 || args[0] == NULL || args[1] == NULL) {
        Napi_ThrowError(env, napi_invalid_arg, "notifyWindowReady: sdlWindowId or ohosWindowId is missing");
        return NULL;
    }
    if ((status = napi_get_value_uint32(env, args[0], &id)) != napi_ok) {
        Napi_ThrowError(env, status, "notifyWindowReady: sdlWindowId is not a number");
        return NULL;
    }
    if ((status = napi_get_value_uint32(env, args[1], &window_id)) != napi_ok) {
        Napi_ThrowError(env, status, "notifyWindowReady: ohosWindowId is not a number");
        return NULL;
    }

    data = OHOS_WindowRegistry_Acquire(id);
    if (!data) {
        Napi_ThrowError(env, napi_invalid_arg, "notifyWindowReady: SDL window data not found");
        return NULL;
    }
    SDL_LockMutex(data->comm_lock);
    data->window_id = window_id;
    SDL_UnlockMutex(data->comm_lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "notifyWindowReady: window %u ohos %u", id, window_id);
    OHOS_Napi_MarkWindowReady(data);
    OHOS_WindowRegistry_Release(data);

    return Napi_CreateUndefined(env);
}

static void OnSurfaceCreatedCallback(OH_ArkUI_SurfaceHolder *holder)
{
    OHNativeWindow *native_window = NULL;
    SDL_WindowData *data = NULL;
    Uint32 id;
    SDL_Event event;

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Surface创建回调被触发");
    id = (Uint32)(intptr_t)(OH_ArkUI_SurfaceHolder_GetUserData(holder));
    if (!(native_window = OH_ArkUI_XComponent_GetNativeWindow(holder))) {
        return;
    }
    event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    event.user.code = OHOS_WINDOW_EVENT_SURFACE_CREATED;
    event.user.windowID = id;
    SDL_LockMutex(g_window_registry_lock);
    if (!(data = OHOS_Tool_QueueWindowData(id))) {
        SDL_UnlockMutex(g_window_registry_lock);
        return;
    }
    SDL_LockMutex(data->registry_entry->lock);
    SDL_UnlockMutex(g_window_registry_lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OnSurfaceCreated: window %u native=%p", id, (void *)native_window);
    data->native_window = native_window;
    {
        int pushed = SDL_PushEvent(&event);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "OnSurfaceCreated: push event result=%d type=%u base=%u",
                     pushed, event.type, g_ohos_event_base);
    }
    OHOS_SendWakeupEvent(data->video_device, NULL);
    SDL_UnlockMutex(data->registry_entry->lock);
}

static void OnSurfaceChangedCallback(OH_ArkUI_SurfaceHolder *holder, uint64_t width, uint64_t height)
{
    Uint32 id;
    SDL_WindowData *data;
    SDL_Event event;

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    event.user.windowID = id;
    event.user.code = OHOS_WINDOW_EVENT_SURFACE_CHANGED;
    event.user.data1 = (void *)(uintptr_t)width;
    event.user.data2 = (void *)(uintptr_t)height;
    SDL_LockMutex(g_window_registry_lock);
    data = OHOS_Tool_QueueWindowData(id);
    if (!data) {
        SDL_UnlockMutex(g_window_registry_lock);
        return;
    }
    SDL_LockMutex(data->registry_entry->lock);
    SDL_UnlockMutex(g_window_registry_lock);
    data->real_surface_w = width;
    data->real_surface_h = height;
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OnSurfaceChanged: window %u %ux%u", id, (unsigned int)width, (unsigned int)height);
    {
        int pushed = SDL_PushEvent(&event);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                     "OnSurfaceChanged: push event result=%d type=%u base=%u",
                     pushed, event.type, g_ohos_event_base);
    }
    OHOS_SendWakeupEvent(data->video_device, NULL);
    SDL_UnlockMutex(data->registry_entry->lock);
}

static void OnSurfaceDestroyedCallback(OH_ArkUI_SurfaceHolder *holder)
{
    Uint32 id;
    SDL_WindowData *data;
    SDL_Event event;

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    data->native_window = NULL;
    event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    event.user.code = OHOS_WINDOW_EVENT_SURFACE_DESTROYED;
    event.user.windowID = id;
    SDL_LockMutex(g_window_registry_lock);
    data = OHOS_Tool_QueueWindowData(id);
    if (!data) {
        SDL_UnlockMutex(g_window_registry_lock);
        return;
    }
    SDL_LockMutex(data->registry_entry->lock);
    SDL_UnlockMutex(g_window_registry_lock);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OnSurfaceDestroyed: window %u", id);
    SDL_PushEvent(&event);

    OHOS_SendWakeupEvent(data->video_device, NULL);
    SDL_UnlockMutex(data->registry_entry->lock);
}

static void OnSurfaceShowCallback(OH_ArkUI_SurfaceHolder *holder) {}

static void OnSurfaceHideCallback(OH_ArkUI_SurfaceHolder *holder) {}

napi_value OHOS_Napi_AttachWindowSurface(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    ArkUI_NodeHandle node = NULL;
    Uint32 id = 0;
    napi_status status;
    OH_ArkUI_SurfaceHolder *holder = NULL;
    SDL_WindowData *data = NULL;
    OH_ArkUI_SurfaceCallback *callback = NULL;

    status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok) {
        Napi_ThrowError(env, status, "attachWindowSurface: get_cb_info failed");
        return NULL;
    }
    if (argc < 2 || args[0] == NULL || args[1] == NULL) {
        Napi_ThrowError(env, napi_invalid_arg, "attachWindowSurface: node or windowId is missing");
        return NULL;
    }
    OHOS_Napi_EnsureNodeAPI();
    if (OH_ArkUI_GetNodeHandleFromNapiValue(env, args[0], &node) != ARKUI_ERROR_CODE_NO_ERROR || node == NULL) {
        Napi_ThrowError(env, napi_generic_failure, "attachWindowSurface: invalid FrameNode");
        return NULL;
    }
    status = napi_get_value_uint32(env, args[1], &id);
    if (status != napi_ok) {
        Napi_ThrowError(env, status, "attachWindowSurface: windowId is not a number");
        return NULL;
    }
    if (!(holder = OH_ArkUI_SurfaceHolder_Create(node))) {
        Napi_ThrowError(env, napi_invalid_arg, "attachWindowSurface: failed to create surfaceHolder");
        return NULL;
    }
    if (!(callback = OH_ArkUI_SurfaceCallback_Create())) {
        OH_ArkUI_SurfaceHolder_Dispose(holder);
        Napi_ThrowError(env, napi_invalid_arg, "attachWindowSurface: failed to create surfaceHolderCallback");
        return NULL;
    }
    SDL_LockMutex(g_registry_lock);
    data = OHOS_WindowRegistry_Acquire(id);
    SDL_UnlockMutex(g_registry_lock);
    if (!data) {
        OH_ArkUI_SurfaceCallback_Dispose(callback);
        OH_ArkUI_SurfaceHolder_Dispose(holder);
        Napi_ThrowError(env, napi_invalid_arg, "attachWindowSurface: failed to locate SDL window");
        return NULL;
    }
    OH_ArkUI_SurfaceHolder_SetUserData(holder, (void *)(intptr_t)id);
    OH_ArkUI_SurfaceCallback_SetSurfaceCreatedEvent(callback, OnSurfaceCreatedCallback);
    OH_ArkUI_SurfaceCallback_SetSurfaceChangedEvent(callback, OnSurfaceChangedCallback);
    OH_ArkUI_SurfaceCallback_SetSurfaceDestroyedEvent(callback, OnSurfaceDestroyedCallback);
    OH_ArkUI_SurfaceCallback_SetSurfaceShowEvent(callback, OnSurfaceShowCallback);
    OH_ArkUI_SurfaceCallback_SetSurfaceHideEvent(callback, OnSurfaceHideCallback);
    OH_ArkUI_SurfaceHolder_AddSurfaceCallback(holder, callback);
    SDL_LockMutex(data->comm_lock);
    data->holder = holder;
    data->surface_callback = callback;
    SDL_UnlockMutex(data->comm_lock);

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "attachWindowSurface: holder created for window %u", id);
    OHOS_Napi_MarkAttachReady(data);

    return Napi_CreateUndefined(env);
}

napi_value OHOS_Napi_DetachWindowSurface(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { NULL };
    Uint32 window_id = 0;
    napi_status status;
    SDL_WindowData *data = NULL;

    status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok) {
        Napi_ThrowError(env, status, "detachWindowSurface: get_cb_info failed");
        return NULL;
    }
    if (argc < 1 || args[0] == NULL) {
        Napi_ThrowError(env, napi_invalid_arg, "detachWindowSurface: windowId is missing");
        return NULL;
    }
    status = napi_get_value_uint32(env, args[0], &window_id);
    if (status != napi_ok) {
        Napi_ThrowError(env, status, "detachWindowSurface: windowId is not a number");
        return NULL;
    }

    SDL_LockMutex(g_window_registry_lock);
    data = OHOS_Tool_QueueWindowData(window_id);
    if (!data) {
        SDL_UnlockMutex(g_window_registry_lock);
        return Napi_CreateUndefined(env);
    }
    SDL_LockMutex(data->registry_entry->lock);
    SDL_UnlockMutex(g_window_registry_lock);
    if (data->holder && data->surface_callback) {
        OH_ArkUI_SurfaceHolder_RemoveSurfaceCallback(data->holder, data->surface_callback);
    }
    if (data->surface_callback) {
        OH_ArkUI_SurfaceCallback_Dispose(data->surface_callback);
        data->surface_callback = NULL;
    }
    if (data->holder) {
        OH_ArkUI_SurfaceHolder_Dispose(data->holder);
        data->holder = NULL;
    }
    data->native_window = NULL;
    SDL_UnlockMutex(data->registry_entry->lock);

    // TODO: 在被动销毁时候应该如何通知

    return Napi_CreateUndefined(env);
}

/* Napi end */

/* Hooks for SDL2 */

int OHOS_CreateWindow(_THIS, SDL_Window *window)
{
    int retval = 0;
    SDL_WindowData *data = NULL;
    OHOS_WindowRequest *req = NULL;
    Uint32 start_time;

    SDL_assert(_this != NULL && window != NULL);

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!g_registry_lock) {
        g_registry_lock = SDL_CreateMutex();
    }
    if (!data || !req || !g_registry_lock) {
        retval = SDL_OutOfMemory();
        goto errfunc;
    }
#ifdef SDL_VIDEO_OPENGL_EGL
    data->egl_lock = SDL_CreateMutex();
    if (!data->egl_lock) {
        retval = SDL_OutOfMemory();
        goto errfunc;
    }
    data->current_surface = EGL_NO_SURFACE;
    data->pending_surface = EGL_NO_SURFACE;
    data->pbuffer_surface = EGL_NO_SURFACE;
#endif
    data->comm_lock = SDL_CreateMutex();
    data->create_cond = SDL_CreateCond();
    if (!data->create_cond || !data->comm_lock) {
        retval = SDL_OutOfMemory();
        goto errfunc;
    }

    SDL_LockMutex(data->comm_lock);
    data->id = window->id;
    data->video_device = _this;
    data->ref_count = 1;
    data->destroying = SDL_FALSE;
    data->create_flags = window->flags;
    window->driverdata = (void *)data;
    SDL_UnlockMutex(data->comm_lock);
    SDL_LockMutex(g_registry_lock);
    retval = OHOS_WindowRegistry_Register(data, window->id);
    if (retval) {
        SDL_UnlockMutex(g_window_registry_lock);
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Create: %s", SDL_GetError());
        goto errfunc;
    }
    SDL_UnlockMutex(g_registry_lock);

    retval = OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_CREATE, req);
    if (retval) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_CreateWindow: %s", SDL_GetError());
        goto errfunc;
    }
    retval = Napi_CallThreadSafeFunction(window_request_tsf, req);
    if (retval) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_CreateWindow: %s", SDL_GetError());
        goto errfunc;
    }
    start_time = SDL_GetTicks();
    SDL_LockMutex(data->comm_lock);
    while (!data->create_done) {
        Uint32 elapsed = SDL_GetTicks() - start_time;
        if (elapsed >= 5000) {
            break;
        }
        if (SDL_CondWaitTimeout(data->create_cond, data->comm_lock, 5000 - elapsed) == SDL_MUTEX_TIMEDOUT) {
            break;
        }
    }
    if (!data->create_done) {
        SDL_UnlockMutex(data->comm_lock);
        retval = SDL_SetError("OHOS Window creation timed out");
        goto errfunc;
    }
    SDL_UnlockMutex(data->comm_lock);
    OHOS_Tool_DestroyWindowRequest(req);
    return retval;
errfunc:
    OHOS_WindowData_Destroy(data);
    OHOS_Tool_DestroyWindowRequest(req);
    return retval;
}

int OHOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "CreateWindowFrom not supported");
    return -1;
}

void OHOS_SetWindowTitle(_THIS, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_TITLE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_SetWindowIcon(SDL_VideoDevice *_this, SDL_Window *window, SDL_Surface *icon)
{
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "SetWindowIcon not surported");
}

void OHOS_SetWindowPosition(_THIS, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_POSITION, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_SetWindowSize(_THIS, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_SIZE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_SetWindowMinimumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_MINIMUM_SIZE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_SetWindowMaximumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

int OHOS_GetWindowBordersSize(SDL_VideoDevice *_this, SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
    SDL_WindowData *data;

    SDL_assert(_this != NULL);
    data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return SDL_InvalidParamError("window");
    }
    OHOS_Tool_LockWindowData(data);
    *top = data->border_top;
    *left = data->border_left;
    *bottom = data->border_bottom;
    *right = data->border_right;
    OHOS_Tool_UnlockWidnowData(data);
}

void OHOS_GetWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *w, int *h)
{
    SDL_WindowData *data;
    int tw, th;

    SDL_assert(_this != NULL);
    data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        SDL_InvalidParamError("window");
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%s", SDL_GetError());
        return;
    }
    tw = window->w;
    th = window->h;
    if (OHOS_Tool_HasFlag(window->flags, SDL_WINDOW_ALLOW_HIGHDPI)) {
        float density = OHOS_Display_GetDensityPixels(window);
        tw *= density;
        th *= density;
    }
    *w = tw;
    *h = th;
}

int OHOS_SetWindowOpacity(SDL_VideoDevice *_this, SDL_Window *window, float opacity) { return -1; }

int OHOS_SetWindowModalFor(SDL_VideoDevice *_this, SDL_Window *modal_window, SDL_Window *parent_window) { return -1; }

int OHOS_SetWindowInputFocus(SDL_VideoDevice *_this, SDL_Window *window) { return -1; }

void OHOS_ShowWindow(_THIS, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SHOW, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_HideWindow(_THIS, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_HIDE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window) { return; }

void OHOS_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_MAXIMIZE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_MINIMIZE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window) { return; }

void OHOS_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool bordered)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_BORDERED, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }
    req->borderless = bordered ? SDL_FALSE : SDL_TRUE;

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable)
{
    OHOS_WindowRequest *req = NULL;
    if (!_this || !window) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_RESIZABLE, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    req->resizable = resizable ? SDL_TRUE : SDL_FALSE;
    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

void OHOS_SetWindowAlwaysOnTop(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool on_top) { return; }

void OHOS_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen) {}

int OHOS_SetWindowGammaRamp(SDL_VideoDevice *_this, SDL_Window *window, const Uint16 *ramp) { return -1; }

int OHOS_GetWindowGammaRamp(SDL_VideoDevice *_this, SDL_Window *window, Uint16 *ramp) { return -1; }

void *OHOS_GetWindowICCProfile(_THIS, SDL_Window *window, size_t *size) { return NULL; }

int OHOS_GetWindowDisplayIndex(SDL_VideoDevice *_this, SDL_Window *window) { return -1; }

void OHOS_SetWindowMouseRect(SDL_VideoDevice *_this, SDL_Window *window) { return; }

void OHOS_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed) { return; }

void OHOS_SetWindowKeyboardGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed) { return; }

void OHOS_DestroyWindow(_THIS, SDL_Window *window)
{
    OHOS_WindowRequest *req = NULL;
    SDL_WindowData *data;
    if (!_this || !window || !(data = (SDL_WindowData *)window->driverdata)) {
        SDL_SetError("invalid _this || window");
        return;
    }

    req = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (!req) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_DESTROY, req)) {
        SDL_free(req);
        req = NULL;
        return;
    }

    if (Napi_CallThreadSafeFunction(window_request_tsf, req)) {
        SDL_free((void *)req->title);
        req = NULL;
        return;
    }
}

int OHOS_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, Uint32 *format, void **pixels, int *pitch) { return -1; }

int OHOS_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects) { return -1; }

void OHOS_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window) { return; }

void OHOS_OnWindowEnter(SDL_VideoDevice *_this, SDL_Window *window) { return; }

int OHOS_FlashWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_FlashOperation operation) { return -1; }

void OHOS_Window_HandleEvents(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    SDL_WindowData *data;

    if (!(window = SDL_GetWindowFromID(event->user.windowID))) {
        SDL_SetError("invalid windowId");
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Window_HandleEvents: %s", SDL_GetError());
        return;
    }

    data = (SDL_WindowData *)window->driverdata;

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                 "OHOS_Window_HandleEvents: window=%u code=%u native=%p ctx=%p current=%p pending=%p pbuffer=%p",
                 event->user.windowID, event->user.code,
                 data ? (void *)data->native_window : NULL,
                 data ? (void *)data->current_context : NULL,
                 data ? (void *)data->current_surface : NULL,
                 data ? (void *)data->pending_surface : NULL,
                 data ? (void *)data->pbuffer_surface : NULL);

    switch (event->user.code) {
    case OHOS_WINDOW_EVENT_SURFACE_CREATED:
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OHOS_Window_HandleEvents: SURFACE_CREATED case");
        if (OHOS_GL_PreparePendingSurface(_this, window) || OHOS_GL_ActivatePendingSurface(_this, window)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "OHOS_WINDOW_EVENT_SURFACE_CREATED: %s", SDL_GetError());
        }
        break;
    case OHOS_WINDOW_EVENT_SURFACE_CHANGED:
    {
        Uint64 pw = (Uint64)(uintptr_t)event->user.data1;
        Uint64 ph = (Uint64)(uintptr_t)event->user.data2;
        int w, h;

        if (pw == 0 || ph == 0) {
            break;
        }
        if (pw > INT_MAX || ph > INT_MAX) {
            break;
        }

        if (window->flags & SDL_WINDOW_ALLOW_HIGHDPI) {
            float density = OHOS_Display_GetDensityPixels(window);
            if (density <= 0.0f) {
                density = 1.0f;
            }
            w = (int)SDL_lroundf((float)pw / density);
            h = (int)SDL_lroundf((float)ph / density);
        } else {
            w = (int)pw;
            h = (int)ph;
        }
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, w, h);
        break;
    }
    case OHOS_WINDOW_EVENT_SURFACE_DESTROYED:
        if (OHOS_GL_DeactivatePendingSurface(_this, window)) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OHOS_WINDOW_EVENT_SURFACE_DESTROYED: %s", SDL_GetError());
        }
        break;
    }
}
/* Hooks for SDL2 end */

#endif

/* vi: set ts=4 sw=4 expandtab: */