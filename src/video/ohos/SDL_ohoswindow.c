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
#include <window_manager/oh_window.h>
#include <arkui/native_interface.h>
#include <arkui/native_node.h>
#include <arkui/native_node_napi.h>

#include "SDL_ohosevents.h"
#include "SDL_timer.h"
#include "../../events/SDL_windowevents_c.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "SDL_ohosgles.h"
#endif

#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosevents.h"

typedef struct OHOS_WindowRegistryEntry
{
    Uint32 id;
    SDL_WindowData *data;
    struct OHOS_WindowRegistryEntry *next;
} OHOS_WindowRegistryEntry;

typedef enum OHOS_WindowCommand
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

typedef struct
{
    OHOS_WindowCommand cmd;
    Uint32 id;

    const char *title;
    int x;
    int y;
    int w;
    int h;
    int min_w;
    int min_h;
    int max_w;
    int max_h;
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

static napi_threadsafe_function g_window_request_tsf = NULL;

static int SDL_UNUSED OHOS_Tool_ParseWindowRequest(SDL_Window *window, OHOS_WindowCommand cmd, OHOS_WindowRequest *request)
{
    if (!window || !request) {
        return SDL_InvalidParamError("window/request");
    }

    SDL_zero(*request);

    request->cmd = cmd;
    request->id = window->id;

    if (cmd == OHOS_WINDOW_CMD_CREATE ||
        cmd == OHOS_WINDOW_CMD_SET_TITLE) {
        request->title = SDL_strdup(window->title ? window->title : "");
        if (!request->title) {
            return SDL_OutOfMemory();
        }
    }

    request->x = window->x;
    request->y = window->y;
    request->w = window->w;
    request->h = window->h;
    request->min_w = window->min_w;
    request->min_h = window->min_h;
    request->max_w = window->max_w;
    request->max_h = window->max_h;
    request->display_index = window->display_index;

    request->borderless = (window->flags & SDL_WINDOW_BORDERLESS) ? SDL_TRUE : SDL_FALSE;
    request->resizable = (window->flags & SDL_WINDOW_RESIZABLE) ? SDL_TRUE : SDL_FALSE;
    request->allow_highdpi = (window->flags & SDL_WINDOW_ALLOW_HIGHDPI) ? SDL_TRUE : SDL_FALSE;
    request->always_on_top = (window->flags & SDL_WINDOW_ALWAYS_ON_TOP) ? SDL_TRUE : SDL_FALSE;
    request->skip_taskbar = (window->flags & SDL_WINDOW_SKIP_TASKBAR) ? SDL_TRUE : SDL_FALSE;
    request->utility = (window->flags & SDL_WINDOW_UTILITY) ? SDL_TRUE : SDL_FALSE;
    request->tooltip = (window->flags & SDL_WINDOW_TOOLTIP) ? SDL_TRUE : SDL_FALSE;
    request->popup_menu = (window->flags & SDL_WINDOW_POPUP_MENU) ? SDL_TRUE : SDL_FALSE;
    request->fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? SDL_TRUE : SDL_FALSE;
    request->fullscreen_desktop =
        ((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) ? SDL_TRUE : SDL_FALSE;
    request->shown = (window->flags & SDL_WINDOW_SHOWN) ? SDL_TRUE : SDL_FALSE;
    request->hidden = (window->flags & SDL_WINDOW_HIDDEN) ? SDL_TRUE : SDL_FALSE;

    request->opacity = window->opacity;
    request->brightness = window->brightness;

    return 0;
}

static void OHOS_Tool_DestroyWindowRequest(OHOS_WindowRequest *request)
{
    if (!request) {
        return;
    }

    if (request->title) {
        SDL_free((void *)request->title);
    }

    SDL_free(request);
}

static napi_value OHOS_WindowRequest_ToNapiObject(napi_env env, const OHOS_WindowRequest *request)
{
    napi_value object;

    if (!request) {
        return NULL;
    }

    object = Napi_CreateObj(env);
    if (!object) {
        return NULL;
    }

    if (Napi_SetNamedProperty(env, object, "cmd", Napi_CreateNumber(env, request->cmd)) ||
        Napi_SetNamedProperty(env, object, "id", Napi_CreateNumber(env, request->id)) ||
        Napi_SetNamedProperty(env, object, "title", Napi_CreateString(env, request->title ? request->title : "")) ||
        Napi_SetNamedProperty(env, object, "x", Napi_CreateNumber(env, request->x)) ||
        Napi_SetNamedProperty(env, object, "y", Napi_CreateNumber(env, request->y)) ||
        Napi_SetNamedProperty(env, object, "w", Napi_CreateNumber(env, request->w)) ||
        Napi_SetNamedProperty(env, object, "h", Napi_CreateNumber(env, request->h)) ||
        Napi_SetNamedProperty(env, object, "minW", Napi_CreateNumber(env, request->min_w)) ||
        Napi_SetNamedProperty(env, object, "minH", Napi_CreateNumber(env, request->min_h)) ||
        Napi_SetNamedProperty(env, object, "maxW", Napi_CreateNumber(env, request->max_w)) ||
        Napi_SetNamedProperty(env, object, "maxH", Napi_CreateNumber(env, request->max_h)) ||
        Napi_SetNamedProperty(env, object, "displayIndex", Napi_CreateNumber(env, request->display_index)) ||
        Napi_SetNamedProperty(env, object, "borderless", Napi_CreateBoolean(env, request->borderless)) ||
        Napi_SetNamedProperty(env, object, "resizable", Napi_CreateBoolean(env, request->resizable)) ||
        Napi_SetNamedProperty(env, object, "allowHighdpi", Napi_CreateBoolean(env, request->allow_highdpi)) ||
        Napi_SetNamedProperty(env, object, "alwaysOnTop", Napi_CreateBoolean(env, request->always_on_top)) ||
        Napi_SetNamedProperty(env, object, "skipTaskbar", Napi_CreateBoolean(env, request->skip_taskbar)) ||
        Napi_SetNamedProperty(env, object, "utility", Napi_CreateBoolean(env, request->utility)) ||
        Napi_SetNamedProperty(env, object, "tooltip", Napi_CreateBoolean(env, request->tooltip)) ||
        Napi_SetNamedProperty(env, object, "popupMenu", Napi_CreateBoolean(env, request->popup_menu)) ||
        Napi_SetNamedProperty(env, object, "fullscreen", Napi_CreateBoolean(env, request->fullscreen)) ||
        Napi_SetNamedProperty(env, object, "fullscreenDesktop", Napi_CreateBoolean(env, request->fullscreen_desktop)) ||
        Napi_SetNamedProperty(env, object, "shown", Napi_CreateBoolean(env, request->shown)) ||
        Napi_SetNamedProperty(env, object, "hidden", Napi_CreateBoolean(env, request->hidden)) ||
        Napi_SetNamedProperty(env, object, "opacity", Napi_CreateNumber(env, request->opacity)) ||
        Napi_SetNamedProperty(env, object, "brightness", Napi_CreateNumber(env, request->brightness))) {
        return NULL;
    }

    return object;
}

static void OHOS_WindowRequest_CallJS(napi_env env, napi_value js_callback, void *context, void *data)
{
    OHOS_WindowRequest *request = (OHOS_WindowRequest *)data;
    napi_value object = NULL;

    (void)context;

    if (!env || !request) {
        OHOS_Tool_DestroyWindowRequest(request);
        return;
    }

    object = OHOS_WindowRequest_ToNapiObject(env, request);
    if (object) {
        Napi_CallFunction(env, js_callback, 1, &object);
    }

    OHOS_Tool_DestroyWindowRequest(request);
}

napi_value OHOS_WindowRequest_RegisterCallback(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { NULL };
    napi_value resource_name = NULL;
    napi_value result = NULL;
    napi_valuetype type;
    napi_status status;

    if (napi_get_cb_info(env, info, &argc, args, NULL, NULL) != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "registerWindowRequestCallback: invalid arguments");
        return NULL;
    }

    if (napi_typeof(env, args[0], &type) != napi_ok || type != napi_function) {
        napi_throw_error(env, NULL, "registerWindowRequestCallback: callback is not a function");
        return NULL;
    }

    if (g_window_request_tsf) {
        napi_release_threadsafe_function(g_window_request_tsf, napi_tsfn_abort);
        g_window_request_tsf = NULL;
    }

    status = napi_create_string_utf8(env, "OHOS_WindowRequest", NAPI_AUTO_LENGTH, &resource_name);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "registerWindowRequestCallback: create resource name failed");
        return NULL;
    }

    status = napi_create_threadsafe_function(
        env,
        args[0],
        NULL,
        resource_name,
        0,
        1,
        NULL,
        NULL,
        NULL,
        OHOS_WindowRequest_CallJS,
        &g_window_request_tsf);
    if (status != napi_ok) {
        g_window_request_tsf = NULL;
        napi_throw_error(env, NULL, "registerWindowRequestCallback: create threadsafe function failed");
        return NULL;
    }

    napi_get_boolean(env, true, &result);
    return result;
}

static SDL_mutex *g_ohos_window_registry_lock = NULL;
static OHOS_WindowRegistryEntry *g_ohos_window_registry_list = NULL;

typedef struct OHOS_SurfaceHolderEntry
{
    Uint32 id;
    OH_ArkUI_SurfaceHolder *holder;
    OH_ArkUI_SurfaceCallback *callback;
    struct OHOS_SurfaceHolderEntry *next;
} OHOS_SurfaceHolderEntry;

static OHOS_SurfaceHolderEntry *g_ohos_surface_holders = NULL;
static SDL_mutex *g_ohos_surface_holder_lock = NULL;

static int OHOS_SurfaceHolder_Add(Uint32 id, OH_ArkUI_SurfaceHolder *holder, OH_ArkUI_SurfaceCallback *callback)
{
    OHOS_SurfaceHolderEntry *entry;
    OHOS_SurfaceHolderEntry *it;

    if (!g_ohos_surface_holder_lock) {
        return SDL_SetError("surface holder registry not initialized");
    }

    entry = (OHOS_SurfaceHolderEntry *)SDL_calloc(1, sizeof(*entry));
    if (!entry) {
        return SDL_OutOfMemory();
    }

    entry->id = id;
    entry->holder = holder;
    entry->callback = callback;

    SDL_LockMutex(g_ohos_surface_holder_lock);

    for (it = g_ohos_surface_holders; it; it = it->next) {
        if (it->id == id) {
            SDL_UnlockMutex(g_ohos_surface_holder_lock);
            SDL_free(entry);
            return SDL_SetError("surface holder already attached");
        }
    }

    entry->next = g_ohos_surface_holders;
    g_ohos_surface_holders = entry;

    SDL_UnlockMutex(g_ohos_surface_holder_lock);
    return 0;
}
static int OHOS_SurfaceHolder_Remove(Uint32 id, OH_ArkUI_SurfaceHolder **holder, OH_ArkUI_SurfaceCallback **callback)
{
    OHOS_SurfaceHolderEntry *entry;
    OHOS_SurfaceHolderEntry *prev = NULL;

    if (!g_ohos_surface_holder_lock) {
        return SDL_SetError("surface holder registry not initialized");
    }

    SDL_LockMutex(g_ohos_surface_holder_lock);

    for (entry = g_ohos_surface_holders; entry; entry = entry->next) {
        if (entry->id == id) {
            break;
        }
        prev = entry;
    }

    if (!entry) {
        SDL_UnlockMutex(g_ohos_surface_holder_lock);
        return SDL_SetError("surface holder not found");
    }

    if (prev) {
        prev->next = entry->next;
    } else {
        g_ohos_surface_holders = entry->next;
    }

    *holder = entry->holder;
    *callback = entry->callback;
    SDL_free(entry);

    SDL_UnlockMutex(g_ohos_surface_holder_lock);
    return 0;
}

static void OHOS_WindowData_Free(SDL_WindowData *data)
{
    if (!data) {
        return;
    }

    if (data->create_sem) {
        SDL_DestroySemaphore(data->create_sem);
    }

    SDL_free(data);
}

static void OHOS_WindowRegistry_Release(SDL_WindowData *data)
{
    SDL_bool should_free = SDL_FALSE;

    if (!data || !g_ohos_window_registry_lock) {
        return;
    }

    SDL_LockMutex(g_ohos_window_registry_lock);

    if (data->ref_count == 0) {
        SDL_UnlockMutex(g_ohos_window_registry_lock);
        SDL_assert(!"window data ref_count underflow");
        return;
    }

    data->ref_count--;

    if (data->ref_count == 0 && data->destroying) {
        should_free = SDL_TRUE;
    }

    SDL_UnlockMutex(g_ohos_window_registry_lock);

    if (should_free) {
        OHOS_WindowData_Free(data);
    }
}

static int OHOS_WindowRegistry_Register(SDL_WindowData *data, Uint32 id)
{
    OHOS_WindowRegistryEntry *entry;
    OHOS_WindowRegistryEntry *it;

    if (!data || id == 0) {
        return SDL_InvalidParamError("data/id");
    }
    if (!g_ohos_window_registry_lock) {
        return SDL_SetError("window registry not initialized");
    }

    entry = (OHOS_WindowRegistryEntry *)SDL_calloc(1, sizeof(*entry));
    if (!entry) {
        return SDL_OutOfMemory();
    }

    SDL_LockMutex(g_ohos_window_registry_lock);

    for (it = g_ohos_window_registry_list; it; it = it->next) {
        if (it->id == id || it->data == data) {
            SDL_UnlockMutex(g_ohos_window_registry_lock);
            SDL_free(entry);
            return SDL_SetError("window already registered");
        }
    }

    data->ref_count = 1;
    data->destroying = SDL_FALSE;

    entry->id = id;
    entry->data = data;
    entry->next = g_ohos_window_registry_list;
    g_ohos_window_registry_list = entry;

    SDL_UnlockMutex(g_ohos_window_registry_lock);
    return 0;
}

static void OHOS_WindowRegistry_Unregister(Uint32 id)
{
    OHOS_WindowRegistryEntry *entry;
    OHOS_WindowRegistryEntry *prev = NULL;
    SDL_WindowData *data;

    if (id == 0 || !g_ohos_window_registry_lock) {
        return;
    }

    SDL_LockMutex(g_ohos_window_registry_lock);

    for (entry = g_ohos_window_registry_list; entry; entry = entry->next) {
        if (entry->id == id) {
            break;
        }
        prev = entry;
    }

    if (!entry) {
        SDL_UnlockMutex(g_ohos_window_registry_lock);
        return;
    }

    data = entry->data;
    data->destroying = SDL_TRUE;

    if (prev) {
        prev->next = entry->next;
    } else {
        g_ohos_window_registry_list = entry->next;
    }

    SDL_UnlockMutex(g_ohos_window_registry_lock);

    SDL_free(entry);
    OHOS_WindowRegistry_Release(data);
}

static SDL_WindowData *OHOS_WindowRegistry_Acquire(Uint32 id)
{
    OHOS_WindowRegistryEntry *entry;

    if (id == 0 || !g_ohos_window_registry_lock) {
        return NULL;
    }

    SDL_LockMutex(g_ohos_window_registry_lock);

    for (entry = g_ohos_window_registry_list; entry; entry = entry->next) {
        if (entry->id == id) {
            if (entry->data->destroying) {
                SDL_UnlockMutex(g_ohos_window_registry_lock);
                return NULL;
            }

            entry->data->ref_count++;
            SDL_UnlockMutex(g_ohos_window_registry_lock);
            return entry->data;
        }
    }

    SDL_UnlockMutex(g_ohos_window_registry_lock);
    return NULL;
}

static void OHOS_SurfaceHolder_OnCreatedCB(OH_ArkUI_SurfaceHolder *holder)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    OHOS_GLWindowSurface *surface;
    OHNativeWindow *native_window;
    SDL_Event event;
    Uint32 id;
    Uint32 generation;

    if (!holder) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        return;
    }

    native_window = OH_ArkUI_XComponent_GetNativeWindow(holder);
    if (!native_window) {
        return;
    }

    surface = OHOS_GLRegistry_Acquire(id);
    if (!surface) {
        return;
    }

    generation = (Uint32)SDL_AtomicAdd(&surface->surface_generation, 1) + 1;
    OHOS_GLRegistry_Release(surface);

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_CREATED;
    event.user.windowID = id;
    event.user.data1 = (void *)(uintptr_t)generation;
    event.user.data2 = (void *)native_window;
    SDL_PushEvent(&event);
#endif
}
static void OHOS_SurfaceHolder_OnChangedCB(OH_ArkUI_SurfaceHolder *holder, Uint64 width, Uint64 height)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    OHOS_GLWindowSurface *surface;
    SDL_Event event;
    Uint32 id;
    Uint32 generation;

    if (!holder || width == 0 || height == 0) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        return;
    }

    surface = OHOS_GLRegistry_Acquire(id);
    if (!surface) {
        return;
    }

    SDL_AtomicSet(&surface->window_surface_ready, SDL_TRUE);
    SDL_AtomicSet(&surface->drawable_width, (int)width);
    SDL_AtomicSet(&surface->drawable_height, (int)height);
    generation = (Uint32)SDL_AtomicGet(&surface->surface_generation);
    OHOS_GLRegistry_Release(surface);

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_CHANGED;
    event.user.windowID = id;
    event.user.data1 = (void *)(uintptr_t)generation;
    event.user.data2 = NULL;
    SDL_PushEvent(&event);
#endif
}
static void OHOS_SurfaceHolder_OnDestroyedCB(OH_ArkUI_SurfaceHolder *holder)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    OHOS_GLWindowSurface *surface;
    SDL_Event event;
    Uint32 id;
    Uint32 generation;

    if (!holder) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        return;
    }

    surface = OHOS_GLRegistry_Acquire(id);
    if (!surface) {
        return;
    }

    generation = (Uint32)SDL_AtomicAdd(&surface->surface_generation, 1) + 1;
    SDL_AtomicSet(&surface->window_surface_ready, SDL_FALSE);
    OHOS_GLRegistry_Release(surface);

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_DESTROYED;
    event.user.windowID = id;
    event.user.data1 = (void *)(uintptr_t)generation;
    event.user.data2 = NULL;
    SDL_PushEvent(&event);
#endif
}
static void OHOS_SurfaceHolder_OnShowCB(OH_ArkUI_SurfaceHolder *holder)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_Event event;
    Uint32 id;

    if (!holder) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        return;
    }

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_SHOWN;
    event.user.windowID = id;
    event.user.data1 = NULL;
    event.user.data2 = NULL;
    SDL_PushEvent(&event);
#endif
}
static void OHOS_SurfaceHolder_OnHideCB(OH_ArkUI_SurfaceHolder *holder)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_Event event;
    Uint32 id;

    if (!holder) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    if (id == 0) {
        return;
    }

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_HIDDEN;
    event.user.windowID = id;
    event.user.data1 = NULL;
    event.user.data2 = NULL;
    SDL_PushEvent(&event);
#endif
}

typedef enum OHOS_WindowNotification
{
    OHOS_WINDOW_NOTIFY_WINDOW_EVENT = 0,
    OHOS_WINDOW_NOTIFY_STATUS_CHANGE,
    OHOS_WINDOW_NOTIFY_STATUS_DID_CHANGE,
    OHOS_WINDOW_NOTIFY_SIZE_CHANGE,
    OHOS_WINDOW_NOTIFY_RECT_CHANGE,
    OHOS_WINDOW_NOTIFY_VISIBILITY_CHANGE,
    OHOS_WINDOW_NOTIFY_AVOID_AREA_CHANGE,
    OHOS_WINDOW_NOTIFY_SYSTEM_AVOID_AREA_CHANGE,
    OHOS_WINDOW_NOTIFY_KEYBOARD_HEIGHT_CHANGE,
    OHOS_WINDOW_NOTIFY_KEYBOARD_WILL_SHOW,
    OHOS_WINDOW_NOTIFY_KEYBOARD_DID_SHOW,
    OHOS_WINDOW_NOTIFY_KEYBOARD_WILL_HIDE,
    OHOS_WINDOW_NOTIFY_KEYBOARD_DID_HIDE,
    OHOS_WINDOW_NOTIFY_TOUCH_OUTSIDE,
    OHOS_WINDOW_NOTIFY_DISPLAY_ID_CHANGE,
    OHOS_WINDOW_NOTIFY_SYSTEM_DENSITY_CHANGE,
    OHOS_WINDOW_NOTIFY_NO_INTERACTION_DETECTED,
    OHOS_WINDOW_NOTIFY_SCREENSHOT,
    OHOS_WINDOW_NOTIFY_SCREENSHOT_APP_EVENT,
    OHOS_WINDOW_NOTIFY_DIALOG_TARGET_TOUCH,
    OHOS_WINDOW_NOTIFY_HIGHLIGHT_CHANGE,
    OHOS_WINDOW_NOTIFY_TITLE_BUTTON_RECT_CHANGE,
    OHOS_WINDOW_NOTIFY_FREE_WINDOW_MODE_CHANGE,
    OHOS_WINDOW_NOTIFY_OCCLUSION_STATE_CHANGE,
    OHOS_WINDOW_NOTIFY_FRAME_METRICS_MEASURED,
    OHOS_WINDOW_NOTIFY_ROTATION_CHANGE,
    OHOS_WINDOW_NOTIFY_WILL_CLOSE,
    OHOS_WINDOW_NOTIFY_SUB_WINDOW_CLOSE
} OHOS_WindowNotification;

static ArkUI_NativeNodeAPI_1 *g_node_api = NULL;

static SDL_bool OHOS_Tool_EnsureNodeApi(void)
{
    if (g_node_api != NULL) {
        return SDL_TRUE;
    }

    OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE, ArkUI_NativeNodeAPI_1, g_node_api);
    return g_node_api != NULL;
}

napi_value OHOS_Napi_AttachWindowSurface(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    napi_value retval = NULL;
    ArkUI_NodeHandle node = NULL;
    OH_ArkUI_SurfaceHolder *holder = NULL;
    OH_ArkUI_SurfaceCallback *callback = NULL;
    Uint32 id = 0;
    SDL_WindowData *data;

    if (napi_get_cb_info(env, info, &argc, args, NULL, NULL) != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "attachNode: invalid arguments");
        return NULL;
    }
    if (!OHOS_Tool_EnsureNodeApi()) {
        napi_throw_error(env, NULL, "attachNode: init ArkUI node API failed");
        return NULL;
    }
    if (OH_ArkUI_GetNodeHandleFromNapiValue(env, args[0], &node) != ARKUI_ERROR_CODE_NO_ERROR || !node) {
        napi_throw_error(env, NULL, "attachNode: invalid frame node");
        return NULL;
    }
    if (napi_get_value_uint32(env, args[1], &id) != napi_ok || id == 0) {
        napi_throw_error(env, NULL, "attachNode: invalid window id");
        return NULL;
    }
    data = OHOS_WindowRegistry_Acquire(id);
    if (!data) {
        napi_throw_error(env, NULL, "attachNode: window data not found");
        return NULL;
    }
    holder = OH_ArkUI_SurfaceHolder_Create(node);
    if (!holder) {
        OHOS_WindowRegistry_Release(data);
        napi_throw_error(env, NULL, "attachNode: create surface holder failed");
        return NULL;
    }
    callback = OH_ArkUI_SurfaceCallback_Create();
    if (!callback) {
        OHOS_WindowRegistry_Release(data);
        OH_ArkUI_SurfaceHolder_Dispose(holder);
        napi_throw_error(env, NULL, "attachNode: create surface callback failed");
        return NULL;
    }
    if (OH_ArkUI_SurfaceHolder_SetUserData(holder, (void *)(intptr_t)id) != ARKUI_ERROR_CODE_NO_ERROR) {
        OHOS_WindowRegistry_Release(data);
        OH_ArkUI_SurfaceCallback_Dispose(callback);
        OH_ArkUI_SurfaceHolder_Dispose(holder);
        napi_throw_error(env, NULL, "attachNode: set user data failed");
        return NULL;
    }

    OH_ArkUI_SurfaceCallback_SetSurfaceCreatedEvent(callback, OHOS_SurfaceHolder_OnCreatedCB);
    OH_ArkUI_SurfaceCallback_SetSurfaceChangedEvent(callback, OHOS_SurfaceHolder_OnChangedCB);
    OH_ArkUI_SurfaceCallback_SetSurfaceDestroyedEvent(callback, OHOS_SurfaceHolder_OnDestroyedCB);
    OH_ArkUI_SurfaceCallback_SetSurfaceShowEvent(callback, OHOS_SurfaceHolder_OnShowCB);
    OH_ArkUI_SurfaceCallback_SetSurfaceHideEvent(callback, OHOS_SurfaceHolder_OnHideCB);

    if (OH_ArkUI_SurfaceHolder_AddSurfaceCallback(holder, callback) != ARKUI_ERROR_CODE_NO_ERROR) {
        OHOS_WindowRegistry_Release(data);
        OH_ArkUI_SurfaceCallback_Dispose(callback);
        OH_ArkUI_SurfaceHolder_Dispose(holder);
        napi_throw_error(env, NULL, "attachNode: add surface callback failed");
        return NULL;
    }

    if (OHOS_SurfaceHolder_Add(id, holder, callback) < 0) {
        OHOS_WindowRegistry_Release(data);
        OH_ArkUI_SurfaceHolder_RemoveSurfaceCallback(holder, callback);
        OH_ArkUI_SurfaceCallback_Dispose(callback);
        OH_ArkUI_SurfaceHolder_Dispose(holder);
        napi_throw_error(env, NULL, "attachNode: add surface holder failed");
        return NULL;
    }

    OHOS_WindowRegistry_Release(data);

    napi_get_undefined(env, &retval);
    return retval;
}
napi_value OHOS_Napi_DetachWindowSurface(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { NULL };
    napi_value retval = NULL;
    OH_ArkUI_SurfaceHolder *holder = NULL;
    OH_ArkUI_SurfaceCallback *callback = NULL;
    Uint32 id = 0;

    if (napi_get_cb_info(env, info, &argc, args, NULL, NULL) != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "detachNode: invalid arguments");
        return NULL;
    }
    if (napi_get_value_uint32(env, args[0], &id) != napi_ok || id == 0) {
        napi_throw_error(env, NULL, "detachNode: invalid window id");
        return NULL;
    }

    if (OHOS_SurfaceHolder_Remove(id, &holder, &callback) < 0) {
        napi_get_undefined(env, &retval);
        return retval;
    }

    if (holder && callback) {
        OH_ArkUI_SurfaceHolder_RemoveSurfaceCallback(holder, callback);
    }
    if (callback) {
        OH_ArkUI_SurfaceCallback_Dispose(callback);
    }
    if (holder) {
        OH_ArkUI_SurfaceHolder_Dispose(holder);
    }

    napi_get_undefined(env, &retval);
    return retval;
}

napi_value OHOS_Napi_NotifyWindowReady(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    napi_value retval = NULL;
    Uint32 id = 0;
    Uint32 window_id = 0;
    SDL_WindowData *data;

    if (napi_get_cb_info(env, info, &argc, args, NULL, NULL) != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "notifyCreateDone: invalid arguments");
        return NULL;
    }
    if (napi_get_value_uint32(env, args[0], &id) != napi_ok || id == 0) {
        napi_throw_error(env, NULL, "notifyCreateDone: invalid id");
        return NULL;
    }
    if (napi_get_value_uint32(env, args[1], &window_id) != napi_ok || window_id == 0) {
        napi_throw_error(env, NULL, "notifyCreateDone: invalid window id");
        return NULL;
    }

    data = OHOS_WindowRegistry_Acquire(id);
    if (!data) {
        napi_throw_error(env, NULL, "notifyCreateDone: window data not found");
        return NULL;
    }

    SDL_AtomicSet(&data->ohos_window_id, (int)window_id);
    SDL_AtomicSet(&data->create_done, SDL_TRUE);
    SDL_SemPost(data->create_sem);

    OHOS_WindowRegistry_Release(data);

    napi_get_undefined(env, &retval);
    return retval;
}

napi_value OHOS_Napi_NotifyWindowDestroyed(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { NULL };
    napi_value retval = NULL;
    Uint32 id = 0;
    SDL_WindowData *data;
    SDL_Event event;

    if (napi_get_cb_info(env, info, &argc, args, NULL, NULL) != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "notifyWindowDestroyed: invalid arguments");
        return NULL;
    }
    if (napi_get_value_uint32(env, args[0], &id) != napi_ok || id == 0) {
        napi_throw_error(env, NULL, "notifyWindowDestroyed: invalid id");
        return NULL;
    }

    data = OHOS_WindowRegistry_Acquire(id);
    if (!data) {
        napi_get_undefined(env, &retval);
        return retval;
    }

    if (!SDL_AtomicGet(&data->sdl_destroying)) {
        SDL_AtomicSet(&data->platform_destroyed, SDL_TRUE);

        SDL_zero(event);
        event.type = SDL_WINDOWEVENT;
        event.window.windowID = id;
        event.window.event = SDL_WINDOWEVENT_CLOSE;
        SDL_PushEvent(&event);
    }

    OHOS_WindowRegistry_Release(data);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_ReturnUndefined(napi_env env)
{
    napi_value retval = NULL;

    napi_get_undefined(env, &retval);
    return retval;
}

static SDL_bool OHOS_Napi_GetWindowArgs(napi_env env, napi_callback_info info, size_t *argc,
                                        napi_value *args, Uint32 *id, const char *name)
{
    if (napi_get_cb_info(env, info, argc, args, NULL, NULL) != napi_ok || *argc < 1) {
        napi_throw_error(env, NULL, name);
        return SDL_FALSE;
    }
    if (napi_get_value_uint32(env, args[0], id) != napi_ok || *id == 0) {
        napi_throw_error(env, NULL, name);
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static void OHOS_PushWindowNotification(Uint32 id, OHOS_WindowNotification notification,
                                        intptr_t data1, intptr_t data2)
{
    SDL_Event event;

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    event.user.code = notification;
    event.user.windowID = id;
    event.user.data1 = (void *)data1;
    event.user.data2 = (void *)data2;
    SDL_PushEvent(&event);
}

napi_value OHOS_Napi_NotifyWindowEvent(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    Uint32 id = 0;
    int32_t window_event = 0;
    SDL_WindowData *data;

    if (!OHOS_Napi_GetWindowArgs(env, info, &argc, args, &id, "notifyWindowEvent")) {
        return NULL;
    }

    data = OHOS_WindowRegistry_Acquire(id);
    if (!data) {
        return OHOS_Napi_ReturnUndefined(env);
    }

    if (argc < 2 || napi_get_value_int32(env, args[1], &window_event) != napi_ok) {
        OHOS_WindowRegistry_Release(data);
        napi_throw_error(env, NULL, "notifyWindowEvent: invalid event");
        return NULL;
    }

    OHOS_PushWindowNotification(id, OHOS_WINDOW_NOTIFY_WINDOW_EVENT, window_event, 0);

    OHOS_WindowRegistry_Release(data);
    return OHOS_Napi_ReturnUndefined(env);
}

static napi_value OHOS_Napi_NotifyWindowStatus(napi_env env, napi_callback_info info,
                                               OHOS_WindowNotification notification,
                                               const char *name)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    Uint32 id = 0;
    int32_t status = 0;
    SDL_WindowData *data;

    if (!OHOS_Napi_GetWindowArgs(env, info, &argc, args, &id, name)) {
        return NULL;
    }

    data = OHOS_WindowRegistry_Acquire(id);
    if (!data) {
        return OHOS_Napi_ReturnUndefined(env);
    }

    if (argc < 2 || napi_get_value_int32(env, args[1], &status) != napi_ok) {
        OHOS_WindowRegistry_Release(data);
        napi_throw_error(env, NULL, name);
        return NULL;
    }

    OHOS_PushWindowNotification(id, notification, status, 0);

    OHOS_WindowRegistry_Release(data);
    return OHOS_Napi_ReturnUndefined(env);
}

napi_value OHOS_Napi_NotifyWindowStatusChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowStatus(env, info, OHOS_WINDOW_NOTIFY_STATUS_CHANGE,
                                        "notifyWindowStatusChange");
}

napi_value OHOS_Napi_NotifyWindowStatusDidChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowStatus(env, info, OHOS_WINDOW_NOTIFY_STATUS_DID_CHANGE,
                                        "notifyWindowStatusDidChange");
}

napi_value OHOS_Napi_NotifyWindowSizeChange(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    napi_value width_value = NULL;
    napi_value height_value = NULL;
    Uint32 id = 0;
    int32_t width = 0;
    int32_t height = 0;
    SDL_WindowData *data;

    if (!OHOS_Napi_GetWindowArgs(env, info, &argc, args, &id, "notifyWindowSizeChange")) {
        return NULL;
    }

    data = OHOS_WindowRegistry_Acquire(id);
    if (!data) {
        return OHOS_Napi_ReturnUndefined(env);
    }

    if (argc < 2 ||
        napi_get_named_property(env, args[1], "width", &width_value) != napi_ok ||
        napi_get_named_property(env, args[1], "height", &height_value) != napi_ok ||
        napi_get_value_int32(env, width_value, &width) != napi_ok ||
        napi_get_value_int32(env, height_value, &height) != napi_ok) {
        OHOS_WindowRegistry_Release(data);
        napi_throw_error(env, NULL, "notifyWindowSizeChange: invalid size");
        return NULL;
    }

    OHOS_PushWindowNotification(id, OHOS_WINDOW_NOTIFY_SIZE_CHANGE, width, height);

    OHOS_WindowRegistry_Release(data);
    return OHOS_Napi_ReturnUndefined(env);
}

static napi_value OHOS_Napi_NotifyWindowNoop(napi_env env, napi_callback_info info, const char *name)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    Uint32 id = 0;
    SDL_WindowData *data;

    if (!OHOS_Napi_GetWindowArgs(env, info, &argc, args, &id, name)) {
        return NULL;
    }

    data = OHOS_WindowRegistry_Acquire(id);
    if (data) {
        OHOS_WindowRegistry_Release(data);
    }

    return OHOS_Napi_ReturnUndefined(env);
}

napi_value OHOS_Napi_NotifyWindowRectChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowRectChange");
}

napi_value OHOS_Napi_NotifyWindowVisibilityChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowVisibilityChange");
}

napi_value OHOS_Napi_NotifyWindowAvoidAreaChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowAvoidAreaChange");
}

napi_value OHOS_Napi_NotifyWindowSystemAvoidAreaChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowSystemAvoidAreaChange");
}

napi_value OHOS_Napi_NotifyWindowKeyboardHeightChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowKeyboardHeightChange");
}

napi_value OHOS_Napi_NotifyWindowKeyboardWillShow(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowKeyboardWillShow");
}

napi_value OHOS_Napi_NotifyWindowKeyboardDidShow(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowKeyboardDidShow");
}

napi_value OHOS_Napi_NotifyWindowKeyboardWillHide(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowKeyboardWillHide");
}

napi_value OHOS_Napi_NotifyWindowKeyboardDidHide(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowKeyboardDidHide");
}

napi_value OHOS_Napi_NotifyWindowTouchOutside(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowTouchOutside");
}

napi_value OHOS_Napi_NotifyWindowDisplayIdChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowDisplayIdChange");
}

napi_value OHOS_Napi_NotifyWindowSystemDensityChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowSystemDensityChange");
}

napi_value OHOS_Napi_NotifyWindowNoInteractionDetected(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowNoInteractionDetected");
}

napi_value OHOS_Napi_NotifyWindowScreenshot(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowScreenshot");
}

napi_value OHOS_Napi_NotifyWindowScreenshotAppEvent(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowScreenshotAppEvent");
}

napi_value OHOS_Napi_NotifyWindowDialogTargetTouch(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowDialogTargetTouch");
}

napi_value OHOS_Napi_NotifyWindowHighlightChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowHighlightChange");
}

napi_value OHOS_Napi_NotifyWindowTitleButtonRectChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowTitleButtonRectChange");
}

napi_value OHOS_Napi_NotifyWindowFreeWindowModeChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowFreeWindowModeChange");
}

napi_value OHOS_Napi_NotifyWindowOcclusionStateChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowOcclusionStateChange");
}

napi_value OHOS_Napi_NotifyWindowFrameMetricsMeasured(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowFrameMetricsMeasured");
}

napi_value OHOS_Napi_NotifyWindowRotationChange(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowRotationChange");
}

napi_value OHOS_Napi_NotifyWindowWillClose(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowWillClose");
}

napi_value OHOS_Napi_NotifyWindowSubWindowClose(napi_env env, napi_callback_info info)
{
    return OHOS_Napi_NotifyWindowNoop(env, info, "notifyWindowSubWindowClose");
}

/* Window hooks, in SDL_VideoDevice declaration order. */

static int OHOS_Window_PostRequest(OHOS_WindowRequest *request)
{
    if (!request) {
        return SDL_InvalidParamError("request");
    }

    if (!g_window_request_tsf) {
        OHOS_Tool_DestroyWindowRequest(request);
        return SDL_SetError("window request channel not registered");
    }

    if (napi_call_threadsafe_function(g_window_request_tsf, request, napi_tsfn_nonblocking) != napi_ok) {
        OHOS_Tool_DestroyWindowRequest(request);
        return SDL_SetError("post window request failed");
    }

    return 0;
}

static void OHOS_Window_SendCommand(SDL_Window *window, OHOS_WindowCommand cmd)
{
    OHOS_WindowRequest *request;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(*request));
    if (!request) {
        SDL_OutOfMemory();
        return;
    }

    if (OHOS_Tool_ParseWindowRequest(window, cmd, request)) {
        OHOS_Tool_DestroyWindowRequest(request);
        return;
    }

    OHOS_Window_PostRequest(request);
}

int OHOS_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = NULL;
    OHOS_WindowRequest *request = NULL;
    SDL_bool window_registered = SDL_FALSE;
    SDL_bool gl_registered = SDL_FALSE;
    int retval = 0;
    Uint32 start;

    SDL_assert(_this != NULL);
    SDL_assert(window != NULL);

    if (!g_ohos_window_registry_lock) {
        g_ohos_window_registry_lock = SDL_CreateMutex();
        if (!g_ohos_window_registry_lock) {
            return SDL_OutOfMemory();
        }
    }

    if (!g_ohos_surface_holder_lock) {
        g_ohos_surface_holder_lock = SDL_CreateMutex();
        if (!g_ohos_surface_holder_lock) {
            return SDL_OutOfMemory();
        }
    }

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!data) {
        return SDL_OutOfMemory();
    }
    window->driverdata = data;

    retval = OHOS_WindowRegistry_Register(data, window->id);
    if (retval) {
        window->driverdata = NULL;
        OHOS_WindowData_Free(data);
        return retval;
    }
    window_registered = SDL_TRUE;

    data->create_sem = SDL_CreateSemaphore(0);
    if (!data->create_sem) {
        retval = SDL_OutOfMemory();
        goto errfunc;
    }

    SDL_AtomicSet(&data->create_done, SDL_FALSE);
    SDL_AtomicSet(&data->ohos_window_id, 0);
    SDL_AtomicSet(&data->sdl_destroying, SDL_FALSE);
    SDL_AtomicSet(&data->platform_destroyed, SDL_FALSE);

    if (window->flags & SDL_WINDOW_OPENGL) {
        retval = OHOS_GLRegistry_Register(window->id);
        if (retval) {
            goto errfunc;
        }
        gl_registered = SDL_TRUE;
    }

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(*request));
    if (!request) {
        retval = SDL_OutOfMemory();
        goto errfunc;
    }

    retval = OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_CREATE, request);
    if (retval) {
        OHOS_Tool_DestroyWindowRequest(request);
        request = NULL;
        goto errfunc;
    }

    retval = OHOS_Window_PostRequest(request);
    request = NULL;
    if (retval) {
        goto errfunc;
    }

    start = SDL_GetTicks();
    while (!SDL_AtomicGet(&data->create_done)) {
        Uint32 elapsed = SDL_GetTicks() - start;

        if (elapsed >= 5000) {
            break;
        }
        if (SDL_SemWaitTimeout(data->create_sem, 5000 - elapsed) < 0) {
            break;
        }
    }

    if (!SDL_AtomicGet(&data->create_done)) {
        return SDL_SetError("OHOS window creation timed out");
    }

    {
        Uint32 sdl_window_id = window->id;
        Uint32 ohos_window_id = (Uint32)SDL_AtomicGet(&data->ohos_window_id);
        WindowManager_WindowProperties properties;
        bool is_shown = false;
        int32_t properties_ret;
        int32_t shown_ret;

        SDL_zero(properties);
        properties_ret = OH_WindowManager_GetWindowProperties(
            (int32_t)ohos_window_id, &properties);
        shown_ret = OH_WindowManager_IsWindowShown(
            (int32_t)ohos_window_id, &is_shown);

        if (properties_ret == 0) {
            SDL_Log("OHOS_CreateWindow: sdl_id=%u ohos_id=%u "
                    "windowRect=(%d,%d,%u,%u) drawableRect=(%d,%d,%u,%u) "
                    "displayId=%u fullscreen=%d shown_ret=%d shown=%d",
                    sdl_window_id, ohos_window_id,
                    properties.windowRect.posX, properties.windowRect.posY,
                    properties.windowRect.width, properties.windowRect.height,
                    properties.drawableRect.posX, properties.drawableRect.posY,
                    properties.drawableRect.width, properties.drawableRect.height,
                    properties.displayId, properties.isFullScreen > 0,
                    shown_ret, is_shown > 0);
        } else {
            SDL_Log("OHOS_CreateWindow: GetWindowProperties failed: sdl_id=%u ohos_id=%u ret=%d",
                    sdl_window_id, ohos_window_id, properties_ret);
        }

        OH_WindowManager_SetWindowStatusBarColor((Sint32)ohos_window_id, 0x00);
    }

    return 0;

errfunc:
    if (request) {
        OHOS_Tool_DestroyWindowRequest(request);
    }
    if (gl_registered) {
        OHOS_GLRegistry_Unregister(window->id);
    }
    if (window_registered) {
        OHOS_WindowRegistry_Unregister(window->id);
    }
    window->driverdata = NULL;
    return retval;
}

int OHOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    return 0;
}

void OHOS_SetWindowTitle(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_TITLE);
}

void OHOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon)
{
}

void OHOS_SetWindowPosition(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_POSITION);
}

void OHOS_SetWindowSize(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_SIZE);
}

void OHOS_SetWindowMinimumSize(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_MINIMUM_SIZE);
}

void OHOS_SetWindowMaximumSize(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE);
}

int OHOS_GetWindowBordersSize(_THIS, SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
    return 0;
}

void OHOS_GetWindowSizeInPixels(_THIS, SDL_Window *window, int *w, int *h)
{
}

int OHOS_SetWindowOpacity(_THIS, SDL_Window *window, float opacity)
{
    OHOS_WindowRequest *request;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(*request));
    if (!request) {
        return SDL_OutOfMemory();
    }

    if (OHOS_Tool_ParseWindowRequest(window, OHOS_WINDOW_CMD_SET_OPACITY, request)) {
        OHOS_Tool_DestroyWindowRequest(request);
        return -1;
    }

    request->opacity = opacity;
    OHOS_Window_PostRequest(request);
    return 0;
}

int OHOS_SetWindowModalFor(_THIS, SDL_Window *modal_window, SDL_Window *parent_window)
{
    return 0;
}

int OHOS_SetWindowInputFocus(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_INPUT_FOCUS);
    return 0;
}

void OHOS_ShowWindow(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SHOW);
}

void OHOS_HideWindow(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_HIDE);
}

void OHOS_RaiseWindow(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_RAISE);
}

void OHOS_MaximizeWindow(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_MAXIMIZE);
}

void OHOS_MinimizeWindow(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_MINIMIZE);
}

void OHOS_RestoreWindow(_THIS, SDL_Window *window)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_RESTORE);
}

void OHOS_SetWindowBordered(_THIS, SDL_Window *window, SDL_bool bordered)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_BORDERED);
}

void OHOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_RESIZABLE);
}

void OHOS_SetWindowAlwaysOnTop(_THIS, SDL_Window *window, SDL_bool on_top)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_ALWAYS_ON_TOP);
}

void OHOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    OHOS_Window_SendCommand(window, OHOS_WINDOW_CMD_SET_FULLSCREEN);
}

int OHOS_SetWindowGammaRamp(_THIS, SDL_Window *window, const Uint16 *ramp)
{
    return 0;
}

int OHOS_GetWindowGammaRamp(_THIS, SDL_Window *window, Uint16 *ramp)
{
    return 0;
}

void *OHOS_GetWindowICCProfile(_THIS, SDL_Window *window, size_t *size)
{
    return NULL;
}

int OHOS_GetWindowDisplayIndex(_THIS, SDL_Window *window)
{
    return 0;
}

void OHOS_SetWindowMouseRect(_THIS, SDL_Window *window)
{
}

void OHOS_SetWindowMouseGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
}

void OHOS_SetWindowKeyboardGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
}

void OHOS_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;

    if (!window || !(data = (SDL_WindowData *)window->driverdata)) {
        return;
    }

    SDL_AtomicSet(&data->sdl_destroying, SDL_TRUE);

    OHOS_GLRegistry_Unregister(window->id);

    if (SDL_AtomicGet(&data->create_done) &&
        !SDL_AtomicGet(&data->platform_destroyed)) {
        // TODO: request ArkTS destroyWindow(window->id)
    }

    OHOS_WindowRegistry_Unregister(window->id);

    window->driverdata = NULL;
}

int OHOS_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    return 0;
}

int OHOS_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    return 0;
}

void OHOS_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
}

void OHOS_OnWindowEnter(_THIS, SDL_Window *window)
{
}

int OHOS_FlashWindow(_THIS, SDL_Window *window, SDL_FlashOperation operation)
{
    return 0;
}

/* Backend-internal hook used by OHOS_PumpEvents. */
void OHOS_Window_HandleEvents(_THIS, const SDL_Event *event)
{
    SDL_WindowData *data;
    SDL_Window *window;

    (void)_this;

    if (!event) {
        return;
    }

    data = OHOS_WindowRegistry_Acquire(event->user.windowID);
    if (!data) {
        return;
    }

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window) {
        switch ((OHOS_WindowNotification)event->user.code) {
        case OHOS_WINDOW_NOTIFY_WINDOW_EVENT:
            switch ((OHOS_ArkUIWindowEventType)(intptr_t)event->user.data1) {
            case OHOS_ARKUI_WINDOW_EVENT_SHOWN:
                SDL_SendWindowEvent(window, SDL_WINDOWEVENT_SHOWN, 0, 0);
                break;
            case OHOS_ARKUI_WINDOW_EVENT_ACTIVE:
                SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
                break;
            case OHOS_ARKUI_WINDOW_EVENT_INACTIVE:
                SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
                break;
            case OHOS_ARKUI_WINDOW_EVENT_HIDDEN:
                SDL_SendWindowEvent(window, SDL_WINDOWEVENT_HIDDEN, 0, 0);
                break;
            case OHOS_ARKUI_WINDOW_EVENT_DESTROYED:
                SDL_SendWindowEvent(window, SDL_WINDOWEVENT_CLOSE, 0, 0);
                break;
            default:
                break;
            }
            break;
        case OHOS_WINDOW_NOTIFY_SIZE_CHANGE:
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_SIZE_CHANGED,
                               (int)(intptr_t)event->user.data1,
                               (int)(intptr_t)event->user.data2);
            break;
        default:
            break;
        }
    }

    OHOS_WindowRegistry_Release(data);
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
