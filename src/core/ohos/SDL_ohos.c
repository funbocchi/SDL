
#include "SDL_internal.h"

#include <napi/native_api.h>

#include "SDL_stdinc.h"

#include "SDL_ohos.h"

static char *s_system_locale = NULL;

int OHOS_Napi_GetLocale(char *buf, size_t buflen)
{
    if (buflen == 0 || buf == NULL) {
        return -1;
    }
    buf[0] = '\0';

    if (s_system_locale == NULL || s_system_locale[0] == '\0') {
        return -1;
    }

    strncpy(buf, s_system_locale, buflen - 1);
    buf[buflen - 1] = '\0';
    return 0;
}

/**
 * @brief 该接口用于获取 Locale 信息并保存在 C 侧，同时导出给Napi在ArkTS侧监听变化
 * @see https://developer.huawei.com/consumer/cn/doc/doccenter-references/api/js-apis-i18n#getsystemlocaleinstance20
 */
static void CacheSystemLocaleInstance(napi_env env)
{
    napi_value i18n_module = NULL;
    napi_value system_obj = NULL;
    napi_value get_locale_func = NULL;
    napi_value locale_obj = NULL;
    napi_value base_name = NULL;
    size_t len = 0;
    char language[4] = {0};
    char country[3] = {0};
    char *raw_locale = NULL;
    char *token = NULL;

    // 加载 ohos.i18n 模块
    napi_status status = napi_load_module(env, "@ohos.i18n", &i18n_module);
    if (status != napi_ok || i18n_module == NULL) {
        return;
    }

    status = napi_get_named_property(env, i18n_module, "System", &system_obj);
    if (status != napi_ok || system_obj == NULL) {
        return;
    }

    status = napi_get_named_property(env, system_obj, "getSystemLocaleInstance", &get_locale_func);
    if (status != napi_ok || get_locale_func == NULL) {
        return;
    }

    status = napi_call_function(env, system_obj, get_locale_func, 0, NULL, &locale_obj);
    if (status != napi_ok || locale_obj == NULL) {
        return;
    }

    status = napi_get_named_property(env, locale_obj, "baseName", &base_name);
    if (status != napi_ok || base_name == NULL) {
        return;
    }
    napi_get_value_string_utf8(env, base_name, NULL, 0, &len);
    if (len == 0) {
        return;
    }

    raw_locale = (char *)malloc(len + 1);
    if (raw_locale == NULL) {
        return;
    }
    napi_get_value_string_utf8(env, base_name, raw_locale, len + 1, &len);

    token = strtok(raw_locale, "-");
    if (token) {
        strncpy(language, token, sizeof(language) - 1);

        token = strtok(NULL, "-");
        while (token) {
            if (strlen(token) == 4 && token[0] >= 'A' && token[0] <= 'Z') {
                token = strtok(NULL, "-");
                continue;
            }

            strncpy(country, token, sizeof(country) - 1);
            break;
        }
    }
    if (language[0]) {
        size_t final_lan = strlen(language);
        if (country[0]) {
            final_lan += 1 + strlen(country);
        }

        if (s_system_locale) {
            free(s_system_locale);
            s_system_locale = NULL;
        }

        s_system_locale = (char *)malloc(final_lan + 1);
        if (s_system_locale != NULL) {
            if (country[0]) {
                SDL_snprintf(s_system_locale, final_lan + 1, "%s_%s", language, country);
            } else {
                SDL_snprintf(s_system_locale, final_lan + 1, "%s", language);
            }
        }
    }
    free(raw_locale);
}

static napi_value OHOS_NAPI_RegisterNapiInterface(napi_env env, napi_value exports)
{
    /// Export the interpreter to the ArkTS side.
    napi_property_descriptor desc[] = {

    };

    /// Perhaps it would be better to add a callback for the registration result here.
    napi_define_properties(env, exports, SDL_arraysize(desc), desc);

    CacheSystemLocaleInstance(env);

    return exports;
}

__attribute__((constructor)) void OHOS_NAPI_RegisterModule(void)
{
    static napi_module ohos_napi_module = {
        .nm_version = 1,
        .nm_flags = 0,
        .nm_filename = NULL,
        .nm_register_func = OHOS_NAPI_RegisterNapiInterface,
        .nm_modname = "sdl2",
        .nm_priv = ((void *)0),
        .reserved = { 0 },
    };

    napi_module_register(&ohos_napi_module);
}