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
#include <arkui/native_key_event.h>
#include <arkui/native_interface_focus.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <window_manager/oh_window_comm.h>
#include <window_manager/oh_window.h>

#include "SDL_system.h"
#include "SDL_mutex.h"
#include "SDL_events.h"
#include "SDL_main.h"
#include "../../events/SDL_events_c.h"

#include <dlfcn.h>
#include <pthread.h>

#include "../../video/ohos/SDL_ohosevents.h"

#define OHOS_ARGS_REQUIRED   1
#define OHOS_ARGS_MAX        4
#define OHOS_ARG_ABILITY     0
#define OHOS_ARG_APP_LIBRARY 1
#define OHOS_ARG_PAGE_NAME   2
#define OHOS_ARG_I18N        3

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
    OHOS_WINDOW_CMD_SET_BORDERED,
    OHOS_WINDOW_CMD_MAXIMIZE,
    OHOS_WINDOW_CMD_MINIMIZE,
    OHOS_WINDOW_CMD_RESTORE,
    OHOS_WINDOW_CMD_RAISE,
    OHOS_WINDOW_CMD_SET_INPUT_FOCUS,
    OHOS_WINDOW_CMD_SET_MINIMUM_SIZE,
    OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE,
    OHOS_WINDOW_CMD_SET_RESIZABLE
} OHOS_WindowCommand;

/* Subset of the ArkTS MaximizePresentation enum. */
typedef enum
{
    OHOS_WINDOW_PRESENTATION_EXIT_IMMERSIVE = 1,   /* maximize: title bar and dock stay visible */
    OHOS_WINDOW_PRESENTATION_ENTER_IMMERSIVE = 3   /* fullscreen: title bar and dock hover only */
} OHOS_WindowPresentation;

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
    SDL_bool                    resizable;
    SDL_bool                    fullscreen;
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
        request->next = NULL;
    }
    SDL_UnlockMutex(queue->lock);
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

static void OHOS_Node_OnMouse(ArkUI_NodeEvent *event)
{
    ArkUI_UIInputEvent *input_event;
    OHOS_FloatPair position, delta;
    SDL_Event sdl_event;
    Uint32 sdl_window_id;
    int32_t action;
    int32_t button;

    input_event = OH_ArkUI_NodeEvent_GetInputEvent(event);
    if (input_event == NULL) {
        return;
    }
    if (OH_ArkUI_UIInputEvent_GetType(input_event) != ARKUI_UIINPUTEVENT_TYPE_MOUSE) {
        return;
    }

    sdl_window_id = (Uint32)(uintptr_t)OH_ArkUI_NodeEvent_GetUserData(event);
    if (sdl_window_id == 0) {
        return;
    }

    action = OH_ArkUI_MouseEvent_GetMouseAction(input_event);

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_MOUSE;
    sdl_event.user.windowID = sdl_window_id;

    switch (action) {
    case UI_MOUSE_EVENT_ACTION_MOVE:
        position.f[0] = OH_ArkUI_PointerEvent_GetX(input_event);
        position.f[1] = OH_ArkUI_PointerEvent_GetY(input_event);
        delta.f[0] = OH_ArkUI_MouseEvent_GetRawDeltaX(input_event);
        delta.f[1] = OH_ArkUI_MouseEvent_GetRawDeltaY(input_event);

        sdl_event.user.code = OHOS_MOUSE_EVENT_MOVE;
        sdl_event.user.data1 = (void *)(intptr_t)position.u;
        sdl_event.user.data2 = (void *)(intptr_t)delta.u;
        break;

    case UI_MOUSE_EVENT_ACTION_PRESS:
    case UI_MOUSE_EVENT_ACTION_RELEASE:
        button = OH_ArkUI_MouseEvent_GetMouseButton(input_event);
        if (button == UI_MOUSE_EVENT_BUTTON_NONE) {
            return;
        }

        sdl_event.user.code = OHOS_MOUSE_EVENT_BUTTON;
        sdl_event.user.data1 = (void *)(intptr_t)button;
        sdl_event.user.data2 = (void *)(intptr_t)((action == UI_MOUSE_EVENT_ACTION_PRESS) ? SDL_PRESSED : SDL_RELEASED);
        break;

    case UI_MOUSE_EVENT_ACTION_CANCEL:
        sdl_event.user.code = OHOS_MOUSE_EVENT_CANCEL;
        break;

    default:
        return;
    }

    SDL_PushEvent(&sdl_event);
}

static void OHOS_Node_OnKeyEvent(ArkUI_NodeEvent *event)
{
    ArkUI_UIInputEvent *input_event;
    ArkUI_KeyEventType key_event_type;
    SDL_Event sdl_event;
    Uint32 sdl_window_id;
    int32_t key_code;

    input_event = OH_ArkUI_NodeEvent_GetInputEvent(event);
    if (input_event == NULL) {
        return;
    }
    if (OH_ArkUI_UIInputEvent_GetType(input_event) != ARKUI_UIINPUTEVENT_TYPE_KEY) {
        return;
    }

    sdl_window_id = (Uint32)(uintptr_t)OH_ArkUI_NodeEvent_GetUserData(event);
    if (sdl_window_id == 0) {
        return;
    }

    /* Only physical presses and releases matter; LONG_PRESS is SDL's own repeat
       business and CLICK is a synthesised down+up pair. */
    key_event_type = OH_ArkUI_KeyEvent_GetType(input_event);
    if (key_event_type != ARKUI_KEY_EVENT_DOWN &&
        key_event_type != ARKUI_KEY_EVENT_UP) {
        return;
    }

    key_code = OH_ArkUI_KeyEvent_GetKeyCode(input_event);
    if (key_code == ARKUI_KEYCODE_UNKNOWN) {
        return;
    }

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_KEYBOARD;
    sdl_event.user.code = OHOS_KEYBOARD_EVENT_KEY;
    sdl_event.user.windowID = sdl_window_id;
    sdl_event.user.data1 = (void *)(intptr_t)key_code;
    sdl_event.user.data2 = (void *)(intptr_t)key_event_type;
    SDL_PushEvent(&sdl_event);
}

static void OHOS_Node_OnTouchEvent(ArkUI_NodeEvent *event)
{
    ArkUI_UIInputEvent *input_event;
    OHOS_FloatPair position;
    SDL_Event sdl_event;
    Uint32 sdl_window_id;
    uint32_t changed_index = 0;
    int32_t action;
    int32_t finger_id;

    input_event = OH_ArkUI_NodeEvent_GetInputEvent(event);
    if (input_event == NULL) {
        return;
    }
    if (OH_ArkUI_UIInputEvent_GetType(input_event) != ARKUI_UIINPUTEVENT_TYPE_TOUCH) {
        return;
    }

    sdl_window_id = (Uint32)(uintptr_t)OH_ArkUI_NodeEvent_GetUserData(event);
    if (sdl_window_id == 0) {
        return;
    }

    /* A touch event carries the whole set of pointers, but only the one that
       changed is a new sample for SDL, so it is pulled out by index. */
    if (OH_ArkUI_PointerEvent_GetChangedPointerId(input_event, &changed_index) != ARKUI_ERROR_CODE_NO_ERROR) {
        return;
    }

    action = OH_ArkUI_UIInputEvent_GetAction(input_event);
    finger_id = OH_ArkUI_PointerEvent_GetPointerId(input_event, changed_index);
    position.f[0] = OH_ArkUI_PointerEvent_GetXByIndex(input_event, changed_index);
    position.f[1] = OH_ArkUI_PointerEvent_GetYByIndex(input_event, changed_index);

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_TOUCH;
    sdl_event.user.code = OHOS_TOUCH_EVENT_POINT;
    sdl_event.user.windowID = sdl_window_id;
    sdl_event.user.data1 = (void *)(intptr_t)position.u;
    sdl_event.user.data2 = (void *)(intptr_t)(((Uint64)(Uint32)action << 32) | (Uint32)finger_id);
    SDL_PushEvent(&sdl_event);
}

static void OHOS_Node_PushFocusEvent(ArkUI_NodeEvent *event, SDL_bool focused)
{
    SDL_Event sdl_event;
    Uint32 sdl_window_id;

    sdl_window_id = (Uint32)(uintptr_t)OH_ArkUI_NodeEvent_GetUserData(event);
    if (sdl_window_id == 0) {
        return;
    }

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    sdl_event.user.code = OHOS_WINDOW_EVENT_FOCUS_CHANGED;
    sdl_event.user.windowID = sdl_window_id;
    sdl_event.user.data1 = (void *)(intptr_t)focused;
    SDL_PushEvent(&sdl_event);
}

static void OHOS_Node_OnFocus(ArkUI_NodeEvent *event)
{
    OHOS_Node_PushFocusEvent(event, SDL_TRUE);
}

static void OHOS_Node_OnBlur(ArkUI_NodeEvent *event)
{
    OHOS_Node_PushFocusEvent(event, SDL_FALSE);
}

static void OHOS_Node_RegisterEvents(ArkUI_NodeHandle node, Uint32 sdl_window_id)
{
    int32_t result;
    void *user_data = (void *)(intptr_t)sdl_window_id;

    result = OH_ArkUI_NativeModule_RegisterCommonEvent(node, NODE_TOUCH_EVENT, user_data, OHOS_Node_OnTouchEvent);
    if (result != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Node_RegisterEvents: NODE_TOUCH_EVENT failed (%d)", (int)result);
    }

    result = OH_ArkUI_NativeModule_RegisterCommonEvent(node, NODE_ON_MOUSE, user_data, OHOS_Node_OnMouse);
    if (result != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Node_RegisterEvents: NODE_ON_MOUSE failed (%d)", (int)result);
    }

    result = OH_ArkUI_NativeModule_RegisterCommonEvent(node, NODE_ON_KEY_EVENT, user_data, OHOS_Node_OnKeyEvent);
    if (result != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Node_RegisterEvents: NODE_ON_KEY_EVENT failed (%d)", (int)result);
    }

    result = OH_ArkUI_NativeModule_RegisterCommonEvent(node, NODE_ON_FOCUS, user_data, OHOS_Node_OnFocus);
    if (result != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Node_RegisterEvents: NODE_ON_FOCUS failed (%d)", (int)result);
    }

    result = OH_ArkUI_NativeModule_RegisterCommonEvent(node, NODE_ON_BLUR, user_data, OHOS_Node_OnBlur);
    if (result != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS_Node_RegisterEvents: NODE_ON_BLUR failed (%d)", (int)result);
    }
}

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
    percent.i32 = 1;
    api->setAttribute(xc, NODE_FOCUSABLE, &com_item);
    api->setLengthMetricUnit(xc, ARKUI_LENGTH_METRIC_UNIT_PX);

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
static napi_ref                 g_ohos_i18n_ref =           NULL;
static napi_ref                 g_ohos_window_stage_ref =   NULL;
static Uint32                   g_ohos_main_window_id =     0;

static napi_threadsafe_function g_ohos_window_request_tsf = NULL;

/* Window event listeners */

typedef struct OHOS_WindowListener
{
    Uint32  sdl_window_id;
    int32_t ohos_window_id;
} OHOS_WindowListener;

static napi_value OHOS_JS_Window_OnWindowEvent(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;
    void *data = NULL;
    OHOS_WindowListener *listener = NULL;
    SDL_Event event;
    int32_t event_type = 0;

    napi_get_cb_info(env, info, &argc, argv, NULL, &data);
    listener = (OHOS_WindowListener *)data;
    napi_get_value_int32(env, argv[0], &event_type);

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    event.user.code = OHOS_WINDOW_EVENT_STATE_CHANGED;
    event.user.windowID = listener->sdl_window_id;
    event.user.data1 = (void *)(uintptr_t)event_type;
    SDL_PushEvent(&event);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_Window_OnRectChange(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;
    void *data = NULL;
    OHOS_WindowListener *listener = NULL;
    WindowManager_WindowProperties properties;
    SDL_Rect *rect = NULL;
    SDL_Event event;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    listener = (OHOS_WindowListener *)data;
    rect = (SDL_Rect *)SDL_calloc(1, sizeof(SDL_Rect));

    OH_WindowManager_GetWindowProperties(listener->ohos_window_id, &properties);

    /* drawableRect is relative to the window origin, windowRect to the screen origin. */
    rect->x = (int32_t)properties.windowRect.posX + (int32_t)properties.drawableRect.posX;
    rect->y = (int32_t)properties.windowRect.posY + (int32_t)properties.drawableRect.posY;
    rect->w = (int32_t)properties.drawableRect.width;
    rect->h = (int32_t)properties.drawableRect.height;

    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    event.user.code = OHOS_WINDOW_EVENT_RECT_CHANGED;
    event.user.windowID = listener->sdl_window_id;
    event.user.data1 = (void *)rect;
    event.user.data2 = (void *)(uintptr_t)properties.displayId;
    SDL_PushEvent(&event);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_Window_OnStatusChange(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;
    void *data = NULL;
    OHOS_WindowListener *listener = NULL;
    SDL_Event sdl_event;
    int32_t status = 0;

    napi_get_cb_info(env, info, &argc, argv, NULL, &data);
    listener = (OHOS_WindowListener *)data;
    napi_get_value_int32(env, argv[0], &status);

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    sdl_event.user.code = OHOS_WINDOW_EVENT_STATUS_CHANGED;
    sdl_event.user.windowID = listener->sdl_window_id;
    sdl_event.user.data1 = (void *)(intptr_t)status;
    SDL_PushEvent(&sdl_event);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_Window_OnDisplayIdChange(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;
    void *data = NULL;
    OHOS_WindowListener *listener = NULL;
    SDL_Event sdl_event;
    int32_t display_id = 0;

    napi_get_cb_info(env, info, &argc, argv, NULL, &data);
    listener = (OHOS_WindowListener *)data;
    napi_get_value_int32(env, argv[0], &display_id);

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    sdl_event.user.code = OHOS_WINDOW_EVENT_DISPLAY_CHANGED;
    sdl_event.user.windowID = listener->sdl_window_id;
    sdl_event.user.data1 = (void *)(intptr_t)display_id;
    SDL_PushEvent(&sdl_event);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_JS_Window_OnVisibilityChange(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;
    void *data = NULL;
    OHOS_WindowListener *listener = NULL;
    SDL_Event sdl_event;
    bool visible = false;

    napi_get_cb_info(env, info, &argc, argv, NULL, &data);
    listener = (OHOS_WindowListener *)data;
    napi_get_value_bool(env, argv[0], &visible);

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    sdl_event.user.code = OHOS_WINDOW_EVENT_VISIBILITY_CHANGED;
    sdl_event.user.windowID = listener->sdl_window_id;
    sdl_event.user.data1 = (void *)(intptr_t)(visible ? SDL_TRUE : SDL_FALSE);
    SDL_PushEvent(&sdl_event);

    napi_get_undefined(env, &retval);
    return retval;
}

static void OHOS_JS_Window_RegisterListeners(napi_env env, napi_value window, Uint32 sdl_window_id, int32_t ohos_window_id)
{
    napi_value on_fn = NULL;
    napi_value type = NULL;
    napi_value handler = NULL;
    napi_value argv[2];
    napi_value ignored = NULL;
    OHOS_WindowListener *listener = NULL;
    void *data = NULL;

    listener = (OHOS_WindowListener *)SDL_calloc(1, sizeof(OHOS_WindowListener));
    if (listener == NULL) {
        SDL_OutOfMemory();
        return;
    }
    listener->sdl_window_id = sdl_window_id;
    listener->ohos_window_id = ohos_window_id;
    data = (void *)listener;

    napi_get_named_property(env, window, "on", &on_fn);

    napi_create_string_utf8(env, "windowEvent", NAPI_AUTO_LENGTH, &type);
    napi_create_function(env, "ohosWindowEvent", NAPI_AUTO_LENGTH, OHOS_JS_Window_OnWindowEvent, data, &handler);
    argv[0] = type;
    argv[1] = handler;
    napi_call_function(env, window, on_fn, 2, argv, &ignored);

    napi_create_string_utf8(env, "windowRectChange", NAPI_AUTO_LENGTH, &type);
    napi_create_function(env, "ohosWindowRectChange", NAPI_AUTO_LENGTH, OHOS_JS_Window_OnRectChange, data, &handler);
    argv[0] = type;
    argv[1] = handler;
    napi_call_function(env, window, on_fn, 2, argv, &ignored);

    napi_create_string_utf8(env, "windowStatusChange", NAPI_AUTO_LENGTH, &type);
    napi_create_function(env, "ohosWindowStatusChange", NAPI_AUTO_LENGTH, OHOS_JS_Window_OnStatusChange, data, &handler);
    argv[0] = type;
    argv[1] = handler;
    napi_call_function(env, window, on_fn, 2, argv, &ignored);

    napi_create_string_utf8(env, "displayIdChange", NAPI_AUTO_LENGTH, &type);
    napi_create_function(env, "ohosDisplayIdChange", NAPI_AUTO_LENGTH, OHOS_JS_Window_OnDisplayIdChange, data, &handler);
    argv[0] = type;
    argv[1] = handler;
    napi_call_function(env, window, on_fn, 2, argv, &ignored);

    napi_create_string_utf8(env, "windowVisibilityChange", NAPI_AUTO_LENGTH, &type);
    napi_create_function(env, "ohosWindowVisibilityChange", NAPI_AUTO_LENGTH, OHOS_JS_Window_OnVisibilityChange, data, &handler);
    argv[0] = type;
    argv[1] = handler;
    napi_call_function(env, window, on_fn, 2, argv, &ignored);
}

static void OHOS_JS_Window_UnregisterListeners(napi_env env, napi_value window)
{
    napi_value off_fn = NULL;
    napi_value type = NULL;
    napi_value ignored = NULL;

    napi_get_named_property(env, window, "off", &off_fn);

    napi_create_string_utf8(env, "windowEvent", NAPI_AUTO_LENGTH, &type);
    napi_call_function(env, window, off_fn, 1, &type, &ignored);

    napi_create_string_utf8(env, "windowRectChange", NAPI_AUTO_LENGTH, &type);
    napi_call_function(env, window, off_fn, 1, &type, &ignored);

    napi_create_string_utf8(env, "windowStatusChange", NAPI_AUTO_LENGTH, &type);
    napi_call_function(env, window, off_fn, 1, &type, &ignored);

    napi_create_string_utf8(env, "displayIdChange", NAPI_AUTO_LENGTH, &type);
    napi_call_function(env, window, off_fn, 1, &type, &ignored);

    napi_create_string_utf8(env, "windowVisibilityChange", NAPI_AUTO_LENGTH, &type);
    napi_call_function(env, window, off_fn, 1, &type, &ignored);
}

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

    /* The content is loaded before the XComponent exists, so nothing focuses it.
       Once focused, the framework follows the window on its own. */
    if (OH_ArkUI_FocusRequest(xc_handle) != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OH_ArkUI_FocusRequest: focusing the XComponent failed");
    }

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
    SDL_Event event;

    napi_get_cb_info(env, info, &argc, argv, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_create_reference(env, argv[0], 1, request->ohos_window_ref_slot);
    request->ohos_window_ref_value = *request->ohos_window_ref_slot;
    napi_get_named_property(env, argv[0], "getWindowProperties", &fn);
    napi_call_function(env, argv[0], fn, 0, NULL, &properteis);
    napi_get_named_property(env, properteis, "id", &id);
    napi_get_value_int32(env, id, request->ohos_window_id_slot);
    request->ohos_window_id_value = *request->ohos_window_id_slot;

    /* Hand the OHOS window id to the SDL thread; SDL_WindowData stores it for the
       cursor hooks, which need it to address the window. */
    SDL_zero(event);
    event.type = g_ohos_event_base + OHOS_EVENT_WINDOW;
    event.user.code = OHOS_WINDOW_EVENT_ID_ASSIGNED;
    event.user.windowID = request->sdl_window_id;
    event.user.data1 = (void *)(intptr_t)request->ohos_window_id_value;
    SDL_PushEvent(&event);

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
        napi_delete_reference(env, request->ohos_window_ref_value);
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

static napi_value OHOS_JS_Window_OnSetResizable(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, arg = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_boolean(env, request->resizable ? true : false, &arg);
    napi_get_named_property(env, window, "setResizeByDragEnabled", &fn);
    napi_call_function(env, window, fn, 1, &arg, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnSetWindowLimits(napi_env env, OHOS_WindowRequest *request, SDL_bool minimum)
{
    napi_value window = NULL, limits = NULL, fn = NULL, promise = NULL;
    napi_value value = NULL;

    napi_get_reference_value(env, request->ohos_window_ref_value, &window);

    /* Unset members stay at 0, which keeps the current limit untouched. */
    napi_create_object(env, &limits);
    napi_create_int32(env, request->width, &value);
    napi_set_named_property(env, limits, minimum ? "minWidth" : "maxWidth", value);
    napi_create_int32(env, request->height, &value);
    napi_set_named_property(env, limits, minimum ? "minHeight" : "maxHeight", value);

    napi_get_named_property(env, window, "setWindowLimits", &fn);
    napi_call_function(env, window, fn, 1, &limits, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnSetMinimumSize(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    return OHOS_JS_Window_OnSetWindowLimits(env, request, SDL_TRUE);
}

static napi_value OHOS_JS_Window_OnSetMaximumSize(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    return OHOS_JS_Window_OnSetWindowLimits(env, request, SDL_FALSE);
}

static napi_value OHOS_JS_Window_OnMaximize(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, arg = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    if (request->sdl_window_id != g_ohos_main_window_id) {
        napi_get_undefined(env, &promise);
        return promise;
    }

    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_named_property(env, window, "maximize", &fn);
    napi_create_int32(env, OHOS_WINDOW_PRESENTATION_EXIT_IMMERSIVE, &arg);
    napi_call_function(env, window, fn, 1, &arg, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnMinimize(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    if (request->sdl_window_id != g_ohos_main_window_id) {
        napi_get_undefined(env, &promise);
        return promise;
    }

    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_named_property(env, window, "minimize", &fn);
    napi_call_function(env, window, fn, 0, NULL, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnRestore(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    if (request->sdl_window_id != g_ohos_main_window_id) {
        napi_get_undefined(env, &promise);
        return promise;
    }

    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_named_property(env, window, "restore", &fn);
    napi_call_function(env, window, fn, 0, NULL, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnRaise(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;

    /* raiseToAppTop() only applies to subwindows. */
    if (request->sdl_window_id == g_ohos_main_window_id) {
        napi_get_undefined(env, &promise);
        return promise;
    }

    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_get_named_property(env, window, "raiseToAppTop", &fn);
    napi_call_function(env, window, fn, 0, NULL, &promise);

    return promise;
}

static napi_value OHOS_JS_Window_OnShow(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_get_reference_value(env, request->ohos_window_ref_value, &window);

    if (request->shown) {
        /* There is no hide(); a hidden main window comes back with restore(). */
        napi_get_named_property(env, window,
                                (request->sdl_window_id == g_ohos_main_window_id) ? "restore" : "showWindow",
                                &fn);
        napi_call_function(env, window, fn, 0, NULL, &promise);
    } else {
        /* There is no hide(); minimizing is the only way to take it off screen. */
        napi_get_named_property(env, window, "minimize", &fn);
        napi_call_function(env, window, fn, 0, NULL, &promise);
    }

    return promise;
}

static napi_value OHOS_JS_Window_OnSetFullscreen(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, arg = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    if (request->sdl_window_id != g_ohos_main_window_id) {
        napi_get_undefined(env, &promise);
        return promise;
    }

    napi_get_reference_value(env, request->ohos_window_ref_value, &window);

    if (request->fullscreen) {
        napi_get_named_property(env, window, "maximize", &fn);
        napi_create_int32(env, OHOS_WINDOW_PRESENTATION_ENTER_IMMERSIVE, &arg);
        napi_call_function(env, window, fn, 1, &arg, &promise);
    } else {
        napi_get_named_property(env, window, "restore", &fn);
        napi_call_function(env, window, fn, 0, NULL, &promise);
    }

    return promise;
}

static napi_value OHOS_JS_Window_OnSetTitle(napi_env env, napi_callback_info info)
{
    void *data = NULL;
    OHOS_WindowRequest *request = NULL;
    napi_value window = NULL, fn = NULL, arg = NULL, promise = NULL;

    napi_get_cb_info(env, info, 0, NULL, NULL, &data);
    request = (OHOS_WindowRequest *)data;
    napi_get_reference_value(env, request->ohos_window_ref_value, &window);
    napi_create_string_utf8(env, request->title, NAPI_AUTO_LENGTH, &arg);
    napi_get_named_property(env, window, "setWindowTitle", &fn);
    napi_call_function(env, window, fn, 1, &arg, &promise);

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
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetResizable,   request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetBordered(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetBordered,    request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetSize,        request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetPos,         request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_Show(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnShow,           request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_Raise(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnRaise,          request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_Restore(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnRestore,        request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetFullscreen(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetFullscreen,  request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetResizable(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetResizable,   request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetMinimumSize(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetMinimumSize,  request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetMaximumSize(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetMaximumSize,  request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_Maximize(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnMaximize,       request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_Minimize(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnMinimize,       request, NULL);
    return promise;
}

static napi_value OHOS_JS_Window_SetTitle(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnSetTitle,       request, NULL);
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
            promise = OHOS_JS_Window_SetTitle(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_FULLSCREEN:
            promise = OHOS_JS_Window_SetFullscreen(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SHOW:
            promise = OHOS_JS_Window_Show(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_BORDERED:
            promise = OHOS_JS_Window_SetBordered(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_MAXIMIZE:
            promise = OHOS_JS_Window_Maximize(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_MINIMIZE:
            promise = OHOS_JS_Window_Minimize(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_RESTORE:
            promise = OHOS_JS_Window_Restore(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_RAISE:
            promise = OHOS_JS_Window_Raise(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_INPUT_FOCUS:
            break;
        case OHOS_WINDOW_CMD_SET_MINIMUM_SIZE:
            promise = OHOS_JS_Window_SetMinimumSize(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE:
            promise = OHOS_JS_Window_SetMaximumSize(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_SET_RESIZABLE:
            promise = OHOS_JS_Window_SetResizable(env, promise, request);
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

int OHOS_Window_Create(Uint32 sdl_window_id, SDL_bool bordered, int width, int height, int pos_x, int pos_y, SDL_bool resizable)
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
    request->resizable = resizable;
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

void OHOS_Window_Show(Uint32 sdl_window_id)
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
    request->shown = SDL_TRUE;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_Hide(Uint32 sdl_window_id)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for hide request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SHOW;
    request->sdl_window_id = sdl_window_id;
    request->shown = SDL_FALSE;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetMinimumSize(Uint32 sdl_window_id, int width, int height)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-minimum-size request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_MINIMUM_SIZE;
    request->sdl_window_id = sdl_window_id;
    request->width = width;
    request->height = height;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetMaximumSize(Uint32 sdl_window_id, int width, int height)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-maximum-size request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE;
    request->sdl_window_id = sdl_window_id;
    request->width = width;
    request->height = height;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

int OHOS_Window_SetOpacity(Uint32 sdl_window_id, float opacity)
{
    (void)sdl_window_id;
    (void)opacity;

    return SDL_Unsupported();
}

int OHOS_Window_SetModalFor(Uint32 modal_window_id, Uint32 parent_window_id)
{
    (void)modal_window_id;
    (void)parent_window_id;

    return SDL_Unsupported();
}

int OHOS_Window_SetInputFocus(Uint32 sdl_window_id)
{
    (void)sdl_window_id;

    return SDL_Unsupported();
}

void OHOS_Window_Raise(Uint32 sdl_window_id)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for raise request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_RAISE;
    request->sdl_window_id = sdl_window_id;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_Maximize(Uint32 sdl_window_id)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for maximize request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_MAXIMIZE;
    request->sdl_window_id = sdl_window_id;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_Minimize(Uint32 sdl_window_id)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for minimize request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_MINIMIZE;
    request->sdl_window_id = sdl_window_id;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_Restore(Uint32 sdl_window_id)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for restore request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_RESTORE;
    request->sdl_window_id = sdl_window_id;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetResizable(Uint32 sdl_window_id, SDL_bool resizable)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-resizable request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_RESIZABLE;
    request->sdl_window_id = sdl_window_id;
    request->resizable = resizable;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetAlwaysOnTop(Uint32 sdl_window_id, SDL_bool on_top)
{
    (void)sdl_window_id;
    (void)on_top;

    SDL_Unsupported();
}

void OHOS_Window_SetFullscreen(Uint32 sdl_window_id, SDL_bool fullscreen)
{
    OHOS_WindowRequest *request = NULL;

    request = (OHOS_WindowRequest *)SDL_calloc(1, sizeof(OHOS_WindowRequest));
    if (request == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS window %u: out of memory for set-fullscreen request", sdl_window_id);
        return;
    }
    request->cmd = OHOS_WINDOW_CMD_SET_FULLSCREEN;
    request->sdl_window_id = sdl_window_id;
    request->fullscreen = fullscreen;
    napi_call_threadsafe_function(g_ohos_window_request_tsf, request, napi_tsfn_nonblocking);
}

void OHOS_Window_SetMouseGrab(Uint32 sdl_window_id, SDL_bool grabbed)
{
    (void)sdl_window_id;
    (void)grabbed;

    SDL_Unsupported();
}

void OHOS_Window_SetKeyboardGrab(Uint32 sdl_window_id, SDL_bool grabbed)
{
    (void)sdl_window_id;
    (void)grabbed;

    SDL_Unsupported();
}

int OHOS_Window_SetGammaRamp(Uint32 sdl_window_id, const Uint16 *ramp)
{
    (void)sdl_window_id;
    (void)ramp;

    return SDL_Unsupported();
}

int OHOS_Window_GetGammaRamp(Uint32 sdl_window_id, Uint16 *ramp)
{
    (void)sdl_window_id;
    (void)ramp;

    return SDL_Unsupported();
}

void *OHOS_Window_GetICCProfile(Uint32 sdl_window_id, size_t *size)
{
    (void)sdl_window_id;
    (void)size;
    return NULL;
}

void OHOS_Window_SetMouseRect(Uint32 sdl_window_id)
{
    (void)sdl_window_id;

    SDL_Unsupported();
}

int OHOS_Window_Flash(Uint32 sdl_window_id, SDL_FlashOperation operation)
{
    (void)sdl_window_id;
    (void)operation;

    return SDL_Unsupported();
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

/* System locale */

#define OHOS_LOCALE_MAX 64

static char      g_ohos_system_locale[OHOS_LOCALE_MAX];
static SDL_mutex *g_ohos_locale_lock = NULL;

/* Runs on the JS thread: the i18n object is only reachable through NAPI there. */
static SDL_bool OHOS_JS_UpdateSystemLocale(napi_env env)
{
    napi_value i18n = NULL, fn = NULL, language = NULL, region = NULL;
    char language_buf[16] = { 0 };
    char region_buf[16] = { 0 };
    char locale[OHOS_LOCALE_MAX];
    char *dash;
    SDL_bool changed = SDL_FALSE;

    if (g_ohos_i18n_ref == NULL) {
        return SDL_FALSE;
    }
    napi_get_reference_value(env, g_ohos_i18n_ref, &i18n);
    if (i18n == NULL) {
        return SDL_FALSE;
    }

    napi_get_named_property(env, i18n, "getSystemLanguage", &fn);
    napi_call_function(env, i18n, fn, 0, NULL, &language);
    napi_get_named_property(env, i18n, "getSystemRegion", &fn);
    napi_call_function(env, i18n, fn, 0, NULL, &region);

    if (napi_get_value_string_utf8(env, language, language_buf, sizeof(language_buf), NULL) != napi_ok ||
        napi_get_value_string_utf8(env, region, region_buf, sizeof(region_buf), NULL) != napi_ok) {
        return SDL_FALSE;
    }

    /* "zh-Hans" carries a script subtag that SDL does not expect. */
    dash = SDL_strchr(language_buf, '-');
    if (dash != NULL) {
        *dash = '\0';
    }

    SDL_snprintf(locale, sizeof(locale), "%s_%s", language_buf, region_buf);

    if (g_ohos_locale_lock != NULL) {
        SDL_LockMutex(g_ohos_locale_lock);
    }
    if (SDL_strcmp(g_ohos_system_locale, locale) != 0) {
        SDL_strlcpy(g_ohos_system_locale, locale, sizeof(g_ohos_system_locale));
        changed = SDL_TRUE;
    }
    if (g_ohos_locale_lock != NULL) {
        SDL_UnlockMutex(g_ohos_locale_lock);
    }

    return changed;
}

void OHOS_Locale_GetPreferredLocales(char *buf, size_t buflen)
{
    if (buf == NULL || buflen == 0) {
        return;
    }

    if (g_ohos_locale_lock != NULL) {
        SDL_LockMutex(g_ohos_locale_lock);
    }
    SDL_strlcpy(buf, g_ohos_system_locale, buflen);
    if (g_ohos_locale_lock != NULL) {
        SDL_UnlockMutex(g_ohos_locale_lock);
    }
}

static napi_value OHOS_JS_UIAbility_OnConfigurationUpdate(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;

    /* The locale is the only configuration entry this backend tracks. */
    if (OHOS_JS_UpdateSystemLocale(env)) {
        SDL_Event sdl_event;

        SDL_zero(sdl_event);
        sdl_event.type = SDL_LOCALECHANGED;
        SDL_PushEvent(&sdl_event);
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
    napi_create_function(env, "onConfigurationUpdate",    NAPI_AUTO_LENGTH, OHOS_JS_UIAbility_OnConfigurationUpdate,    NULL, &fn);
    napi_set_named_property(env, proto, "onConfigurationUpdate",    fn);
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
    if (argc > OHOS_ARG_I18N) {
        if (g_ohos_i18n_ref) {
            napi_delete_reference(env, g_ohos_i18n_ref);
            g_ohos_i18n_ref = NULL;
        }
        napi_create_reference(env, argv[OHOS_ARG_I18N], 1, &g_ohos_i18n_ref);

        if (g_ohos_locale_lock == NULL) {
            g_ohos_locale_lock = SDL_CreateMutex();
        }
        /* Seed the cache now, while we know we are on the JS thread. */
        OHOS_JS_UpdateSystemLocale(env);
    }

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnCreate(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value argv[3] = { NULL, NULL, NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnNewWant(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value argv[3] = { NULL, NULL, NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnWindowStageCreate(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2] = { NULL, NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnWindowStageWillDestroy(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2] = { NULL, NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    SDL_SendAppEvent(SDL_APP_TERMINATING);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnWindowStageRestore(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2] = { NULL, NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnWindowStageDestroy(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnWillForeground(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    SDL_SendAppEvent(SDL_APP_WILLENTERFOREGROUND);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnForeground(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnDidForeground(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    SDL_SendAppEvent(SDL_APP_DIDENTERFOREGROUND);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnWillBackground(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnBackground(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnDidBackground(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    SDL_SendAppEvent(SDL_APP_DIDENTERBACKGROUND);
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnPrepareToTerminate(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;
    napi_valuetype ability_type = napi_undefined;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_typeof(env, argv[0], &ability_type);
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "OHOS ability onPrepareToTerminate: argc=%u ability=%p(%d)",
                (unsigned int)argc, (void *)argv[0], (int)ability_type);

    napi_get_boolean(env, true, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnPrepareToTerminateAsync(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;
    napi_value resolution = NULL;
    napi_deferred deferred = NULL;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    napi_create_promise(env, &deferred, &retval);
    napi_get_boolean(env, true, &resolution);
    napi_resolve_deferred(env, deferred, resolution);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnConfigurationUpdate(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2] = { NULL, NULL };
    napi_value retval = NULL;
    napi_valuetype ability_type = napi_undefined;
    napi_valuetype config_type = napi_undefined;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_typeof(env, argv[0], &ability_type);
    napi_typeof(env, argv[1], &config_type);
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "OHOS ability onConfigurationUpdate: argc=%u ability=%p(%d) newConfig=%p(%d)",
                (unsigned int)argc, (void *)argv[0], (int)ability_type, (void *)argv[1],
                (int)config_type);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnMemoryLevel(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2] = { NULL, NULL };
    napi_value retval = NULL;
    int32_t level = 0;
    napi_valuetype ability_type = napi_undefined;
    napi_valuetype level_type = napi_undefined;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_typeof(env, argv[0], &ability_type);
    napi_typeof(env, argv[1], &level_type);
    napi_get_value_int32(env, argv[1], &level);
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "OHOS ability onMemoryLevel: argc=%u ability=%p(%d) level=%d(%d)",
                (unsigned int)argc, (void *)argv[0], (int)ability_type, (int)level, (int)level_type);

    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value OHOS_Napi_HandleAbilityOnContinue(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2] = { NULL, NULL };
    napi_value retval = NULL;
    napi_valuetype ability_type = napi_undefined;
    napi_valuetype want_param_type = napi_undefined;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_typeof(env, argv[0], &ability_type);
    napi_typeof(env, argv[1], &want_param_type);
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "OHOS ability onContinue: argc=%u ability=%p(%d) wantParam=%p(%d)",
                (unsigned int)argc, (void *)argv[0], (int)ability_type, (void *)argv[1],
                (int)want_param_type);

    napi_create_int32(env, 0, &retval);
    return retval;
}







static napi_value OHOS_Napi_HandleAbilityOnDestroy(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { NULL };
    napi_value retval = NULL;
    napi_valuetype ability_type = napi_undefined;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    napi_typeof(env, argv[0], &ability_type);
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "OHOS ability onDestroy: argc=%u ability=%p(%d)",
                (unsigned int)argc, (void *)argv[0], (int)ability_type);

    napi_get_undefined(env, &retval);
    return retval;
}
/* Exposed to the page: addNode(nodeContent, sdlWindowId) receives the NodeContent the
   page provides and the SDL window the node has to be created for. */
static napi_value OHOS_Napi_AddNode(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2] = { NULL, NULL };
    napi_value retval = NULL;
    ArkUI_NodeContentHandle content = NULL;
    uint32_t sdl_window_id = 0;
    int32_t content_status;
    napi_status id_status;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (argc < 2) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OHOS addNode: expected (nodeContent, sdlWindowId)");
        napi_get_boolean(env, false, &retval);
        return retval;
    }

    content_status = OH_ArkUI_GetNodeContentFromNapiValue(env, argv[0], &content);
    id_status = napi_get_value_uint32(env, argv[1], &sdl_window_id);

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "OHOS addNode: nodeContent=%p contentStatus=%d sdlWindowId=%u idStatus=%d",
                (void *)content, (int)content_status, sdl_window_id, (int)id_status);

    napi_get_boolean(env, content != NULL && id_status == napi_ok, &retval);
    return retval;
}

static napi_value OHOS_Napi_InitModule(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"handleAbilityOnCreate",                    NULL, OHOS_Napi_HandleAbilityOnCreate, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnNewWant",                   NULL, OHOS_Napi_HandleAbilityOnNewWant, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnWindowStageCreate",         NULL, OHOS_Napi_HandleAbilityOnWindowStageCreate, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnWindowStageWillDestroy",    NULL, OHOS_Napi_HandleAbilityOnWindowStageWillDestroy, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnWindowStageRestore",        NULL, OHOS_Napi_HandleAbilityOnWindowStageRestore, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnWindowStageDestroy",        NULL, OHOS_Napi_HandleAbilityOnWindowStageDestroy, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnWillForeground",            NULL, OHOS_Napi_HandleAbilityOnWillForeground, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnForeground",                NULL, OHOS_Napi_HandleAbilityOnForeground, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnDidForeground",             NULL, OHOS_Napi_HandleAbilityOnDidForeground, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnWillBackground",            NULL, OHOS_Napi_HandleAbilityOnWillBackground, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnBackground",                NULL, OHOS_Napi_HandleAbilityOnBackground, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnDidBackground",             NULL, OHOS_Napi_HandleAbilityOnDidBackground, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnPrepareToTerminate",        NULL, OHOS_Napi_HandleAbilityOnPrepareToTerminate, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnPrepareToTerminateAsync",   NULL, OHOS_Napi_HandleAbilityOnPrepareToTerminateAsync, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnConfigurationUpdate",       NULL, OHOS_Napi_HandleAbilityOnConfigurationUpdate, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnMemoryLevel",               NULL, OHOS_Napi_HandleAbilityOnMemoryLevel, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnContinue",                  NULL, OHOS_Napi_HandleAbilityOnContinue, NULL, NULL, NULL, napi_default, NULL},
        {"handleAbilityOnDestroy",                   NULL, OHOS_Napi_HandleAbilityOnDestroy, NULL, NULL, NULL, napi_default, NULL},
        {"addNode",                                  NULL, OHOS_Napi_AddNode, NULL, NULL, NULL, napi_default, NULL},
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

/* Application startup */

static char g_ohos_app_library[256] = "libmain.so";

void OHOS_Main_SetAppLibrary(const char *library)
{
    if (library && library[0]) {
        SDL_strlcpy(g_ohos_app_library, library, sizeof(g_ohos_app_library));
    }
}

static void *OHOS_StartMainThread(void *unused)
{
    static char prog[] = "app";
    char *argv[] = { prog, NULL };
    SDL_main_func p_main;
    void *handle;

    (void)unused;

    SDL_SetMainReady();

    handle = dlopen(g_ohos_app_library, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "OHOS_Main_StartApp: cannot load '%s'", g_ohos_app_library);
        return NULL;
    }

    p_main = (SDL_main_func)dlsym(handle, "SDL_main");
    if (!p_main) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "OHOS_Main_StartApp: '%s' does not export SDL_main", g_ohos_app_library);
        dlclose(handle);
        return NULL;
    }

    p_main(1, argv);
    dlclose(handle);
    return NULL;
}

void OHOS_Main_StartApp(void)
{
    static SDL_bool started = SDL_FALSE;
    pthread_t thread;
    int rc;

    if (started) {
        return;
    }
    started = SDL_TRUE;

    rc = pthread_create(&thread, NULL, OHOS_StartMainThread, NULL);
    if (rc != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OHOS_Main_StartApp: pthread_create=%d", rc);
        return;
    }
    pthread_detach(thread);
}

/* vi: set ts=4 sw=4 expandtab: */
