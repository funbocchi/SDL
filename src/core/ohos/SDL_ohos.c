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

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>

#include "../../video/ohos/SDL_ohoswindow.h"

static napi_threadsafe_function call_create_window_tsf = NULL;

static SDL_mutex *s_locale_mutex = NULL;
static char s_locale_lang[4] = { 0 };
static char s_locale_region[4] = { 0 };

/* Napi tools start */

/**
 * @brief Creates an napi string value, mapping a NULL input to JS null.
 *
 * A NULL @p str is a legal input here and yields the JavaScript null
 * value; only a creation failure returns NULL. Callers passing fields
 * that must never be null should validate @p str before calling.
 *
 * @param env The NAPI environment handle.
 * @param str A null-terminated C string, or NULL to produce JS null.
 * @return napi_value on success, NULL on failure.
 */
static napi_value Napi_CreateString(napi_env env, const char *str)
{
    napi_value retval = NULL;
    if (napi_create_string_utf8(env, str ? str : "", NAPI_AUTO_LENGTH, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/**
 * @brief Creates an napi number value.
 *
 * All C numeric types (int32, uint32, float, double) funnel through
 * the double conversion: integers within +/-2^53 are represented
 * exactly, which covers the full int32 and uint32 ranges. For int64
 * values beyond 2^53, use the bigint interfaces instead.
 *
 * @param env The NAPI environment handle.
 * @param v The numeric value.
 * @return napi_value on success, NULL on failure.
 */
static napi_value Napi_CreateNumber(napi_env env, double v)
{
    napi_value retval = NULL;
    if (napi_create_double(env, v, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/**
 * @brief Creates an empty napi object.
 *
 * @param env The NAPI environment handle.
 * @return napi_value on success, NULL on failure.
 */
static napi_value Napi_CreateObj(napi_env env)
{
    napi_value retval = NULL;
    if (napi_create_object(env, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/**
 * @brief Creates an napi array with the specified length.
 *
 * @param env The NAPI environment handle.
 * @param len The array length.
 * @return napi_value on success, NULL on failure.
 */
static napi_value Napi_CreateArray(napi_env env, size_t len)
{
    napi_value retval = NULL;
    if (napi_create_array_with_length(env, len, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/**
 * @brief Creates an napi boolean value.
 *
 * @param env The NAPI environment handle.
 * @param b Boolean value, true or false.
 * @return napi_value on success, NULL on failure.
 */
static napi_value Napi_CreateBoolean(napi_env env, bool b)
{
    napi_value retval = NULL;
    if (napi_get_boolean(env, b, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/**
 * @brief Gets the undefined value from napi.
 *
 * @param env The NAPI environment handle.
 * @return napi_value representing undefined on success, NULL on failure.
 */
static napi_value Napi_GetUndefined(napi_env env)
{
    napi_value retval = NULL;
    if (napi_get_undefined(env, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/**
 * @brief Sets a named property on an object.
 *
 * @param env The NAPI environment handle.
 * @param obj The target object.
 * @param name The property name.
 * @param val The property value; NULL (creation failure) fails the call.
 * @return true on success, false on failure.
 */
static SDL_bool Napi_SetNamedProp(napi_env env, napi_value obj, const char *name, napi_value val)
{
    return (val != NULL && napi_set_named_property(env, obj, name, val) == napi_ok) ? SDL_TRUE : SDL_FALSE;
}

/* Napi tool end */

/* ThreadSafe Tools */

static SDL_bool OHOS_CreateThreadsafeFunction(napi_env env, napi_callback_info info, const char *name, napi_threadsafe_function_call_js fn, napi_threadsafe_function *out)
{
    size_t argc = 1;
    napi_value js_cb = NULL;
    napi_valuetype type = napi_undefined;
    napi_value work_name = NULL;
    napi_status status;

    if ((napi_get_cb_info(env, info, &argc, &js_cb, NULL, NULL) != napi_ok) || argc < 1 || js_cb == NULL) {
        return SDL_FALSE;
    }

    if (napi_typeof(env, js_cb, &type) != napi_ok || type != napi_function) {
        return SDL_FALSE;
    }

    if (*out != NULL) {
        napi_release_threadsafe_function(*out, napi_tsfn_abort);
        *out = NULL;
    }
    work_name = Napi_CreateString(env, name);
    if (work_name == NULL) {
        return SDL_FALSE;
    }

    status = napi_create_threadsafe_function(env, js_cb, NULL, work_name, 0, 1, NULL, NULL, NULL, fn, out);
    if (status != napi_ok) {
        *out = NULL;
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

/* ThreadSafe Tools end */

/* ThreadSafe Methods */

static void TSF_CallCreateWindow(napi_env env, napi_value js_cb, void *context, void *data)
{
    OHOSWindowRequestParam *param = NULL;
    napi_value obj;
    napi_value undefined;

    param = (OHOSWindowRequestParam *)data;
    if (param == NULL) {
        return;
    }

    if (!(obj = Napi_CreateObj(env)) ||
        !(undefined = Napi_GetUndefined(env)) ||
        !(Napi_SetNamedProp(env, obj, "id", Napi_CreateNumber(env, param->id))) ||
        !(Napi_SetNamedProp(env, obj, "title", Napi_CreateString(env, param->title))) ||
        !(Napi_SetNamedProp(env, obj, "x", Napi_CreateNumber(env, param->x))) ||
        !(Napi_SetNamedProp(env, obj, "y", Napi_CreateNumber(env, param->y))) ||
        !(Napi_SetNamedProp(env, obj, "w", Napi_CreateNumber(env, param->w))) ||
        !(Napi_SetNamedProp(env, obj, "h", Napi_CreateNumber(env, param->h))) ||
        !(Napi_SetNamedProp(env, obj, "fullscreen", Napi_CreateBoolean(env, param->fullscreen))) ||
        !(Napi_SetNamedProp(env, obj, "borderless", Napi_CreateBoolean(env, param->borderless)))) {
        goto endfunc;
    }

    napi_call_function(env, undefined, js_cb, 1, &obj, NULL);

endfunc:
    SDL_free(param);
    return;
}

static napi_value OHOS_ThreadSafe_RequestWindowReady(napi_env env, napi_callback_info info)
{
    if (!OHOS_CreateThreadsafeFunction(env, info, "R", TSF_CallCreateWindow, &call_create_window_tsf)) {
        return Napi_GetUndefined(env);
    }
    return Napi_GetUndefined(env);
}

/* ThreadSafe Methods end */

/* Public func */

/**
 * @brief Reads cached locale data and writes it to buf as "lang_country" format.
 *
 * Called by SDL_SYS_GetPreferredLocales in SDL_syslocale.c.
 * Locks internally to ensure thread safety.
 *
 * @param buf Output buffer for the locale string.
 * @param buflen Size of buf.
 * @return 0 on success, -1 on failure.
 */
int SDL_OHOS_GetLocale(char *buf, size_t buflen)
{
    if (!buf || buflen == 0) {
        return -1;
    }

    if (!s_locale_mutex) {
        /* To avoid the situation of repeatedly creating mutex objects, this function directly returns here. */
        return -1;
    }

    SDL_LockMutex(s_locale_mutex);

    if (!s_locale_lang[0]) {
        SDL_UnlockMutex(s_locale_mutex);
        return -1;
    }

    if (s_locale_region[0]) {
        SDL_snprintf(buf, buflen, "%s_%s", s_locale_lang, s_locale_region);
    } else {
        SDL_strlcpy(buf, s_locale_lang, buflen);
    }

    SDL_UnlockMutex(s_locale_mutex);

    return 0;
}

int SDL_OHOS_RequestWindow(OHOSWindowRequestParam *params)
{
    int retval = 0;
    if (params == NULL || call_create_window_tsf == NULL) {
        retval = SDL_SetError("The parameter is invalid or the tsf has not been initialized.");
        goto endfunc;
    }

    if (napi_call_threadsafe_function(call_create_window_tsf, params, napi_tsfn_nonblocking) != napi_ok) {
        retval = SDL_SetError("The call to create a window using call_create_window_tsf failed");
    }

endfunc:
    return retval;
}

/* Public func end */

/**
 * @brief NAPI callback: receives and caches the language and region codes from the ArkTS side.
 *
 * Called by the ArkTS side via nativeModule.updateLocale(language, region).
 * Locks internally to write s_locale_lang and s_locale_region, ensuring thread safety.
 *
 * @param env The NAPI environment handle.
 * @param info The callback info, containing the argument list.
 * @return undefined on success, or NULL if arguments are insufficient or language is empty.
 *
 * @param args[0] The language code string, e.g. "zh", "en".
 * @param args[1] The region code string, e.g. "CN", "US". Can be an empty string.
 */
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
        return Napi_GetUndefined(env);
    }
    if (!s_locale_mutex) {
        s_locale_mutex = SDL_CreateMutex();
    }

    SDL_LockMutex(s_locale_mutex);

    SDL_strlcpy(s_locale_lang, lang, sizeof(s_locale_lang));
    SDL_strlcpy(s_locale_region, region, sizeof(s_locale_region));

    SDL_UnlockMutex(s_locale_mutex);
    return Napi_GetUndefined(env);
}

/**
 * @brief Registers NAPI interfaces and attaches them to the exports object.
 *
 * This function is called during module initialization to expose
 * native functions to the ArkTS side.
 *
 * @param env The NAPI environment handle.
 * @param exports The exports object to attach properties to.
 * @return The exports object with registered properties.
 */
static napi_value OHOS_Napi_RegisterInterface(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "updateLocale", NULL, OHOS_Napi_UpdateLocale, NULL, NULL, NULL, napi_default, NULL },
        { "requestWindowReady", NULL, OHOS_ThreadSafe_RequestWindowReady, NULL, NULL, NULL, napi_default, NULL },
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
