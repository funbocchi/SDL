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

#ifdef SDL_VIDEO_DRIVER_OHOS

#include "SDL_error.h"
#include "../SDL_sysvideo.h"


#include "SDL_ohoswindow.h"
#include "SDL_ohosgles.h"
#include "SDL_ohosmessagebox.h"

#define OHOS_VID_DRIVER_NAME "ohos"

int OHOS_VideoInit(_THIS){}

void OHOS_VideoQuit(_THIS){}

static SDL_VideoDevice *OHOS_CreateDevice(void){
    SDL_VideoDevice *device;
    SDL_VideoData *data;
    SDL_bool block_on_pause;

    /* Initialize all variables that we clean on shutdown */
    device=(SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if(!device){
        SDL_OutOfMemory();
        return NULL;
    }

    data = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if(!data){
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

    device->driverdata = data;

    device->VideoInit = OHOS_VideoInit;
    device->VideoQuit = OHOS_VideoQuit;

    device->CreateSDLWindow = OHOS_CreateWindow;
    device->SetWindowTitle = OHOS_SetWindowTitle;
    device->SetWindowFullscreen = OHOS_SetWindowFullScreen;
    device->MinimizeWindow = OHOS_MinimizeWindow;
    device->SetWindowResizable = OHOS_SetWindowResizable;
    device->DestroyWindow = OHOS_DestroyWindow;
}

VideoBootStrap OHOS_bootstrap = {
    OHOS_VID_DRIVER_NAME, "SDL OHOS video driver",
    OHOS_CreateDevice,
    NULL
};