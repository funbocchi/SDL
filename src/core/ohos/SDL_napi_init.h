

#include "napi/native_api.h"

static napi_value SDL_NapiInit(napi_env env, napi_value exports);

static napi_module ohosSDL2Module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = SDL_NapiInit,
    .nm_modname = "SDL2",
    .nm_priv = ((void *)0),
    .reserved = { 0 },
};

__attribute__((constructor)) void RegisterOHOSModule(void)
{
    napi_module_register(&ohosSDL2Module);
}