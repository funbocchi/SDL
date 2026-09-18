

#include "napi/native_api.h"

static napi_value Init(napi_env env, napi_value exports);

static napi_module ohosSDL2Module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = Init,
    .nm_modname = "laptop",
    .nm_priv = ((void *)0),
    .reserved = { 0 },
};

__attribute__((constructor)) void RegisterOHOSModule(void)
{
    napi_module_register(&ohosSDL2Module);
}