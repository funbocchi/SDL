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

#include <ohaudio/native_audiostream_base.h>

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_audio_c.h"
#include "SDL_audio.h"
#include "SDL_ohaudio.h"

static OH_AudioStream_SampleFormat SDL2OHAudioFormat(SDL_AudioFormat format)
{
    switch (format) {
    case AUDIO_U8:
        return AUDIOSTREAM_SAMPLE_U8;
    case AUDIO_S16LSB:
        return AUDIOSTREAM_SAMPLE_S16LE;
    case AUDIO_S32LSB:
        return AUDIOSTREAM_SAMPLE_S32LE;
    case AUDIO_F32LSB:
        return AUDIOSTREAM_SAMPLE_F32LE;
    default:
        return AUDIOSTREAM_SAMPLE_S16LE;
    }
}

static SDL_AudioFormat OHAudioFormat2SDL(OH_AudioStream_SampleFormat format)
{
    switch (format) {
    case AUDIOSTREAM_SAMPLE_U8:
        return AUDIO_U8;
    case AUDIOSTREAM_SAMPLE_S16LE:
        return AUDIO_S16LSB;
    case AUDIOSTREAM_SAMPLE_S32LE:
        return AUDIO_S32LSB;
    case AUDIOSTREAM_SAMPLE_F32LE:
        return AUDIO_F32LSB;
    default:
        return AUDIOSTREAM_SAMPLE_S16LE;
    }
}

static void OHAudio_DetectDevices(void)
{
}

static int OHAudio_OpenDevice(_THIS, const char *devname)
{
}

static void OHAudio_ThreadInit(_THIS)
{
}

static void OHAudio_ThreadDeInit(_THIS)
{
}

static void OHAudio_WaitDevice(_THIS)
{
}

static void OHAudio_PlayDevice(_THIS)
{
}

static Uint8 OHAudio_GetDeviceBuf(_THIS)
{
}

static int OHAudio_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
}

static void OHAudio_FlushCapture(_THIS)
{
}

static void OHAudio_CloseDevice(_THIS)
{
}

static void OHAudio_LockDevice(_THIS)
{
}

static void OHAudio_UnlockDevice(_THIS)
{
}

static void OHAudio_FreeDeviceHandle(void *handle)
{
}

static void OHAudio_Deinitialize(void)
{
}

static int OHAudio_GetDefaultAudioInfo(char **name, SDL_AudioSpec *spec, int iscapture)
{
}

static SDL_bool OHAudio_Init(SDL_AudioDriverImpl *impl)
{
    impl->DetectDevices = OHAudio_DetectDevices;
    impl->OpenDevice = OHAudio_OpenDevice;
    impl->ThreadInit = OHAudio_ThreadDeInit;
    impl->ThreadDeinit = OHAudio_ThreadDeInit;
    impl->WaitDevice = OHAudio_WaitDevice;
    impl->PlayDevice = OHAudio_PlayDevice;
    impl->GetDeviceBuf = OHAudio_GetDeviceBuf;
    impl->CaptureFromDevice = OHAudio_CaptureFromDevice;
    impl->FlushCapture = OHAudio_FlushCapture;
    impl->CloseDevice = OHAudio_CloseDevice;
    impl->LockDevice = OHAudio_LockDevice;
    impl->UnlockDevice = OHAudio_UnlockDevice;
    impl->FreeDeviceHandle = OHAudio_FreeDeviceHandle;
    impl->Deinitialize = OHAudio_Deinitialize;
    impl->GetDefaultAudioInfo = OHAudio_GetDefaultAudioInfo;

    return SDL_TRUE;
}

AudioBootStrap OHAudio_bootstrap = {
    "OHAudio", "OHOS OHAudio audio driver", OHAudio_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
