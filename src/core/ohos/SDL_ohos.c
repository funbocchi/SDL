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

#include "SDL_ohos.h"

#include <napi/native_api.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <arkui/native_node.h>
#include <arkui/native_interface.h>
#include <arkui/native_node_napi.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <window_manager/oh_window_comm.h>
#include <window_manager/oh_window.h>

#include "SDL_system.h"
#include "SDL_mutex.h"
#include "SDL_events.h"

#include "../../video/ohos/SDL_ohosevents.h"

#define OHOS_ARGS_REQUIRED   1
#define OHOS_ARGS_MAX        4
#define OHOS_ARG_ABILITY     0
#define OHOS_ARG_APP_LIBRARY 1
#define OHOS_ARG_PAGE_NAME   2

#define OHOS_WINDOW_CREATE_TIMEOUT   5000
#define OHOS_WINDOW_SUBWINDOW_TITLE "default_title"
#define OHOS_WINDOW_SUBWINDOW_NAME  "sdl2_default"

typedef enum
{
    OHOS_WINDOW_CMD_CREATE,
    OHOS_WINDOW_CMD_DESTROY,
    OHOS_WINDOW_CMD_SET_SIZE,
    OHOS_WINDOW_CMD_SET_POS,
    OHOS_WINDOW_CMD_SET_TITLE,
    OHOS_WINDOW_CMD_SET_FULLSCREEN,
    OHOS_WINDOW_CMD_SHOW,
    OHOS_WINDOW_CMD_SET_BORDERED
} OHOS_WindowCommand;

typedef struct OHOS_WindowRequest
{
    OHOS_WindowCommand          cmd;
    Uint32                      sdl_window_id;
    SDL_sem                    *sem;
    napi_ref                   *ohos_window_ref_slot;
    napi_ref                    ohos_window_ref_value;
    ArkUI_NodeHandle           *xc_slot;
    ArkUI_NodeHandle            xc_value;
    ArkUI_NodeHandle           *parent_slot;
    ArkUI_NodeHandle            parent_value;
    OH_ArkUI_SurfaceHolder    **surface_holder_slot;
    OH_ArkUI_SurfaceHolder     *surface_holder_value;
    OH_ArkUI_SurfaceCallback  **surface_callback_slot;
    OH_ArkUI_SurfaceCallback   *surface_callback_value;
    int32_t                    *ohos_window_id_slot;
    int32_t                     ohos_window_id_value;

    SDL_bool                    bordered;
    int                         width, height;
    int                         pos_x, pos_y;
    SDL_bool                    shown;
    char                       *title;

    struct OHOS_WindowRequest  *next;
} OHOS_WindowRequest;

/* Apply for relevant interfaces */

/* NodeContent tools */

/* WindowEventQ tools */

typedef struct OHOS_WindowEventQ
{
    Uint32                      sdl_window_id;
    napi_ref                    ohos_window_ref;
    SDL_sem                    *sem;
    SDL_mutex                  *lock;
    SDL_bool                    busy;
    OHOS_WindowRequest         *pending_head;
    OHOS_WindowRequest         *pending_tail;
    SDL_bool                    destroying;
    ArkUI_NodeHandle            xc;
    ArkUI_NodeHandle            parent;
    OH_ArkUI_SurfaceHolder     *surface_holder;
    OH_ArkUI_SurfaceCallback   *surface_callback;
    int32_t                     ohos_window_id;

    struct OHOS_WindowEventQ   *next;
} OHOS_WindowEventQ;
static OHOS_WindowEventQ       *g_ohos_window_event_queues = NULL;

static OHOS_WindowEventQ *OHOS_WindowEventQueue_Create(Uint32 sdl_window_id)
{
    OHOS_WindowEventQ *queue = (OHOS_WindowEventQ *)SDL_calloc(1, sizeof(OHOS_WindowEventQ));
    if (queue == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS window %u: failed to allocate event queue", sdl_window_id);
        return NULL;
    }
    queue->sdl_window_id = sdl_window_id;
    queue->sem = SDL_CreateSemaphore(1);
    queue->lock = SDL_CreateMutex();
    if (queue->lock == NULL) {
        SDL_free(queue);
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS window %u: failed to allocate event queue", sdl_window_id);
        return NULL;
    }
    queue->next = g_ohos_window_event_queues;
    g_ohos_window_event_queues = queue;
    return queue;
}

static OHOS_WindowEventQ *OHOS_WindowEventQueue_Find(Uint32 sdl_window_id)
{
    OHOS_WindowEventQ *queue = g_ohos_window_event_queues;
    
    while (queue && queue->sdl_window_id != sdl_window_id) {
        queue = queue->next;
    }
    return queue;
}

static void OHOS_WindowEventQueue_Enqueue(OHOS_WindowEventQ *queue, OHOS_WindowRequest *request)
{
    SDL_assert(queue != NULL);
    SDL_assert(request != NULL);
    SDL_LockMutex(queue->lock);
    if (queue->pending_tail == NULL) {
        queue->pending_head = request;
        queue->pending_tail = request;
    } else {
        queue->pending_tail->next = request;
        queue->pending_tail = request;
    }
    SDL_UnlockMutex(queue->lock);
    /* TODO: Consider merging some similar operations in the future. */
}

static OHOS_WindowRequest *OHOS_WindowEventQueue_Dequeue(OHOS_WindowEventQ *queue)
{
    OHOS_WindowRequest *request = NULL;

    SDL_assert(queue != NULL);
    SDL_LockMutex(queue->lock);
    if (queue->pending_head != NULL) {
        request = queue->pending_head;
        queue->pending_head = request->next;
        if (queue->pending_tail == request) {
            queue->pending_tail = NULL;
        }
    }
    SDL_UnlockMutex(queue->lock);
    request->next = NULL;
    return request;
}

static void OHOS_WindowEventQueue_Clear(OHOS_WindowEventQ *queue)
{
    OHOS_WindowRequest *request = NULL;
    SDL_assert(queue != NULL);
    SDL_LockMutex(queue->lock);
    request = queue->pending_head;
    queue->pending_head = NULL;
    queue->pending_tail = NULL;
    SDL_UnlockMutex(queue->lock);
    while (request != NULL) {
        OHOS_WindowRequest *next = request->next;
        SDL_free(request);
        request = next;
    }
}

static void OHOS_WindowEventQueue_Destroy(Uint32 sdl_window_id)
{}

static void OHOS_WindowEventQueue_Unlink(OHOS_WindowEventQ *queue)
{
    OHOS_WindowEventQ *prev = g_ohos_window_event_queues;

    SDL_assert(prev != NULL);
    SDL_assert(queue != NULL);
    if (prev == queue) {
        g_ohos_window_event_queues = queue->next;
    }else {
        OHOS_WindowEventQ *cur = prev->next;
        while (cur != queue) {
            prev = cur;
            cur = cur->next;
        }
        prev->next = queue->next;
    }
    queue->next = NULL;
}

/* Napi tools */

ArkUI_NativeNodeAPI_1          *node_api = NULL;

static ArkUI_NativeNodeAPI_1 *OHOS_Tool_EnsureNodeApi()
{
    if (node_api == NULL) {
        OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE, ArkUI_NativeNodeAPI_1, node_api);
    }
    return node_api;
}

static int32_t OHOS_Tool_EnableCrossLanguage(ArkUI_NodeHandle handle)
{
    int32_t retval = 0;
    ArkUI_CrossLanguageOption *option = OH_ArkUI_CrossLanguageOption_Create();
    if (option == NULL) {
        return ARKUI_ERROR_CODE_PARAM_INVALID;
    }
    OH_ArkUI_CrossLanguageOption_SetAttributeSettingStatus(option, true);
    OH_ArkUI_CrossLanguageOption_SetTreeOperatingStatus(option, true);
    retval = OH_ArkUI_NodeUtils_SetCrossLanguageOption(handle, option);
    OH_ArkUI_CrossLanguageOption_Destroy(option);
    return retval;
}

static napi_value OHOS_Tool_PromiseThen(napi_env env, napi_value promise, napi_callback callback, void *data, const char *name)
{
    napi_value then_fn = NULL;
    napi_value handler = NULL;
    napi_value retval = NULL;

    napi_get_named_property(env, promise, "then", &then_fn);
    napi_create_function(env, name ? name : "then", NAPI_AUTO_LENGTH, callback, data, &handler);
    napi_call_function(env, promise, then_fn, 1, &handler, &retval);
    return retval;
}

static napi_value OHOS_Tool_PromiseFinally(napi_env env, napi_value promise, napi_callback callback, void *data, const char *name)
{
    napi_value finally_fn = NULL;
    napi_value handler = NULL;
    napi_value retval = NULL;

    napi_get_named_property(env, promise, "finally", &finally_fn);
    napi_create_function(env, name ? name : "finally", NAPI_AUTO_LENGTH, callback, data, &handler);
    napi_call_function(env, promise, finally_fn, 1, &handler, &retval);
    return retval;
}

/* SurfaceHolder interfaces */

static void OHOS_SurfaceHolder_OnCreated(OH_ArkUI_SurfaceHolder *holder)
{
    OHNativeWindow *native_window = NULL;
    Uint32 id = 0;
    SDL_Event event;

    id = (Uint32)(uintptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    native_window = OH_ArkUI_XComponent_GetNativeWindow(holder);
    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_CREATED;
    event.user.windowID = id;
    event.user.data1 = (void *)native_window;
    SDL_PushEvent(&event);
}

static void OHOS_SurfaceHolder_OnChanged(OH_ArkUI_SurfaceHolder *holder, uint64_t width, uint64_t height)
{
    SDL_Event event;
    Uint32 id;
    id = (Uint32)(uintptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_CHANGED;
    event.user.windowID = id;
    event.user.data1 = (void *)(uintptr_t)width;
    event.user.data2 = (void *)(uintptr_t)height;
    SDL_PushEvent(&event);
}

static void OHOS_SurfaceHolder_OnDestroyed(OH_ArkUI_SurfaceHolder *holder)
{
    SDL_Event event;
    Uint32 id;
    id = (Uint32)(uintptr_t)OH_ArkUI_SurfaceHolder_GetUserData(holder);
    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_GL;
    event.user.code = OHOS_GL_EVENT_SURFACE_DESTROYED;
    event.user.windowID = id;
    SDL_PushEvent(&event);
}

static void OHOS_SurfaceHolder_OnShown(OH_ArkUI_SurfaceHolder *holder)
{}

/* Node tools */

static ArkUI_NodeHandle OHOS_Node_CreateXComponentWithCallbacks(Uint32 sdl_window_id, OHOS_WindowRequest *request)
{
    ArkUI_NativeNodeAPI_1 *api = NULL;
    ArkUI_NodeHandle xc = NULL;
    OH_ArkUI_SurfaceHolder *holder = NULL;
    OH_ArkUI_SurfaceCallback *callback = NULL;
    ArkUI_AttributeItem com_item;
    ArkUI_NumberValue percent;

    api = OHOS_Tool_EnsureNodeApi();
    xc = api->createNode(ARKUI_NODE_XCOMPONENT);
    com_item.string = "native_xcomponent";
    api->setAttribute(xc, NODE_XCOMPONENT_ID, &com_item);
    percent.f32 = 1.0f;
    com_item.value=&percent;
    com_item.size = 1;
    api->setAttribute(xc, NODE_WIDTH_PERCENT, &com_item);
    api->setAttribute(xc, NODE_HEIGHT_PERCENT, &com_item);

    holder = OH_ArkUI_SurfaceHolder_Create(xc);
    callback = OH_ArkUI_SurfaceCallback_Create();
    OH_ArkUI_SurfaceCallback_SetSurfaceCreatedEvent(callback, OHOS_SurfaceHolder_OnCreated);
    OH_ArkUI_SurfaceCallback_SetSurfaceChangedEvent(callback, OHOS_SurfaceHolder_OnChanged);
    OH_ArkUI_SurfaceCallback_SetSurfaceDestroyedEvent(callback, OHOS_SurfaceHolder_OnDestroyed);
    OH_ArkUI_SurfaceHolder_AddSurfaceCallback(holder, callback);

    *request->surface_holder_slot = holder;
    *request->surface_callback_slot = callback;

    return xc;
}

static void OHOS_Node_DestroyXComponent()
{}

/* JS thread-related processing interfaces */

static char                     g_ohos_window_page_name[256] = "pages/SDL2Page";
static napi_ref                 g_ohos_ability_ref =        NULL;
static napi_ref                 g_ohos_window_stage_ref =   NULL;
static Uint32                   g_ohos_main_window_id =     0;

static napi_threadsafe_function g_ohos_window_request_tsf = NULL;

static napi_value OHOS_JS_Window_OnRequestDone(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;
    void *data;
    OHOS_WindowRequest *request = NULL;
    
    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    if (request->sem != NULL) {
        SDL_SemPost(request->sem);
    }
    if (request->title != NULL) {
        SDL_free(request->title);
    }
    SDL_free(request);
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_Window_OnContentLoaded(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL;
    napi_value get_ui_context_fn = NULL;
    napi_value ui_context = NULL;
    napi_value get_page_root_fn = NULL;
    napi_value root_frame_node = NULL;
    ArkUI_NodeHandle root_node_handle = NULL;
    ArkUI_NodeHandle xc_handle = NULL;
    ArkUI_NativeNodeAPI_1 *api = NULL;

    napi_get_cb_info(env, info, NULL, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_get_reference_value(env, *request->ohos_window_ref_slot, &window);
    napi_get_named_property(env, window, "getUIContext", &get_ui_context_fn);
    napi_call_function(env, window, get_ui_context_fn, 0, NULL, &ui_context);
    napi_get_named_property(env, ui_context, "getPageRootNode", &get_page_root_fn);
    napi_call_function(env, ui_context, get_page_root_fn, 0, NULL, &root_frame_node);

    OH_ArkUI_GetNodeHandleFromNapiValue(env, root_frame_node, &root_node_handle);
    OHOS_Tool_EnableCrossLanguage(root_node_handle);
    api = OHOS_Tool_EnsureNodeApi();
    xc_handle = OHOS_Node_CreateXComponentWithCallbacks(request->sdl_window_id, request);
    api->addChild(root_node_handle, xc_handle);
    napi_get_undefined(env, &retval);
    *request->xc_slot = xc_handle;
    *request->parent_slot = root_node_handle;
    return retval;
}

static napi_value OHOS_JS_Window_OnWindowObtained(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value load_content_fn = NULL, fn = NULL;
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value promise = NULL;
    napi_value path = NULL, properteis = NULL, id = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_create_reference(env, argv[0], 1, request->ohos_window_ref_slot);
    request->ohos_window_ref_value = *request->ohos_window_ref_slot;
    napi_get_named_property(env, argv[0], "getWindowProperties", &fn);
    napi_call_function(env, argv[0], fn, 0, NULL, &properteis);
    napi_get_named_property(env, properteis, "id", &id);
    napi_get_value_int32(env, id, request->ohos_window_id_slot);
    request->ohos_window_id_value = *request->ohos_window_id_slot;
    napi_create_string_utf8(env, g_ohos_window_page_name, NAPI_AUTO_LENGTH, &path);
    napi_get_named_property(env, argv[0], "loadContent", &load_content_fn);
    napi_call_function(env, argv[0], load_content_fn, 1, &path, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnCreate(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value stage = NULL;
    napi_value promise = NULL;

    napi_get_cb_info(env, info, NULL, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_get_reference_value(env, g_ohos_window_stage_ref, &stage);
    if (request->sdl_window_id == g_ohos_main_window_id) {
        napi_value get_main_window_fn = NULL;
        napi_get_named_property(env, stage, "getMainWindow", &get_main_window_fn);
        napi_call_function(env, stage, get_main_window_fn, 0, NULL, &promise);
    } else {
        napi_value create_sub_window_with_options_fn = NULL;
        napi_value options = NULL;
        char name_buf[64];
        napi_value name = NULL;
        napi_value title = NULL;
        napi_value decor_enable = NULL;
        size_t argc = 2;
        napi_value argv[2] = { NULL };

        SDL_snprintf(name_buf, sizeof(name_buf), "%s_%u", OHOS_WINDOW_SUBWINDOW_NAME, (unsigned int)request->sdl_window_id);
        napi_create_string_utf8(env, name_buf, NAPI_AUTO_LENGTH, &name);
        napi_create_object(env, &options);
        napi_create_string_utf8(env, OHOS_WINDOW_SUBWINDOW_TITLE, NAPI_AUTO_LENGTH, &title);
        napi_get_boolean(env, true, &decor_enable);
        napi_set_named_property(env, options, "enableDecor", decor_enable);
        napi_set_named_property(env, options, "title", title);
        napi_get_named_property(env, stage, "createSubWindowWithOptions", &create_sub_window_with_options_fn);
        argv[0] = name;
        argv[1] = options;
        napi_call_function(env, stage, create_sub_window_with_options_fn, argc, argv, &promise);
    }
    return promise;
}

static napi_value OHOS_JS_Window_OnDestroy(napi_env env, napi_callback_info info)
{
    void *data;
    OHOS_WindowRequest *request = NULL;
    ArkUI_NativeNodeAPI_1 *api = NULL;
    napi_value destroy_fn = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    OH_ArkUI_SurfaceHolder_RemoveSurfaceCallback(request->surface_holder_value, request->surface_callback_value);
    OH_ArkUI_SurfaceCallback_Dispose(request->surface_callback_value);
    OH_ArkUI_SurfaceHolder_Dispose(request->surface_holder_value);
    api = OHOS_Tool_EnsureNodeApi();
    api->removeChild(request->parent_value, request->xc_value);
    api->disposeNode(request->xc_value);

    if (request->sdl_window_id == g_ohos_main_window_id) {
        napi_value stage = NULL;
        napi_get_reference_value(env, g_ohos_window_stage_ref, &stage);
        napi_get_named_property(env, stage, "releaseUIContent", &destroy_fn);
        napi_call_function(env, stage, destroy_fn, 0, NULL, &promise);
    } else {
        napi_value window = NULL;
        napi_get_reference_value(env, request->ohos_window_ref_value, &window);
        napi_delete_reference(env, request->ohos_window_ref_value);
        napi_get_named_property(env, window, "destroyWindow", &destroy_fn);
        napi_call_function(env, window, destroy_fn, 0, NULL, &promise);
    }
    return promise;
}

static napi_value OHOS_JS_Window_OnSetBordered(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value fn = NULL, promise = NULL, window = NULL, bordered = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_get_boolean(env, (request->bordered == SDL_TRUE) ? true : false, &bordered);
    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_named_property(env, window, "setWindowDecorVisible", &fn);
    napi_call_function(env, window, fn, 1, &bordered, &promise);
    return promise;
}

static napi_value OHOS_JS_Window_OnSetPos(napi_env env, napi_callback_info info)
{
    int32_t result;
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    WindowManager_WindowProperties window_properties;
    napi_value retval = NULL;
    int32_t pos_y_offset = 0;
    int32_t pos_x = 0, pos_y = 0;
    napi_value window = NULL, fn = NULL, promise = NULL;
    napi_value argv[2] = { NULL };

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    result = OH_WindowManager_GetWindowProperties(request->ohos_window_id_value, &window_properties);
    if (result != 0) {
        napi_get_undefined(env, &retval);
        return retval;
    }
    pos_y_offset = window_properties.drawableRect.posY;
    pos_x = (int32_t)request->pos_x;
    pos_y = (int32_t)request->pos_y - pos_y_offset;
    napi_create_int32(env, pos_x, &argv[0]);
    napi_create_int32(env, pos_y, &argv[1]);
    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_named_property(env, window, "moveWindowToGlobalDisplay", &fn);
    napi_call_function(env, window, fn, 2, argv, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnSetSize(napi_env env, napi_callback_info info)
{
    int32_t result;
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value retval = NULL;
    WindowManager_WindowProperties window_properties;
    int32_t height_offset = 0;
    int32_t width = 0, height = 0;
    napi_value window = NULL, fn = NULL, promise = NULL;
    napi_value argv[2] = { NULL };

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;

    result = OH_WindowManager_GetWindowProperties(request->ohos_window_id_value, &window_properties);
    if (result != 0) {
        napi_get_undefined(env, &retval);
        return retval;
    }
    height_offset = window_properties.windowRect.height - window_properties.drawableRect.height;
    width = request->width;
    height = request->height + height_offset;
    napi_create_int32(env, width, &argv[0]);
    napi_create_int32(env, height, &argv[1]);
    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_named_property(env, window, "resize", &fn);
    napi_call_function(env, window, fn, 2, argv, &promise);
    return promise;
}

static napi_value OHOS_JS_Window_Destroy(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnDestroy, request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_Create(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    if (g_ohos_main_window_id == 0) {
        g_ohos_main_window_id = request->sdl_window_id;
    }
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnCreate,         request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnWindowObtained, request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnContentLoaded,  request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetBordered,    request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetSize,        request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetPos,         request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetBordered(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetBordered,    request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetSize,        request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetPos,         request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetSize(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetSize,        request, NULL);
}
static napi_value OHOS_JS_Window_SetPos(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetPos,         request, NULL);
}

static napi_value OHOS_JS_Window_Dispatch(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;
    napi_value promise = NULL;
    napi_deferred deferred = NULL;
    OHOS_WindowRequest *request = NULL;
    void *data = NULL;
    OHOS_WindowEventQ *queue = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    queue = (OHOS_WindowEventQ *)data;
    request = OHOS_WindowEventQueue_Dequeue(queue);
    if (request == NULL) {
        queue->busy = SDL_FALSE;
        napi_get_undefined(env, &retval);
        return retval;
    }
    napi_create_promise(env, &deferred, &promise);

    request->ohos_window_ref_value = queue->ohos_window_ref;
    request->ohos_window_id_value = queue->ohos_window_id;
    switch (request->cmd) {
        case OHOS_WINDOW_CMD_CREATE:
            request->xc_slot = &queue->xc;
            request->parent_slot = &queue->parent;
            request->surface_holder_slot = &queue->surface_holder;
            request->surface_callback_slot = &queue->surface_callback;
            request->ohos_window_ref_slot = &queue->ohos_window_ref;
            request->ohos_window_id_slot = &queue->ohos_window_id;
            promise = OHOS_JS_Window_Create(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_DESTROY:
            request->xc_value = queue->xc;
            request->parent_value = queue->parent;
            request->surface_holder_value = queue->surface_holder;
            request->surface_callback_value = queue->surface_callback;
            SDL_free(queue);
            promise = OHOS_JS_Window_Destroy(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_SIZE:
            promise = OHOS_JS_Window_SetSize(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_POS:
            promise = OHOS_JS_Window_SetPos(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_TITLE:
        case OHOS_WINDOW_CMD_SET_FULLSCREEN:
        case OHOS_WINDOW_CMD_SHOW:
            break;
        case OHOS_WINDOW_CMD_SET_BORDERED:
            promise = OHOS_JS_Window_SetBordered(env, promise, request);
            break;
    }
    promise = OHOS_Tool_PromiseFinally(env, promise, OHOS_JS_Window_OnRequestDone, request, NULL);
    if (request->cmd != OHOS_WINDOW_CMD_DESTROY) {
        promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_Dispatch, queue, NULL);
    }
    napi_get_undefined(env, &retval);
    napi_resolve_deferred(env, deferred, retval);
    return retval;
}

static napi_value OHOS_JS_Window_Activate(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = {NULL};
    void *ptr = NULL;
    OHOS_WindowEventQ *queue = NULL;
    napi_value retval = NULL;
    napi_value dispatch_fn = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_get_value_external(env, argv[0], &ptr);
    queue = (OHOS_WindowEventQ *)ptr;
    ptr = NULL;
    if (queue->busy == SDL_TRUE) {
        napi_get_undefined(env, &retval);
        return retval;
    }
    queue->busy = SDL_TRUE;
    napi_create_function(env, "dispatch", NAPI_AUTO_LENGTH, OHOS_JS_Window_Dispatch, queue, &dispatch_fn);
    napi_call_function(env, NULL, dispatch_fn, 0, NULL, NULL);
    napi_get_undefined(env, &retval);
    return retval;
}

static void OHOS_JS_Window_OnRequest(napi_env env, napi_value js_cb, void *context, void *data)
{
    OHOS_WindowRequest *request = (OHOS_WindowRequest *)data;
    OHOS_WindowEventQ *queue = NULL;
    napi_value queue_ext = NULL;
    
    if (request == NULL) {
        return;
    }
    queue = OHOS_WindowEventQueue_Find(request->sdl_window_id);
    if (queue == NULL) {
        if (request->cmd != OHOS_WINDOW_CMD_CREATE) {
            SDL_free(data);
            return;
        }
        queue = OHOS_WindowEventQueue_Create(request->sdl_window_id);
        if (queue == NULL) {
            SDL_free(data);
            return;
        }
        queue->sem = request->sem;
    }
    if (request->cmd == OHOS_WINDOW_CMD_DESTROY) {
        OHOS_WindowEventQueue_Clear(queue);
        OHOS_WindowEventQueue_Unlink(queue);
    }
    OHOS_WindowEventQueue_Enqueue(queue, request);

    napi_create_external(env, queue, NULL, NULL, &queue_ext);

    napi_call_function(env, NULL, js_cb, 1, &queue_ext, NULL);
}

static napi_value OHOS_JS_Window_Init(napi_env env, napi_value exprts)
{
    napi_value activate_cb = NULL;
    napi_value resource_name = NULL;

    if (g_ohos_window_request_tsf != NULL) {
        return exprts;
    }
    napi_create_function(env, "activate", NAPI_AUTO_LENGTH, OHOS_JS_Window_Activate, NULL, &activate_cb);
    napi_create_string_utf8(env, "ohosWindowEventTSF", NAPI_AUTO_LENGTH, &resource_name);
    napi_create_threadsafe_function(env, activate_cb, NULL, resource_name, 0, 1, NULL, NULL, NULL, OHOS_JS_Window_OnRequest, &g_ohos_window_request_tsf);
    return exprts;
}

/* SDL thread-related processing interfaces */

int OHOS_Window_Create(Uint32 sdl_window_id, SDL_bool bordered, int width, int height, int pos_x, int pos_y)
{
    SDL_sem *sem = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_status status;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    sem = SDL_CreateSemaphore(0);
    if (request == NULL || sem == NULL) {
        SDL_DestroySemaphore(sem);
        SDL_free(request);
        return SDL_OutOfMemory();
    }
    request->cmd = OHOS_WINDOW_CMD_CREATE;
    request->sdl_window_id = sdl_window_id;
    request->bordered = bordered;
    request->width = width;
    request->height = height;
    request->pos_x = pos_x;
    request->pos_y = pos_y;
    request->sem = sem;
    status = napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Window_Create push status=%d", (int)status);
    if (status != napi_ok) {
        SDL_DestroySemaphore(sem);
        SDL_free(request);
        return SDL_SetError("OHOS window channel closed");
    }
    if (SDL_SemWaitTimeout(sem, OHOS_WINDOW_CREATE_TIMEOUT) != 0) {
        return SDL_SetError("OHOS window creation timed out");
    }
    return 0;
}

void OHOS_Window_Destroy(Uint32 sdl_window_id)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for destroy request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_DESTROY;
    request->sdl_window_id = sdl_window_id;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetPos(Uint32 sdl_window_id, int pos_x, int pos_y)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-position request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_POS;
    request->sdl_window_id = sdl_window_id;
    request->pos_x = pos_x;
    request->pos_y = pos_y;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetSize(Uint32 sdl_window_id, int width, int height)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-size request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_SIZE;
    request->sdl_window_id = sdl_window_id;
    request->width = width;
    request->height = height;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetBordered(Uint32 sdl_window_id, SDL_bool bordered, int width, int height, int pos_x, int pos_y)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-bordered request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_BORDERED;
    request->sdl_window_id = sdl_window_id;
    request->bordered = bordered;
    request->width = width;
    request->height = height;
    request->pos_x = pos_x;
    request->pos_y = pos_y;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetTitle(Uint32 sdl_window_id, const char *title)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-title request", sdl_window_id);
        return;
    }
    request->title = SDL_strdup(title ? title : "");
    if (request->title == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-title text", sdl_window_id);
        SDL_free(request);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_TITLE;
    request->sdl_window_id = sdl_window_id;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_Show(Uint32 sdl_window_id, SDL_bool is_shown)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for show request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SHOW;
    request->sdl_window_id = sdl_window_id;
    request->shown = is_shown;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

/* UIAbility Lifecycle Callbacks */

static napi_value OHOS_JS_UIAbility_OnDestroy(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    napi_delete_reference(env, g_ohos_ability_ref);
    g_ohos_ability_ref = NULL;
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnWillForeground(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    SDL_OnApplicationWillEnterForeground();
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnDidForeground(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    SDL_OnApplicationDidBecomeActive();
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnWillBackground(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    SDL_OnApplicationWillResignActive();
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnDidBackground(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    SDL_OnApplicationDidEnterBackground();
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnWindowStageCreate(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;
    size_t argc = 1;
    napi_value argv[1] = {NULL};

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (g_ohos_window_stage_ref) {
        napi_delete_reference(env, g_ohos_window_stage_ref);
        g_ohos_window_stage_ref = NULL;
    }
    napi_create_reference(env, argv[0], 1, &g_ohos_window_stage_ref);

    /* The window request channel must exist before SDL_main can ask for a window. */
    OHOS_JS_Window_Init(env, NULL);

    OHOS_Main_StartApp();

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnWindowStageWillDestroy(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    SDL_OnApplicationWillTerminate();
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnWindowStageDestroy(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    if (g_ohos_window_request_tsf != NULL) {
        napi_release_threadsafe_function(g_ohos_window_request_tsf, napi_tsfn_abort);
        g_ohos_window_request_tsf = NULL;
    }
    if (g_ohos_window_stage_ref != NULL) {
        napi_delete_reference(env, g_ohos_window_stage_ref);
        g_ohos_window_stage_ref = NULL;
    }
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_UIAbility_OnMemoryLevel(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    int32_t memory_level = 0;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_get_value_int32(env, argv[0], &memory_level);
    if (memory_level == 2) {
        SDL_OnApplicationDidReceiveMemoryWarning();
    }
    napi_get_undefined(env, &retval);
    return retval;
}

static void TakeOverUIAbility(napi_env env, napi_value ability)
{
    napi_value proto = NULL;
    napi_value fn = NULL;

    napi_get_prototype(env, ability, &proto);
    napi_create_function(env, "onDestroy",                NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnDestroy,                NULL, &fn);
    napi_set_named_property(env, proto, "onDestroy",                fn);
    napi_create_function(env, "onWillForeground",         NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnWillForeground,         NULL, &fn);
    napi_set_named_property(env, proto, "onWillForeground",         fn);
    napi_create_function(env, "onDidForeground",          NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnDidForeground,          NULL, &fn);
    napi_set_named_property(env, proto, "onDidForeground",          fn);
    napi_create_function(env, "onWillBackground",         NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnWillBackground,         NULL, &fn);
    napi_set_named_property(env, proto, "onWillBackground",         fn);
    napi_create_function(env, "onDidBackground",          NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnDidBackground,          NULL, &fn);
    napi_set_named_property(env, proto, "onDidBackground",          fn);
    napi_create_function(env, "onWindowStageCreate",      NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnWindowStageCreate,      NULL, &fn);
    napi_set_named_property(env, proto, "onWindowStageCreate",      fn);
    napi_create_function(env, "onWindowStageWillDestroy", NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnWindowStageWillDestroy, NULL, &fn);
    napi_set_named_property(env, proto, "onWindowStageWillDestroy", fn);
    napi_create_function(env, "onWindowStageDestroy",     NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnWindowStageDestroy,     NULL, &fn);
    napi_set_named_property(env, proto, "onWindowStageDestroy",     fn);
    napi_create_function(env, "onMemoryLevel",            NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnMemoryLevel,            NULL, &fn);
    napi_set_named_property(env, proto, "onMemoryLevel",            fn);
}

/* Register related interfaces and bind UIAbility lifecycle callback functions. */

static void OHOS_Window_SetPageName(const char *name)
{
    if (name != NULL && name[0] != '\0') {
        SDL_strlcpy(g_ohos_window_page_name, name, sizeof(g_ohos_window_page_name));
    }
}

static napi_value OHOS_Napi_ProvideArkTSObjects(napi_env env, napi_callback_info info)
{
    size_t argc =  OHOS_ARGS_MAX;
    napi_value argv[OHOS_ARGS_MAX] = {NULL};
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if(argc < OHOS_ARGS_REQUIRED){
        napi_throw_type_error(env, NULL, "invalid params");
        return NULL;
    }

    if(g_ohos_ability_ref){
        napi_delete_reference(env, g_ohos_ability_ref);
        g_ohos_ability_ref = NULL;
    }
    napi_create_reference(env, argv[OHOS_ARG_ABILITY], 1, &g_ohos_ability_ref);
    TakeOverUIAbility(env, argv[OHOS_ARG_ABILITY]);
    if (argc > OHOS_ARG_APP_LIBRARY) {
        size_t length = 0;
        char buffer[256];
        if (napi_get_value_string_utf8(env, argv[OHOS_ARG_APP_LIBRARY], buffer, sizeof(buffer), &length)==napi_ok) {
            OHOS_Main_SetAppLibrary(buffer);
        }
    }
    if (argc > OHOS_ARG_PAGE_NAME) {
        size_t length = 0;
        char buffer[256];
        if (napi_get_value_string_utf8(env, argv[OHOS_ARG_PAGE_NAME], buffer, sizeof(buffer), &length) == napi_ok) {
            OHOS_Window_SetPageName(buffer);
        }
    }

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_InitModule(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"provideArkTSObjects", NULL, OHOS_Napi_ProvideArkTSObjects, NULL, NULL, NULL, napi_default, NULL},
    };
    napi_define_properties(env, exports, sizeof(desc)/sizeof(desc[0]), desc);

    return exports;
}

static napi_module g_ohos_napi_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = OHOS_Napi_InitModule,
    .nm_modname = "SDL2",
    .nm_priv = NULL,
    .reserved = {0}
};

__attribute__((constructor)) void OHOS_Napi_RegisterModule(void)
{
    napi_module_register(&g_ohos_napi_module);
}

/* vi: set ts=4 sw=4 expandtab: */