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

static napi_ref g_ohos_ability_ref =      NULL;
static napi_ref g_ohos_window_stage_ref = NULL;

static napi_threadsafe_function g_ohos_window_request_tsf = NULL;

typedef enum
{
    OHOS_WINDOW_CMD_CREATE,
    OHOS_WINDOW_CMD_DESTROY,
    OHOS_WINDOW_CMD_SET_SIZE,
    OHOS_WINDOW_CMD_SET_TITLE,
    OHOS_WINDOW_CMD_SET_FULLSCREEN,
    OHOS_WINDOW_CMD_SHOW
} OHOS_WindowCommand;

typedef struct OHOS_WindowRequest
{
    OHOS_WindowCommand cmd;
} OHOS_WindowRequest;

int OHOS_Window_Create(Uint32 sdl_window_id, int *width, int *height)
{
}

void OHOS_Window_Destroy(Uint32 sdl_window_id)
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