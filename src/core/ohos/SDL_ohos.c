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

#ifdef SDL_PLATFORM_OHOS

/// ArkUI 相关库
#include <ace/xcomponent/native_interface_xcomponent.h>

static napi_threadsafe_function CreateNapiThreadsafeFunction(napi_env env, const char *name, napi_threadsafe_function_call_js fn)
{
}

/// XComponent 组件相关
/**
 *@brief XComponent 组件相关回调
 */
static void OHOS_SurfaceHolder_OnCreated(OH_ArkUI_SurfaceHolder *holder)
{
    OHNativeWindow *window = OH_ArkUI_XComponent_GetNativeWindow(holder);
}

static void OHOS_SurfaceHolder_OnChanged(OH_ArkUI_SurfaceHolder *holder, uint64_t width, uint64_t height)
{
}

static void OHOS_SurfaceHolder_OnDestroyed(OH_ArkUI_SurfaceHolder *holder)
{
}

/**
 * @brief 接收并绑定相关回调
 */
static napi_value OHOS_NodeContent_Bind(napi_env env, napi_callback_info info)
{
}

/**
 * @brief 解除绑定
 */
static napi_value OHOS_NodeContent_Unbind(napi_env env, napi_callback_info info)
{
}

static napi_value OHOS_NAPI_RegisterInterface(napi_env env, napi_value exports)
{
    /// Export the interpreter to the ArkTS side.
    napi_property_descriptor desc[] = {

    };

    /// Perhaps it would be better to add a callback for the registration result here.
    napi_define_properties(env, exports, SDL_arraysize(desc), desc);

    CacheSystemLocaleInstance(env);

    return exports;
}

/**
 * @brief 在引入 libsdl2.so 时加载，注册 napi 接口
 */
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

#endif /// SDL_PLATFORM_OHOS