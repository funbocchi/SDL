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

static SDL_mutex *s_locale_mutex = NULL;
static char s_locale_lang[4] = { 0 };
static char s_locale_region[4] = { 0 };

/* Napi tools start */

/**
 * @brief 创建一个 UTF-8 编码的 napi 字符串值。
 *
 * @param env NAPI 环境句柄。
 * @param str 以 '\0' 结尾的 C 字符串。若为 NULL 则直接返回 NULL。
 * @return 成功返回 napi_value，失败或 str 为 NULL 时返回 NULL。
 */
static napi_value Napi_CreateString(napi_env env, const char *str)
{
    napi_value retval = NULL;
    if (!str) {
        return NULL;
    }
    if (napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/**
 * @brief 创建一个空的 napi 对象。
 *
 * @param env NAPI 环境句柄。
 * @return 成功返回 napi_value，失败返回 NULL。
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
 * @brief 创建一个指定长度的 napi 数组。
 *
 * @param env NAPI 环境句柄。
 * @param len 数组长度。
 * @return 成功返回 napi_value，失败返回 NULL。
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
 * @brief 创建一个 napi 布尔值。
 *
 * @param env NAPI 环境句柄。
 * @param b 布尔值，true 或 false。
 * @return 成功返回 napi_value，失败返回 NULL。
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
 * @brief 获取 napi 的 undefined 值。
 *
 * @param env NAPI 环境句柄。
 * @return 成功返回表示 undefined 的 napi_value，失败返回 NULL。
 */
static napi_value Napi_GetUndefined(napi_env env)
{
    napi_value retval = NULL;
    if (napi_get_undefined(env, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

/* Napi tool end */

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
static napi_value OHOS_NAPI_UpdateLocale(napi_env env, napi_callback_info info)
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
static napi_value OHOS_NAPI_RegisterInterface(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "updateLocale", NULL, OHOS_NAPI_UpdateLocale, NULL, NULL, NULL, napi_default, NULL },
    };

    napi_define_properties(env, exports, SDL_arraysize(desc), desc);
    return exports;
}

__attribute__((constructor)) void OHOS_NAPI_RegisterModule(void)
{
    static napi_module ohos_napi_module = {
        .nm_version = 1,
        .nm_flags = 0,
        .nm_filename = NULL,
        .nm_register_func = OHOS_NAPI_RegisterInterface,
        .nm_modname = "sdl2",
        .nm_priv = ((void *)0),
        .reserved = { 0 },
    };
    napi_module_register(&ohos_napi_module);
}

#endif

/* vi: set ts=4 sw=4 expandtab: */
