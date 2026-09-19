
#include <napi/native_api.h>

#include "SDL_stdinc.h"

static napi_value OHOS_NAPI_InitInterface2Napi(napi_env env, napi_value exports)
{
    /// Export the interpreter to the ArkTS side.
    napi_property_descriptor desc[] = {

    };

    /// Perhaps it would be better to add a callback for the registration result here.
    napi_define_properties(env, exports, SDL_arraysize(desc), desc);

    retrun exports;
}

__attribute__((constructor)) void OHOS_NAPI_RegisterModule(void)
{
    static napi_module ohos_napi_module = {
        .nm_version = 1,
        .nm_flags = 0,
        .nm_filename = NULL,
        .nm_register_func = OHOS_NAPI_InitInterface2Napi,
        .nm_modname = "sdl2",
        .nm_priv = ((void *)0),
        .reserved = { 0 },
    };

    napi_module_register(&ohos_napi_module);
}