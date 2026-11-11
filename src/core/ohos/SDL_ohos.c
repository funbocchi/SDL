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

#include "SDL_system.h"
#include "SDL_main.h"
#include "SDL_mutex.h"

#define OHOS_ARGS_REQUIRED   1
#define OHOS_ARGS_MAX        4
#define OHOS_ARG_ABILITY     0
#define OHOS_ARG_APP_LIBRARY 1

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
    OHOS_WINDOW_CMD_SHOW
} OHOS_WindowCommand;

typedef struct OHOS_WindowRequest
{
    OHOS_WindowCommand          cmd;
    Uint32                      sdl_window_id;
    SDL_sem                    *sem;
    SDL_bool                    completed;
    napi_ref                   *window_ref;

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

static napi_value OHOS_Tool_PromiseThen(napi_env env, napi_value promise, napi_callback callback, void *data,const char *name)
{
    napi_value then_fn = NULL;
    napi_value handler = NULL;
    napi_value retval = NULL;

    napi_get_named_property(env, promise, "then", &then_fn);
    napi_create_function(env, name ? name : "then", NAPI_AUTO_LENGTH, callback, data, &handler);
    napi_call_function(env, promise, then_fn, 1, &handler, &retval);
    return retval;
}

/* JS thread-related processing interfaces */

static napi_ref                 g_ohos_ability_ref =        NULL;
static napi_ref                 g_ohos_window_stage_ref =   NULL;
static Uint32                   g_ohos_main_window_id =     0;

static napi_threadsafe_function g_ohos_window_request_tsf = NULL;

static napi_value activate_cb =  NULL;

static napi_value OHOS_JS_Window_OnWindowObtained(napi_env env, napi_callback_info info)
{

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
        napi_value argv[argc] = { NULL };

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

static void OHOS_JS_Window_OnDestroy(napi_env env)
{}

static napi_value OHOS_JS_Window_Create(napi_env env, napi_value promise, OHOS_WindowRequest *request)
{
    napi_value on_create_fn = NULL;

    napi_create_function(env, "onCreate", NAPI_AUTO_LENGTH, OHOS_JS_Window_OnCreate, request, &on_create_fn);
    if (g_ohos_main_window_id == 0) {
        g_ohos_main_window_id = request->sdl_window_id;
    }
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnCreate, request, NULL);
    promise = OHOS_Tool_PromiseThen(env, promise, OHOS_JS_Window_OnWindowObtained, request, NULL);
    return promise;
}

napi_value OHOS_JS_Window_Dispatch(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL;
    napi_value promise = NULL;
    napi_deferred deferred = NULL;
    napi_value finally_fn = NULL;
    napi_value dispatch_fn = NULL;
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
    request->window_ref = &queue->ohos_window_ref;
    napi_create_promise(env, &deferred, &promise);

    switch (request->cmd) {
        case OHOS_WINDOW_CMD_CREATE:
            promise = OHOS_JS_Window_Create(env, promise, request);
            break;
        case OHOS_WINDOW_CMD_DESTROY:
        case OHOS_WINDOW_CMD_SET_SIZE:
        case OHOS_WINDOW_CMD_SET_POS:
        case OHOS_WINDOW_CMD_SET_TITLE:
        case OHOS_WINDOW_CMD_SET_FULLSCREEN:
        case OHOS_WINDOW_CMD_SHOW:
            break;
    }
    napi_create_function(env, "dispatch", NAPI_AUTO_LENGTH, OHOS_JS_Window_Dispatch, queue, &dispatch_fn);
    napi_get_named_property(env, promise, "finally", &finally_fn);
    napi_call_function(env, promise, finally_fn, 1, &dispatch_fn, NULL);
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
    }
    if (request->cmd == OHOS_WINDOW_CMD_DESTROY) {
        OHOS_WindowEventQueue_Clear(queue);
        OHOS_WindowEventQueue_Unlink(queue);
    }
    OHOS_WindowEventQueue_Enqueue(queue, request);

    napi_create_external(env, queue, NULL, NULL, &queue_ext);

    napi_call_function(env, NULL, js_cb, 0, NULL, NULL);
}

static napi_value OHOS_JS_Window_Init(napi_env env, napi_value exprts)
{
    napi_value resource_name = NULL;

    if (activate_cb != NULL) {
        return NULL;
    }
    napi_create_function(env, "activate", NAPI_AUTO_LENGTH, OHOS_JS_Window_Activate, NULL, &activate_cb);
    napi_create_string_utf8(env, "ohosWindowEventTSF", NAPI_AUTO_LENGTH, &resource_name);
    napi_create_threadsafe_function(env, activate_cb, NULL, resource_name, 0, 1, NULL, NULL, NULL, OHOS_JS_Window_OnRequest, &g_ohos_window_request_tsf);
    return exprts;
}

/* SDL thread-related processing interfaces */

int OHOS_Window_Create(Uint32 sdl_window_id, int width, int height)
{
}

void OHOS_Window_Destroy(Uint32 sdl_window_id)
{
}

void OHOS_Window_SetPos(Uint32 sdl_window_id, int pos_x, int pos_y)
{
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
    OHOS_WindowEntry_Init(env);

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

    if (g_ohos_window_stage_ref) {
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