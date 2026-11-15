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

#ifndef SDL_OHAUDIO_H
#define SDL_OHAUDIO_H

#include "../SDL_sysaudio.h"
#include <ohaudio/native_audiostream_base.h>

/* Hidden "this" pointer for the audio functions */
#define _THIS SDL_AudioDevice *this

/* One block per callback; the count is the playout depth. */
#define OHOS_AUDIO_BLOCK_COUNT 4

struct SDL_PrivateAudioData
{
    OH_AudioRenderer *renderer;
    OH_AudioCapturer *capturer;

    /* OHAudio pushes through a callback while SDL pulls, so a block ring
       bridges the two, one block per callback request. */
    Uint8   *blocks[OHOS_AUDIO_BLOCK_COUNT];
    int      block_size;
    int      write_index;
    int      read_index;

    SDL_sem *free_blocks;
    SDL_sem *filled_blocks;
} SDL_PrivateAudioData;

#endif

/* vi: set ts=4 sw=4 expandtab: */
