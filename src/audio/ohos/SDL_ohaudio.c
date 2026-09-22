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

#include <ohaudio/native_audiocapturer.h>
#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostream_base.h>

#if 0
#else
//! 后续使用hilog库对该宏进行实现
#define LOGE(...)
#define LOGI(...)
#define LOGV(...)
#endif

static OH_AudioStream_SampleFormat SDLToOHOSAudioFormat(SDL_AudioFormat format)
{
    switch (format) {
    case AUDIO_U8:
        return AUDIOSTREAM_SAMPLE_FORMAT_U8;
    case AUDIO_S16LSB:
        return AUDIOSTREAM_SAMPLE_FORMAT_S16LE;
    case AUDIO_S16MSB:
        return AUDIOSTREAM_SAMPLE_FORMAT_S16BE;
    case AUDIO_S32LSB:
        return AUDIOSTREAM_SAMPLE_FORMAT_S32LE;
    case AUDIO_S32MSB:
        return AUDIOSTREAM_SAMPLE_FORMAT_S32BE;
    case AUDIO_F32LSB:
        return AUDIOSTREAM_SAMPLE_FORMAT_F32LE;
    case AUDIO_F32MSB:
        return AUDIOSTREAM_SAMPLE_FORMAT_F32BE;
    default:
        return AUDIOSTREAM_SAMPLE_FORMAT_S16LE;
    }
}

static void OHAudio_ThreadInit(_THIS)
{
    SDL_SetThreadPriority(this->iscapture ? SDL_THREAD_PRIORITY_HIGH : SDL_THREAD_PRIORITY_TIME_CRITICAL);

    if (this->iscapture) {
        OH_AudioCapturer_Start(this->hidden->stream.capturer);
    } else {
        OH_AudioRenderer_Start(this->hidden->stream.renderer);
    }
}

static int OHAudio_DestroyCapturer(_THIS)
{
    if (this->hidden->capturer) {
        OH_AudioCapturer_Stop(this->hidden->capturer);
        OH_AudioCapturer_Release(this->hidden->capturer);
        this->hidden->capturer = NULL;
    }

    if (this->hidden->builder) {
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        this->hidden->builder = NULL;
    }

    if (this->hidden->semaphore) {
        SDL_DestroySemaphore(this->hidden->semaphore);
        this->hidden->semaphore = NULL;
    }

    if (this->hidden->mixbuff) {
        SDL_free(this->hidden->mixbuff);
        this->hidden->mixbuff = NULL;
    }
}

static int OHAudio_CreateCapturer(_THIS)
{
    OH_AudioStream_Result result;
    OH_AudioStream_SampleFormat fmt;

    result = OH_AudioStreamBuilder_Create(&this->hidden->builder,
                                          AUDIOSTREAM_TYPE_CAPTURER);
    if (result != AUDIOSTREAM_SUCCESS) {
        return SDL_SetError("Create capturer builder failed: %d", (int)result);
    }

    OH_AudioStreamBuilder_SetSamplingRate(this->hidden->builder, this->spec.freq);
    OH_AudioStreamBuilder_SetChannelCount(this->hidden->builder, this->spec.channels);
    fmt = SDLToOHOSAudioFormat(this->spec.format);
    OH_AudioStreamBuilder_SetSampleFormat(this->hidden->builder, fmt);

    OH_AudioStreamBuilder_SetCapturerInfo(this->hidden->builder,
                                          AUDIOSTREAM_CAPTURE_INPUT_SOURCE_MIC);
    OH_AudioStreamBuilder_SetCapturerReadDataCallback(
        this->hidden->builder, OHOSAudioReadDataCallback, this);
    OH_AudioStreamBuilder_SetStreamErrorCallback(
        this->hidden->builder, OHOSAudioErrorCallbackCapturer, this);

    this->hidden->semaphore = SDL_CreateSemaphore(0);
    if (!this->hidden->semaphore) {
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        return SDL_SetError("CreateSemaphore failed");
    }

    this->hidden->num_buffers = 4;
    this->hidden->mixbuf_size = (size_t)this->hidden->num_buffers * this->spec.size;
    this->hidden->mixbuff = (Uint8 *)SDL_malloc(this->hidden->mixbuf_size);
    if (!this->hidden->mixbuff) {
        SDL_DestroySemaphore(this->hidden->semaphore);
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        return SDL_OutOfMemory();
    }
    this->hidden->callback_bytes = 0;
    this->hidden->processed_bytes = 0;
    SDL_AtomicSet(&this->hidden->error_callback_triggered, 0);

    result = OH_AudioStreamBuilder_GenerateCapturer(this->hidden->builder,
                                                    &this->hidden->capturer);
    if (result != AUDIOSTREAM_SUCCESS) {
        SDL_free(this->hidden->mixbuff);
        SDL_DestroySemaphore(this->hidden->semaphore);
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        return SDL_SetError("GenerateCapturer failed: %d", (int)result);
    }

    return 0;
}

static int OHAudio_DestroyRenderer(_THIS)
{
    if (this->hidden->renderer) {
        OH_AudioRenderer_Stop(this->hidden->renderer);
        OH_AudioRenderer_Release(this->hidden->renderer);
        this->hidden->renderer = NULL;
    }

    if (this->hidden->builder) {
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        this->hidden->builder = NULL;
    }

    if (this->hidden->semaphore) {
        SDL_DestroySemaphore(this->hidden->semaphore);
        this->hidden->semaphore = NULL;
    }

    if (this->hidden->mixbuff) {
        SDL_free(this->hidden->mixbuff);
        this->hidden->mixbuff = NULL;
    }
}

static int OHAudio_CreateRenderer(_THIS)
{
    OH_AudioStream_Result result;
    OH_AudioStream_SampleFormat fmt;

    result = OH_AudioStreamBuilder_Create(&this->hidden->builder, AUDIOSTREAM_TYPE_RENDERER);

    if (result != AUDIOSTREAM_SUCCESS) {
        return SDL_SetError("Create renderer builder failed: %d", (int)result);
    }

    OH_AudioStreamBuilder_SetSamplingRate(this->hidden->builder, this->spec.freq);
    OH_AudioStreamBuilder_SetChannelCount(this->hidden->builder, this->spec.channels);
    fmt = SDLToOHOSAudioFormat(this->spec.format);
    OH_AudioStreamBuilder_SetSampleFormat(this->hidden->builder, fmt);

    OH_AudioStreamBuilder_SetRendererInfo(this->hidden->builder, AUDIOSTREAM_USAGE_GAME);
    OH_AudioStreamBuilder_SetRendererWriteDataCallback(this->hidden->builder, OHOSAudioWriteDataCallback, this);
    OH_AudioStreamBuilder_SetStreamErrorCallback(this->hidden->builder, OHOSAudioErrorCallbackRenderer, this);

    this->hidden->semaphore = SDL_CreateSemaphore(0);
    if (!this->hidden->semaphore) {
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        return SDL_SetError("CreateSemaphore failed");
    }

    this->hidden->num_buffers = 4;
    this->hidden->mixbuf_size = (size_t)this->hidden->num_buffers * this->spec.size;
    this->hidden->mixbuff = (Uint8 *)SDL_malloc(this->hidden->mixbuf_size);
    if (!this->hidden->mixbuff) {
        SDL_DestroySemaphore(this->hidden->semaphore);
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        return SDL_OutOfMemory();
    }
    this->hidden->callback_bytes = 0;
    this->hidden->processed_bytes = 0;
    SDL_AtomicSet(&this->hidden->error_callback_triggered, 0);

    result = OH_AudioStreamBuilder_GenerateRenderer(this->hidden->builder, &this->hidden->renderer);
    if (result != AUDIOSTREAM_SUCCESS) {
        SDL_free(this->hidden->mixbuff);
        SDL_DestroySemaphore(this->hidden->semaphore);
        OH_AudioStreamBuilder_Destroy(this->hidden->builder);
        return SDL_SetError("GenerateRenderer failed: %d", (int)result);
    }

    return 0;
}

static int OHAudio_OpenDevice(_THIS, const char *devname)
{
    this->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*this->hidden));
    if (!this->hidden) {
        return SDL_OutOfMemory();
    }

    if (this->iscapture && false) {
        LOGI("OHAudio_OpenDevice() %s for capturer", devname);
        return OHAudio_CreateCapturer(this);
    } else {
        LOGI("OHAudio_OpenDevice() %s for renderer", devname);
        return OHAudio_CreateRenderer(this);
    }
}

static void OHAudio_WaitDevice(_THIS)
{
    while (!SDL_AtomicGet(&this->shutdown)) {
        if (SDL_SemWaitTimeout(&this->hidden->semaphore, 100) == 0) {
            if (SDL_AtomicGet(&this->hidden->error_callback_triggered)) {
                SDL_OpenedAudioDeviceDisconnected(this);
            }
            return;
        }
    }
}

static void OHAudio_PlayDevice(_THIS)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;
    OH_AudioStream_Result err;

    err = (OH_AudioStream_Result)SDL_AtomicGet(&hidden->error_callback_triggered);
    if (err) {
        SDL_LogError(SDL_LOG_GATEGORY_AUDIO,
                     "OHAudio: Audio device triggered error %d", (int)err);
        SDL_OpenedAudioDeviceDisconnected(this);
        return;
    }
    SDL_MemoryBarrierRelease();
    hidden->processed_bytes += this->spec.size;
}

static Uint8 *OHAudio_GetDeviceBuf(_THIS)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;
    size_t offset = hidden->processed_bytes % hidden->mixbuf_size;
    return hidden->mixbuff + offset;
}

static void OHAudio_CloseDevice(_THIS)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;

    if (hidden) {
        if (this->iscapture) {
            OHAudio_DestroyCapturer(this);
        } else {
            OHAudio_DestroyRenderer
        }

        if (hidden->semaphore) {
            SDL_DestroySemaphore(hidden->semaphore);
            hidden->semaphore = NULL;
        }

        if (hidden->mixbuff) {
            SDL_free(hidden->mixbuff);
            hidden->mixbuff = NULL;
        }

        SDL_free(hidden);
        this->hidden = NULL;
    }
}

static void OHAudio_Deinitialize(_THIS)
{
    LOGI("OHAudio_Deinitialize()");
}

static int OHAudio_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;
    const size_t offset = hidden->processed_bytes % hidden->mixbuf_size;
    const size_t end = (offset + buflen) % hidden->mixbuf_size;

    SDL_MemoryBarrierAcquire();

    if (offset <= end) {
        SDL_memcpy(buffer, hidden->mixbuf + offset, buflen);
    } else {
        size_t partial = hidden->mixbuf_size - offset;
        SDL_memcpy(buffer, hidden->mixbuff + offset, partial);
        SDL_memcpy((Uint8 *)buffer + partial, hidden->mixbuff, end);
    }

    hidden->processed_bytes += buflen;

    return buflen;
}

static SDL_bool OHAudio_Init(SDL_AudioDriverImpl *impl)
{
    LOGI("OHAudio_Init() called");

    LOGI("OHAudio_Init() - set pointers");

    /* Set the function pointers */
    impl->ThreadInit = OHAudio_ThreadInit;
    impl->OpenDevice = OHAudio_OpenDevice;
    impl->WaitDevice = OHAudio_WaitDevice;
    impl->PlayDevice = OHAudio_PlayDevice;
    impl->GetDeviceBuf = OHAudio_GetDeviceBuf;
    impl->CaptureFromDevice = OHAudio_CaptureFromDevice;
    impl->CloseDevice = OHAudio_CloseDevice;
    impl->Deinitialize = OHAudio_Deinitialize;

    /* and the capabilities */
    impl->HasCaptureSupport = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_TRUE;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;

    LOGI("OHAudio_Init() - success");

    /* this audio target is available. */
    return SDL_TRUE;
}

AudioBootStrap OHAudio_bootstrap = {
    "OHAudio", "OHOS OHAudio audio driver", OHAudio_Init, SDL_FALSE
};

void OHAudio_ResumeDevices(void)
{
}

void OHAudio_PauseDevices(void)
{
}

#endif