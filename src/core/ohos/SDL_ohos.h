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
#ifndef SDL_OHOS_H
#define SDL_OHOS_H

/**
 * @brief 允许创建的最大窗口数
 */
#define MAX_WINDOWS 8

typedef struct
{
    char id[128];
    int used;

    OH_ArkUI_SurfaceHolder *holder;
    OH_ArkUI_SurfaceCallback *callback;
    OHNativeWindow *native_window;
    int surface_ready;

    void *window_data;
    int window_id;

    SDL_mutex *mutex;
    SDL_cond *cond;
} OHOS_SurfaceEntry;

extern OHOS_SurfaceEntry g_surface_table[MAX_WINDOWS];

int OHOS_Napi_GetLocale(char *buf, size_t buflen);

/**
* @brief 关鼠标痕迹显示
*/
extern SDL_OHOS_HideMousePointer(void);
/**
* @brief 开鼠标痕迹显示
*/
extern SDL_OHOS_ShowMousePointer(void);

#endif