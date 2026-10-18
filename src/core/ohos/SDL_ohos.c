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

#ifdef __OHOS__

#include "SDL_mutex.h"
#include "SDL_ohos.h"
#include "SDL_stdinc.h"
#include "SDL_system.h"

#include <napi/native_api.h>

#include "../../video/ohos/SDL_ohoswindow.h"

static SDL_mutex *s_locale_mutex = NULL;
static char s_locale_lang[4] = { 0 };
static char s_locale_region[4] = { 0 };

static const char *Napi_StatusStr(napi_status status)
{
    switch (status) {
    case napi_invalid_arg:
        return "napi_invalid_arg";
    case napi_pending_exception:
        return "napi_pending_exception";
    case napi_generic_failure:
        return "napi_generic_failure";
    default:
        return "napi_other_error";
    }
}

napi_value Napi_CreateString(napi_env env, const char *data)
{
    napi_value result;
    napi_status status;
    if ((status = napi_create_string_utf8(env, data, NAPI_AUTO_LENGTH, &result)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CreateString: create string failed");
        return NULL;
    }
    return result;
}

napi_value Napi_CreateNumber(napi_env env, double data)
{
    napi_value result;
    napi_status status;
    if ((status = napi_create_double(env, data, &result)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CreateNumber: create number failed");
        return NULL;
    }
    return result;
}

napi_value Napi_CreateBoolean(napi_env env, SDL_bool data)
{
    napi_value result;
    napi_status status;
    if ((status = napi_get_boolean(env, (data == SDL_TRUE) ? true : false, &result)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CreateBoolean: create boolean failed");
        return NULL;
    }
    return result;
}

napi_value Napi_CreateObj(napi_env env)
{
    napi_value result;
    napi_status status;
    if ((status = napi_create_object(env, &result)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CreateObj: create object failed");
        return NULL;
    }
    return result;
}

napi_value Napi_CreateNull(napi_env env)
{
    napi_value result;
    napi_status status;
    if ((status = napi_get_null(env, &result)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CreateNull: create null failed");
        return NULL;
    }
    return result;
}

napi_value Napi_CreateUndefined(napi_env env)
{
    napi_value result;
    napi_status status;
    if ((status = napi_get_undefined(env, &result)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CreateUndefined: create undefined failed");
        return NULL;
    }
    return result;
}

SDL_bool Napi_SetNamedProperty(napi_env env, napi_value obj, const char *name, napi_value data)
{
    napi_status status;
    if (!data) {
        return SDL_FALSE;
    }
    if ((status = napi_set_named_property(env, obj, name, data)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_SetNamedProperty: set named property failed");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

SDL_bool Napi_CheckTSFCallbackArg(napi_env env, napi_callback_info info, napi_value *cb)
{
    size_t argc = 1;
    napi_valuetype type = napi_undefined;
    napi_status status;

    *cb = NULL;
    if ((status = napi_get_cb_info(env, info, &argc, cb, NULL, NULL)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_GetCallbackArg: get_cb_info failed");
        return SDL_FALSE;
    }
    if (*cb == NULL) {
        Napi_ThrowError(env, napi_invalid_arg, "Napi_GetCallbackArg: missing callback argument");
        return SDL_FALSE;
    }
    if ((status = napi_typeof(env, *cb, &type)) != napi_ok) {
        Napi_ThrowError(env, status, "Napi_GetCallbackArg: typeof failed");
        return SDL_FALSE;
    }
    if (type != napi_function) {
        Napi_ThrowError(env, napi_invalid_arg, "Napi_GetCallbackArg: argument is not a function");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static void Napi_FinalizeTSF(napi_env env, void *finalize_data, void *hint)
{
    napi_threadsafe_function *tsf = (napi_threadsafe_function *)finalize_data;
    (void)env;
    (void)hint;
    if (tsf != NULL) {
        *tsf = NULL;
    }
}

SDL_bool Napi_CreateTSF(napi_env env, napi_value cb, const char *name, napi_threadsafe_function_call_js js_cb, napi_threadsafe_function *tsf)
{
    napi_value resource_name = NULL;
    napi_status status;

    if (*tsf != NULL) {
        status = napi_release_threadsafe_function(*tsf, napi_tsfn_release);
        if (status != napi_ok) {
            Napi_ThrowError(env, status, "Napi_CreateTSF: release tsf failed");
            return SDL_FALSE;
        }
        *tsf = NULL;
    }

    resource_name = Napi_CreateString(env, name);
    if (resource_name == NULL) {
        return SDL_FALSE;
    }

    status = napi_create_threadsafe_function(env, cb, NULL, resource_name, 0, 1, tsf, Napi_FinalizeTSF, NULL, js_cb, tsf);
    if (status != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CreateTSF: create tsf failed");
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

SDL_bool Napi_CallFunction(napi_env env, napi_value func, size_t argc, const napi_value *argv)
{
    napi_value undefined = NULL;
    napi_status status;

    if (func == NULL || (argc > 0 && argv == NULL)) {
        Napi_ThrowError(env, napi_invalid_arg, "Napi_CallTSF: func or argv invalid");
        return SDL_FALSE;
    }

    undefined = Napi_CreateUndefined(env);
    if (undefined == NULL) {
        return SDL_FALSE;
    }

    status = napi_call_function(env, undefined, func, argc, argv, NULL);
    if (status != napi_ok) {
        Napi_ThrowError(env, status, "Napi_CallFunction: call js function failed");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

SDL_bool Napi_CallThreadSafeFunction(napi_threadsafe_function tsf, void *data)
{
    napi_status status;

    if (tsf == NULL) {
        SDL_SetError("Napi_CallThreadSafeFunction: tsf has not been initialized");
        return SDL_FALSE;
    }
    if (data == NULL) {
        SDL_SetError("Napi_CallThreadSafeFunction: data is NULL");
        return SDL_FALSE;
    }

    status = napi_call_threadsafe_function(tsf, data, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        SDL_SetError("Napi_CallThreadSafeFunction: call failed with (%s, status %d)", Napi_StatusStr(status), (Sint32)status);
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void Napi_ClearPending(napi_env env, const char *context)
{
    napi_value exc = NULL;
    napi_value msg_val = NULL;
    char msg[256] = "unknow js error";
    size_t len = 0;

    if (napi_get_and_clear_last_exception(env, &exc) != napi_ok || exc == NULL) {
        return;
    }

    if (napi_get_named_property(env, exc, "message", &msg_val) == napi_ok && msg_val != NULL) {
        napi_get_value_string_utf8(env, msg_val, msg, sizeof(msg), &len);
    }

    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "%s: %s", context, msg);
}

void Napi_ThrowError(napi_env env, napi_status status, const char *data)
{
    if (status == napi_pending_exception) {
        return;
    }
    napi_throw_error(env, Napi_StatusStr(status), data);
}

static napi_value OHOS_Napi_UpdateLocale(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL };
    char lang[4] = { 0 };
    char region[4] = { 0 };
    size_t len = 0;

    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (argc < 2) {
        return NULL;
    }

    napi_get_value_string_utf8(env, args[0], lang, sizeof(lang), &len);
    napi_get_value_string_utf8(env, args[1], region, sizeof(region), &len);

    if (!lang[0]) {
        return Napi_CreateUndefined(env);
    }
    if (!s_locale_mutex) {
        s_locale_mutex = SDL_CreateMutex();
    }

    SDL_LockMutex(s_locale_mutex);

    SDL_strlcpy(s_locale_lang, lang, sizeof(s_locale_lang));
    SDL_strlcpy(s_locale_region, region, sizeof(s_locale_region));

    SDL_UnlockMutex(s_locale_mutex);
    return Napi_CreateUndefined(env);
}

static napi_value OHOS_Napi_RegisterInterface(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "updateLocale", NULL, OHOS_Napi_UpdateLocale, NULL, NULL, NULL, napi_default, NULL },
        { "requestWindowReady", NULL, OHOS_ThreadSafe_RegisterCallCreateWindow, NULL, NULL, NULL, napi_default, NULL },
        { "bindNode", NULL, OHOS_NodeContent_BindNode, NULL, NULL, NULL, napi_default, NULL },
        { "unbindNode", NULL, OHOS_NodeContent_UnbindNode, NULL, NULL, NULL, napi_default, NULL },
    };

    napi_define_properties(env, exports, SDL_arraysize(desc), desc);
    return exports;
}

__attribute__((constructor)) void OHOS_Napi_RegisterModule(void)
{
    static napi_module ohos_napi_module = {
        .nm_version = 1,
        .nm_flags = 0,
        .nm_filename = NULL,
        .nm_register_func = OHOS_Napi_RegisterInterface,
        .nm_modname = "sdl2",
        .nm_priv = ((void *)0),
        .reserved = { 0 },
    };
    napi_module_register(&ohos_napi_module);
}

#endif

/* vi: set ts=4 sw=4 expandtab: */
