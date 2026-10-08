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

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_audio_c.h"
#include "SDL_audio.h"
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

static Uint8 *OHAudio_GetDeviceBuf(_THIS)
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
