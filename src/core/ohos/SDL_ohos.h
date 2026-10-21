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

#ifndef SDL2_OHOS_SDL_OHOS_H
#define SDL2_OHOS_SDL_OHOS_H

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#include <hilog/log.h>
#include <napi/native_api.h>
#include <native_window/external_window.h>

// C -> JS

extern napi_value Napi_CreateString(napi_env env, const char *data);

extern napi_value Napi_CreateNumber(napi_env env, double data);

extern napi_value Napi_CreateBoolean(napi_env env, SDL_bool data);

extern napi_value Napi_CreateObj(napi_env env);

extern napi_value Napi_CreateNull(napi_env env);

extern napi_value Napi_CreateUndefined(napi_env env);

extern int Napi_SetNamedProperty(napi_env env, napi_value obj, const char *name, napi_value data);

extern int Napi_CheckTSFCallbackArg(napi_env env, napi_callback_info info, napi_value *cb);

extern int Napi_CreateTSF(napi_env env, napi_value cb, const char *name, napi_threadsafe_function_call_js js_cb, napi_threadsafe_function *tsf);

extern int Napi_CallFunction(napi_env env, napi_value func, size_t argc, const napi_value *argv);

extern int Napi_CallThreadSafeFunction(napi_threadsafe_function tsf, void *data);

extern void Napi_ClearPending(napi_env env, const char *context);

extern void Napi_ThrowError(napi_env env, napi_status status, const char *data);

// JS -> C

extern int Napi_GetUint32(napi_env env, napi_value value, Uint32 *result);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif // SDL2_OHOS_SDL_OHOS_H
