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

#ifndef SDL_OHOSVIDEO_H
#define SDL_OHOSVIDEO_H

#include <window_manager/oh_display_info.h>

#include "../SDL_sysvideo.h"

/* Private display data */
typedef struct
{
    int wake_fd;
    int wake_write;
    SDL_bool events_ready;
} SDL_VideoData;

typedef struct
{
    Uint32 id;

    int physical_width;
    int physical_height;

    Uint32 available_width;
    Uint32 available_height;

    float density_dpi;
    float density_pixels;

    float scaled_density;

    float x_dpi;
    float y_dpi;

    int x_offset;
    int y_offset;

    NativeDisplayManager_Rotation rotation;

    NativeDisplayManager_DisplayState state;

    NativeDisplayManager_Orientation orientation;
} SDL_DisplayData;

extern const char *OHOS_Tool_DisplayManagerErrorToString(NativeDisplayManager_ErrorCode code);

#endif // SDL_OHOSVIDEO_H

/* vi: set ts=4 sw=4 expandtab: */