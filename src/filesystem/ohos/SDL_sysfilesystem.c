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

#ifdef SDL_FILESYSTEM_OHOS

#include <sys/stat.h>

#define ABILITY_RUNTIME_START_OPTIONS_H
typedef enum AbilityRuntime_StartOptions AbilityRuntime_StartOptions;

#include <AbilityKit/ability_runtime/application_context.h>

#include "SDL_error.h"
#include "SDL_filesystem.h"
#include "SDL_stdinc.h"

char *SDLCALL SDL_GetBasePath(void)
{
    char buffer[1024];
    int32_t writelen = 0;
    AbilityRuntime_ErrorCode rc;
    size_t len;

    rc = OH_AbilityRuntime_ApplicationContextGetBundleCodeDir(buffer, (int32_t)sizeof(buffer), &writelen);
    if (rc != ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
        SDL_SetError("OH_AbilityRuntime_ApplicationContextGetBundleCodeDir failed: %d", (int)rc);
        return NULL;
    }

    len = SDL_strlen(buffer);
    if (len > 0 && buffer[len - 1] != '/') {
        if (len + 1 < sizeof(buffer)) {
            buffer[len] = '/';
            buffer[len + 1] = '\0';
        }
    }
    return SDL_strdup(buffer);
}

char *SDLCALL SDL_GetPrefPath(const char *org, const char *app)
{
    char buffer[1024];
    int32_t writelen = 0;
    AbilityRuntime_ErrorCode rc;
    char *retval;

    rc = OH_AbilityRuntime_ApplicationContextGetFilesDir(buffer, (int32_t)sizeof(buffer), &writelen);
    if (rc != ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
        SDL_SetError("OH_AbilityRuntime_ApplicationContextGetFilesDir failed: %d", (int)rc);
        return NULL;
    }

    if (SDL_asprintf(&retval, "%s/%s/", buffer, app) < 0) {
        SDL_OutOfMemory();
        return NULL;
    }
    mkdir(buffer, 0755);
    mkdir(retval, 0755);

    return retval;
}

#endif