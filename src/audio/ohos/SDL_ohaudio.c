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

#ifdef SDL_AUDIO_DRIVER_OHOS

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_audio_c.h"
#include "SDL_audio.h"


static void ohaudio_OpenDevice(_THIS, const char *devname)
{
}

static void ohaudio_WaitDevice(_THIS)
{
}

static void ohaudio_PlayDevice(_THIS)
{
}

static Uint8 *ohaudio_GetDeviceBuf(_THIS)
{
}

static void ohaudio_CloseDevice(_THIS)
{
}

static int ohaudio_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
}

static SDL_bool ohaudio_Init(SDL_AudioDriverImpl *impl)
{
}

void ohaudio_ResumeDevices(void)
{
}

void ohaudio_PauseDevices(void)
{
}

#endif