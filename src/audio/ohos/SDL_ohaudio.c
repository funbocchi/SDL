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

#include <ohaudio/native_audio_manager.h>
#include <ohaudio/native_audio_routing_manager.h>
#include <ohaudio/native_audiostream_base.h>
#include <ohaudio/native_audiostreambuilder.h>
#include <accesstoken/ability_access_control.h>

#include "../../core/ohos/SDL_ohos.h"
#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_ohaudio.h"
#include "SDL_stdinc.h"

/**
 * @brief Enumerate devices by flag and register each with SDL2
 *
 * Queries OHAudio for a list of audio devices matching the given flag
 * (output or input), extracts the device ID and display name from each
 * descriptor, and registers the device with the SDL2 audio framework.
 *
 * The device ID (uint32_t) is cast to void* and used as the SDL2 device
 * handle. This handle will be passed back to OpenDevice when the
 * application opens this specific device.
 *
 * @param routing_manager OHAudio routing manager instance
 * @param flag           Device filter flag (OUTPUT or INPUT)
 * @param iscapture      SDL_FALSE for playback devices, SDL_TRUE for capture
 */
static void OHAudio_EnumerateDevicesByFlag(OH_AudioRoutingManager *routing_manager, OH_AudioDevice_Flag flag, SDL_bool iscapture)
{
    OH_AudioDeviceDescriptorArray *array = NULL;
    OH_AudioDeviceDescriptor *desc = NULL;
    char *name = NULL;
    Uint32 id = 0;
    OH_AudioCommon_Result res;
    Uint32 i;

    res = OH_AudioRoutingManager_GetDevices(routing_manager, flag, &array);
    if (res != AUDIOCOMMON_RESULT_SUCCESS || array == NULL) {
        return;
    }

    for (i = 0; i < array->size; i++) {
        desc = array->descriptors[i];
        if (desc == NULL) {
            continue;
        }
        name = NULL;
        id = 0;

        res = OH_AudioDeviceDescriptor_GetDeviceName(desc, &name);
        res = OH_AudioDeviceDescriptor_GetDeviceId(desc, &id);

        name = (name != NULL) ? name : "Unknown Audio Device";

        SDL_AddAudioDevice(iscapture, name, NULL, (void *)(uintptr_t)id);
    }
    OH_AudioRoutingManager_ReleaseDevices(routing_manager, array);
}

/**
 * @brief Convert SDL2 audio sample format to OHAudio sample format
 *
 * Maps the SDL_AudioFormat requested by the application to the closest
 * supported OHAudio sample format during SDL2 audio backend device opening.
 * The actual format must be written back to device->spec.format so that
 * the SDL2 framework can automatically create an AudioCVT converter to
 * handle any remaining differences.
 *
 * Mapping:
 * - AUDIO_U8     -> AUDIOSTREAM_SAMPLE_U8
 * - AUDIO_S16LSB -> AUDIOSTREAM_SAMPLE_S16LE
 * - AUDIO_S32LSB -> AUDIOSTREAM_SAMPLE_S32LE
 * - AUDIO_F32LSB -> AUDIOSTREAM_SAMPLE_F32LE
 * - Others       -> AUDIOSTREAM_SAMPLE_S16LE (fallback)
 *
 * Notes:
 * - Formats without a direct OHAudio counterpart (e.g. AUDIO_S8, big-endian
 *   variants) fall through to default and are downgraded to S16LE. The SDL2
 *   framework's AudioCVT handles the conversion automatically.
 * - AUDIOSTREAM_SAMPLE_S24LE is supported by OHAudio but has no corresponding
 *   SDL2 input source, so it will not appear as input to this function.
 *
 * @param format SDL2 audio sample format
 * @return Corresponding OHAudio sample format
 */
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

/**
 * @brief Convert OHAudio sample format to SDL2 audio sample format
 *
 * Maps the OHAudio sample format actually used by the audio stream back to
 * an SDL_AudioFormat so it can be written to device->spec.format. This
 * allows the SDL2 framework to compare the requested format against the
 * actual format and create an AudioCVT converter if needed.
 *
 * Mapping:
 * - AUDIOSTREAM_SAMPLE_U8    -> AUDIO_U8
 * - AUDIOSTREAM_SAMPLE_S16LE -> AUDIO_S16LSB
 * - AUDIOSTREAM_SAMPLE_S24LE -> AUDIO_S16LSB (downgrade, no SDL2 equivalent)
 * - AUDIOSTREAM_SAMPLE_S32LE -> AUDIO_S32LSB
 * - AUDIOSTREAM_SAMPLE_F32LE -> AUDIO_F32LSB
 * - Others                   -> AUDIO_S16LSB (fallback)
 *
 * Notes:
 * - AUDIOSTREAM_SAMPLE_S24LE has no equivalent in SDL2 and is downgraded
 *   to AUDIO_S16LSB.
 * - The return value should be written back to device->spec.format for
 *   the SDL2 framework to perform format comparison.
 *
 * @param format OHAudio sample format
 * @return Corresponding SDL2 audio sample format
 */
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
    case AUDIOSTREAM_SAMPLE_S24LE:
    default:
        return AUDIO_S16LSB;
    }
}

/**
 * @brief Enumerate available audio devices and register them with SDL2
 *
 * Called by the SDL2 audio framework during driver initialization to
 * discover all available audio devices in the system. Queries OHAudio
 * RoutingManager for both output (playback) and input (capture) devices,
 * then registers each device via SDL_AddAudioDevice.
 *
 * This function obtains AudioManager and RoutingManager on demand and
 * does not retain them — they are not needed for stream creation in
 * OpenDevice, which uses StreamBuilder directly.
 *
 * After this function returns, SDL2 maintains an internal device list
 * that the application can query via:
 *   - SDL_GetNumAudioDevices(iscapture)
 *   - SDL_GetAudioDeviceName(index, iscapture)
 */
static void OHAudio_DetectDevices(void)
{
    OH_AudioManager *audio_manager;
    OH_AudioRoutingManager *routing_manager = NULL;
    OH_AudioCommon_Result res;

    res = OH_GetAudioManager(&audio_manager);
    if (audio_manager == NULL) {
        return;
    }

    res = OH_AudioManager_GetAudioRoutingManager(&routing_manager);
    if (res != AUDIOCOMMON_RESULT_SUCCESS || routing_manager == NULL) {
        return;
    }
    // Enumerate output (playback) devices
    OHAudio_EnumerateDevicesByFlag(routing_manager, AUDIO_DEVICE_FLAG_OUTPUT, SDL_FALSE);
    // Enumerate input (capture) devices
    OHAudio_EnumerateDevicesByFlag(routing_manager, AUDIO_DEVICE_FLAG_INPUT, SDL_TRUE);
}

/* OHAudio calls this on its own audio thread, once per block it needs. */
static OH_AudioData_Callback_Result OHAudio_OnWriteData(OH_AudioRenderer *renderer, void *userData,
                                                        void *audioData, int32_t audioDataSize)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *)userData;
    struct SDL_PrivateAudioData *hidden;
    static int ohos_cb_total = 0;
    static int ohos_cb_underruns = 0;
    int size;

    (void)renderer;

    if (this == NULL || this->hidden == NULL) {
        SDL_memset(audioData, 0, audioDataSize);
        return AUDIO_DATA_CALLBACK_RESULT_VALID;
    }
    hidden = this->hidden;

    ++ohos_cb_total;

    if (hidden->filled_blocks != NULL && SDL_SemWaitTimeout(hidden->filled_blocks, 0) == 0) {
        size = SDL_min((int)audioDataSize, hidden->block_size);
        SDL_memcpy(audioData, hidden->blocks[hidden->read_index], size);
        if (size < (int)audioDataSize) {
            SDL_memset((Uint8 *)audioData + size, 0, (size_t)(audioDataSize - size));
        }
        hidden->read_index = (hidden->read_index + 1) % OHOS_AUDIO_BLOCK_COUNT;
        SDL_SemPost(hidden->free_blocks);
    } else {
        /* Underrun: keep the stream running with silence. */
        ++ohos_cb_underruns;
        SDL_memset(audioData, 0, audioDataSize);
    }

    if ((ohos_cb_total % 100) == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "OHAudio stats: callbacks=%d underruns=%d size=%d block=%d",
                     ohos_cb_total, ohos_cb_underruns, (int)audioDataSize, hidden->block_size);
    }

    return AUDIO_DATA_CALLBACK_RESULT_VALID;
}

static void OHAudio_OnReadData(OH_AudioCapturer *capturer, void *userData, void *audioData, int32_t audioDataSize)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *)userData;
    struct SDL_PrivateAudioData *hidden;
    int size;

    (void)capturer;

    if (this == NULL || this->hidden == NULL) {
        return;
    }
    hidden = this->hidden;

    if (hidden->free_blocks == NULL || SDL_SemWaitTimeout(hidden->free_blocks, 0) != 0) {
        return; /* No room, the sample is dropped. */
    }

    size = SDL_min((int)audioDataSize, hidden->block_size);
    SDL_memcpy(hidden->blocks[hidden->write_index], audioData, size);
    hidden->write_index = (hidden->write_index + 1) % OHOS_AUDIO_BLOCK_COUNT;
    SDL_SemPost(hidden->filled_blocks);
}

static void OHAudio_OnRendererError(OH_AudioRenderer *renderer, void *userData, OH_AudioStream_Result error)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *)userData;

    (void)renderer;
    (void)error;

    if (this != NULL) {
        SDL_OpenedAudioDeviceDisconnected(this);
    }
}

static void OHAudio_OnCapturerError(OH_AudioCapturer *capturer, void *userData, OH_AudioStream_Result error)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *)userData;

    (void)capturer;
    (void)error;

    if (this != NULL) {
        SDL_OpenedAudioDeviceDisconnected(this);
    }
}

static void OHAudio_OnRendererInterrupt(OH_AudioRenderer *renderer, void *userData,
                                        OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *)userData;

    (void)renderer;
    (void)type;

    if (this == NULL) {
        return;
    }
    /* Pause and stop both mean the stream no longer owns the output. */
    if (hint == AUDIOSTREAM_INTERRUPT_HINT_PAUSE || hint == AUDIOSTREAM_INTERRUPT_HINT_STOP) {
        SDL_OpenedAudioDeviceDisconnected(this);
    }
}

static void OHAudio_OnCapturerInterrupt(OH_AudioCapturer *capturer, void *userData,
                                        OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *)userData;

    (void)capturer;
    (void)type;

    if (this == NULL) {
        return;
    }
    if (hint == AUDIOSTREAM_INTERRUPT_HINT_PAUSE || hint == AUDIOSTREAM_INTERRUPT_HINT_STOP) {
        SDL_OpenedAudioDeviceDisconnected(this);
    }
}

static void OHAudio_FreeBlocks(struct SDL_PrivateAudioData *hidden)
{
    int i;

    for (i = 0; i < OHOS_AUDIO_BLOCK_COUNT; ++i) {
        if (hidden->blocks[i] != NULL) {
            SDL_free(hidden->blocks[i]);
            hidden->blocks[i] = NULL;
        }
    }
    if (hidden->free_blocks != NULL) {
        SDL_DestroySemaphore(hidden->free_blocks);
        hidden->free_blocks = NULL;
    }
    if (hidden->filled_blocks != NULL) {
        SDL_DestroySemaphore(hidden->filled_blocks);
        hidden->filled_blocks = NULL;
    }
}

static int OHAudio_OpenDevice(_THIS, const char *devname)
{
    OH_AudioStreamBuilder *builder = NULL;
    OH_AudioStream_Result res;
    OH_AudioStream_SampleFormat format = AUDIOSTREAM_SAMPLE_S16LE;
    int32_t sample_frames = 0;
    int32_t freq = 0;
    int32_t channels = 0;
    int i;

    (void)devname;

    /* Recording needs a runtime permission; opening fails so the caller can ask for it. */
    if (this->iscapture && !OH_AT_CheckSelfPermission("ohos.permission.MICROPHONE")) {
        return SDL_SetError("Microphone permission is not granted");
    }

    this->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    /* OHAudio has no notion of more than eight discrete channels. */
    this->spec.channels = (Uint8)SDL_clamp(this->spec.channels, 1, 8);

    res = OH_AudioStreamBuilder_Create(&builder, this->iscapture ? AUDIOSTREAM_TYPE_CAPTURER : AUDIOSTREAM_TYPE_RENDERER);
    if (res != AUDIOSTREAM_SUCCESS || builder == NULL) {
        return SDL_SetError("OH_AudioStreamBuilder_Create failed: %d", (int)res);
    }

    OH_AudioStreamBuilder_SetSampleFormat(builder, SDL2OHAudioFormat(this->spec.format));
    OH_AudioStreamBuilder_SetSamplingRate(builder, this->spec.freq);
    OH_AudioStreamBuilder_SetChannelCount(builder, this->spec.channels);
    OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_FAST);
    OH_AudioStreamBuilder_SetFrameSizeInCallback(builder, 1024);

    if (this->iscapture) {
        /* CAMCORDER is the recording source, not the one used for phone calls. */
        OH_AudioStreamBuilder_SetCapturerInfo(builder, AUDIOSTREAM_SOURCE_TYPE_CAMCORDER);
        OH_AudioStreamBuilder_SetCapturerReadDataCallback(builder, OHAudio_OnReadData, this);
        OH_AudioStreamBuilder_SetCapturerErrorCallback(builder, OHAudio_OnCapturerError, this);
        OH_AudioStreamBuilder_SetCapturerInterruptCallback(builder, OHAudio_OnCapturerInterrupt, this);
        res = OH_AudioStreamBuilder_GenerateCapturer(builder, &this->hidden->capturer);
    } else {
        OH_AudioStreamBuilder_SetRendererWriteDataCallback(builder, OHAudio_OnWriteData, this);
        OH_AudioStreamBuilder_SetRendererErrorCallback(builder, OHAudio_OnRendererError, this);
        OH_AudioStreamBuilder_SetRendererInterruptCallback(builder, OHAudio_OnRendererInterrupt, this);
        res = OH_AudioStreamBuilder_GenerateRenderer(builder, &this->hidden->renderer);
    }
    OH_AudioStreamBuilder_Destroy(builder);

    if (res != AUDIOSTREAM_SUCCESS) {
        return SDL_SetError("Couldn't generate the OHAudio stream: %d", (int)res);
    }

    /* Query what the stream actually settled on, the request is only a hint. */
    if (this->iscapture) {
        OH_AudioCapturer_GetFrameSizeInCallback(this->hidden->capturer, &sample_frames);
        OH_AudioCapturer_GetSamplingRate(this->hidden->capturer, &freq);
        OH_AudioCapturer_GetChannelCount(this->hidden->capturer, &channels);
        OH_AudioCapturer_GetSampleFormat(this->hidden->capturer, &format);
    } else {
        OH_AudioRenderer_GetFrameSizeInCallback(this->hidden->renderer, &sample_frames);
        OH_AudioRenderer_GetSamplingRate(this->hidden->renderer, &freq);
        OH_AudioRenderer_GetChannelCount(this->hidden->renderer, &channels);
        OH_AudioRenderer_GetSampleFormat(this->hidden->renderer, &format);
    }

    if (sample_frames <= 0 || freq <= 0 || channels <= 0) {
        return SDL_SetError("OHAudio returned an unusable stream format");
    }

    /* SDL must mix in exactly the granularity OHAudio asks for. */
    this->spec.samples = (Uint16)SDL_min(sample_frames, 0xFFFF);
    this->spec.freq = (int)freq;
    this->spec.channels = (Uint8)channels;
    this->spec.format = OHAudioFormat2SDL(format);

    this->hidden->block_size = (int)this->spec.samples * (SDL_AUDIO_BITSIZE(this->spec.format) / 8) * this->spec.channels;
    if (this->hidden->block_size <= 0) {
        return SDL_SetError("Invalid OHAudio block size");
    }

    for (i = 0; i < OHOS_AUDIO_BLOCK_COUNT; ++i) {
        this->hidden->blocks[i] = (Uint8 *)SDL_calloc(1, this->hidden->block_size);
        if (this->hidden->blocks[i] == NULL) {
            return SDL_OutOfMemory();
        }
    }

    this->hidden->free_blocks = SDL_CreateSemaphore(OHOS_AUDIO_BLOCK_COUNT);
    this->hidden->filled_blocks = SDL_CreateSemaphore(0);
    if (this->hidden->free_blocks == NULL || this->hidden->filled_blocks == NULL) {
        return SDL_SetError("Couldn't create the OHAudio block semaphores");
    }

    res = this->iscapture ? OH_AudioCapturer_Start(this->hidden->capturer)
                          : OH_AudioRenderer_Start(this->hidden->renderer);
    if (res != AUDIOSTREAM_SUCCESS) {
        return SDL_SetError("Couldn't start the OHAudio stream: %d", (int)res);
    }

    return 0;
}

static void OHAudio_ThreadInit(_THIS)
{
}

static void OHAudio_ThreadDeInit(_THIS)
{
}

static void OHAudio_WaitDevice(_THIS)
{
    if (this->hidden != NULL && this->hidden->free_blocks != NULL) {
        SDL_SemWait(this->hidden->free_blocks);
    }
}

static void OHAudio_PlayDevice(_THIS)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;

    if (hidden == NULL || hidden->filled_blocks == NULL) {
        return;
    }

    /* GetDeviceBuf() handed out the write block, so it is full now. */
    hidden->write_index = (hidden->write_index + 1) % OHOS_AUDIO_BLOCK_COUNT;
    SDL_SemPost(hidden->filled_blocks);
}

static Uint8 *OHAudio_GetDeviceBuf(_THIS)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;

    if (hidden == NULL || hidden->blocks[hidden->write_index] == NULL) {
        return NULL;
    }
    return hidden->blocks[hidden->write_index];
}

static int OHAudio_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;
    int size;

    if (hidden == NULL || hidden->filled_blocks == NULL) {
        SDL_memset(buffer, 0, buflen);
        return buflen;
    }

    if (SDL_SemWaitTimeout(hidden->filled_blocks, 1000) != 0) {
        SDL_memset(buffer, 0, buflen);
        return buflen;
    }

    size = SDL_min(buflen, hidden->block_size);
    SDL_memcpy(buffer, hidden->blocks[hidden->read_index], size);
    if (size < buflen) {
        SDL_memset((Uint8 *)buffer + size, 0, (size_t)(buflen - size));
    }
    hidden->read_index = (hidden->read_index + 1) % OHOS_AUDIO_BLOCK_COUNT;
    SDL_SemPost(hidden->free_blocks);

    return buflen;
}

static void OHAudio_FlushCapture(_THIS)
{
    struct SDL_PrivateAudioData *hidden = this->hidden;

    if (hidden == NULL || hidden->filled_blocks == NULL) {
        return;
    }

    while (SDL_SemTryWait(hidden->filled_blocks) == 0) {
        hidden->read_index = (hidden->read_index + 1) % OHOS_AUDIO_BLOCK_COUNT;
        SDL_SemPost(hidden->free_blocks);
    }
}

static void OHAudio_CloseDevice(_THIS)
{
    if (this->hidden == NULL) {
        return;
    }

    if (this->hidden->renderer != NULL) {
        OH_AudioRenderer_Stop(this->hidden->renderer);
        OH_AudioRenderer_Release(this->hidden->renderer);
        this->hidden->renderer = NULL;
    }
    if (this->hidden->capturer != NULL) {
        OH_AudioCapturer_Stop(this->hidden->capturer);
        OH_AudioCapturer_Release(this->hidden->capturer);
        this->hidden->capturer = NULL;
    }

    OHAudio_FreeBlocks(this->hidden);

    SDL_free(this->hidden);
    this->hidden = NULL;
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
    (void)name;
    (void)spec;
    (void)iscapture;

    return SDL_Unsupported();
}

/**
 * @brief Initialize the OHAudio driver and register backend callbacks
 *
 * Called once during SDL2 audio subsystem initialization to register
 * the set of function pointers that the SDL2 audio framework will use
 * to interact with the OHAudio backend. Each field in the impl struct
 * corresponds to a specific stage in the audio device lifecycle.
 *
 * The registered callbacks form a complete backend contract:
 *
 * - DetectDevices:       Enumerate available audio devices (output & input)
 * - OpenDevice:          Open a specific device and configure the audio stream
 * - ThreadInit:          Per-thread setup when the audio worker thread starts
 * - ThreadDeinit:        Per-thread cleanup when the audio worker thread exits
 * - WaitDevice:          Block until the device is ready for the next I/O cycle
 * - PlayDevice:          Submit audio data to the device for playback
 * - GetDeviceBuf:        Obtain a writable buffer for direct audio data access
 * - CaptureFromDevice:   Read captured audio data from the device
 * - FlushCapture:        Discard any buffered captured data
 * - CloseDevice:         Close the device and release associated resources
 * - LockDevice:          Acquire exclusive access to the device (thread safety)
 * - UnlockDevice:        Release exclusive access to the device
 * - FreeDeviceHandle:    Free a device handle obtained during enumeration
 * - Deinitialize:        Tear down the entire driver (inverse of this function)
 * - GetDefaultAudioInfo: Query the system default audio device and its format
 *
 * @param impl SDL2 audio driver implementation struct to populate
 * @return SDL_TRUE on success
 */
static SDL_bool OHAudio_Init(SDL_AudioDriverImpl *impl)
{
    impl->DetectDevices = OHAudio_DetectDevices;
    impl->OpenDevice = OHAudio_OpenDevice;
    impl->ThreadInit = OHAudio_ThreadInit;
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

    impl->HasCaptureSupport = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_FALSE;
    impl->OnlyHasDefaultOutputDevice = SDL_FALSE;

    return SDL_TRUE;
}

AudioBootStrap OHAudio_bootstrap = {
    "OHAudio", "OHOS OHAudio audio driver", OHAudio_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
