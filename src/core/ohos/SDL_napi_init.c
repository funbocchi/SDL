#include "SDL_napi_init.h"

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {

    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}