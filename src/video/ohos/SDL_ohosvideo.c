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

#include <EGL/egl.h>
#include <window_manager/oh_display_info.h>
#include <window_manager/oh_display_manager.h>

#include "../../events/SDL_events_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../SDL_pixels_c.h"
#include "../SDL_sysvideo.h"
#include "SDL_hints.h"
#include "SDL_mouse.h"
#include "SDL_video.h"

#include "SDL_ohosmessagebox.h"
#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"

#define OHOS_VID_DRIVER_NAME "OHOS"

/* Tools */
static SDL_DisplayOrientation OHOS_RotationToOrientation(NativeDisplayManager_Rotation rotation)
{
    switch (rotation) {
    case DISPLAY_MANAGER_ROTATION_0:
        return SDL_ORIENTATION_PORTRAIT;
    case DISPLAY_MANAGER_ROTATION_90:
        return SDL_ORIENTATION_LANDSCAPE;
    case DISPLAY_MANAGER_ROTATION_180:
        return SDL_ORIENTATION_PORTRAIT_FLIPPED;
    case DISPLAY_MANAGER_ROTATION_270:
        return SDL_ORIENTATION_LANDSCAPE_FLIPPED;
    default:
        return SDL_ORIENTATION_UNKNOWN;
    }
}

/**
 * @brief Initializes the OHOS video subsystem.
 *
 * This function performs three main tasks:
 * 1. Allocates and attaches device-level private data (OHOS_VideoDeviceData)
 *    to _this->driverdata.
 * 2. Initializes EGL (eglGetDisplay, eglInitialize, eglChooseConfig) when
 *    SDL_VIDEO_OPENGL_EGL is enabled, storing the EGLDisplay and EGLConfig
 *    for later use by GL_CreateContext and GL_SwapWindow.
 * 3. Queries all available physical displays via
 *    OH_NativeDisplayManager_CreateAllDisplays, registers each active display
 *    to SDL2 via SDL_AddVideoDisplay, and fills display-level private data
 *    (OHOS_VideoDisplayData) with DPI, available bounds, rotation, etc.
 *
 * If no displays are available after enumeration, or if EGL initialization
 * fails, all allocated resources are cleaned up and an error is returned.
 *
 * @param _this The video device handle, created by OHOS_CreateDevice.
 * @return 0 on success, -1 on failure with SDL error set.
 *
 * @note Vulkan does not require any initialization in this function.
 *       Vulkan resources are lazily initialized in Vulkan_LoadLibrary.
 */
static int OHOS_VideoInit(_THIS)
{
    OHOS_VideoDeviceData *video_device_data = NULL;
    int dis_index;
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;

    NativeDisplayManager_DisplaysInfo *all_displays = NULL;
    NativeDisplayManager_DisplayInfo *dis_info = NULL;
    NativeDisplayManager_ErrorCode error_code;
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE
    };
    EGLint num_configs = 0;
#endif

    OHOS_VideoDisplayData *video_display_data = NULL;

    video_device_data = (OHOS_VideoDeviceData *)SDL_calloc(1, sizeof(OHOS_VideoDeviceData));
    if (!video_device_data) {
        return SDL_OutOfMemory();
    }
    _this->driverdata = video_device_data;

#ifdef SDL_VIDEO_OPENGL_EGL
    video_device_data->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (video_device_data->egl_display == EGL_NO_DISPLAY) {
        SDL_free(video_device_data);
        _this->driverdata = NULL;
        return SDL_SetError("OHOS VideoInit: eglGetDisplay failed");
    }

    if (!eglInitialize(video_device_data->egl_display, &video_device_data->egl_major, &video_device_data->egl_minor)) {
        eglTerminate(video_device_data->egl_display);
        SDL_free(video_device_data);
        _this->driverdata = NULL;
        return SDL_SetError("OHOS VideoInit: eglInitialze failed");
    }

    if (!eglChooseConfig(video_device_data->egl_display, attribs, &video_device_data->egl_config, 1, &num_configs) || num_configs == 0) {
        eglTerminate(video_device_data->egl_display);
        SDL_free(video_device_data);
        _this->driverdata = NULL;
        return SDL_SetError("OHOS VideoInit: eglChooseConfig failed");
    }
#endif

    error_code = OH_NativeDisplayManager_CreateAllDisplays(&all_displays);

    if (error_code == DISPLAY_MANAGER_OK && all_displays != NULL) {
        for (dis_index = 0; dis_index < all_displays->displaysLength; dis_index++) {
            dis_info = &all_displays->displaysInfo[dis_index];
            if (!dis_info->isAlive) {
                continue;
            }
            SDL_zero(mode);
            mode.format = SDL_PIXELFORMAT_RGBA8888;
            mode.w = dis_info->width;
            mode.h = dis_info->height;
            mode.refresh_rate = (int)dis_info->refreshRate;
            mode.driverdata = NULL;

            video_display_data = (OHOS_VideoDisplayData *)SDL_calloc(1, sizeof(OHOS_VideoDisplayData));
            if (!video_display_data) {
                SDL_OutOfMemory();
                video_display_data = NULL;
                continue;
            }
            video_display_data->id = dis_info->id;
            video_display_data->physical_width = dis_info->physicalWidth;
            video_display_data->physical_height = dis_info->physicalHeight;
            video_display_data->available_width = dis_info->availableWidth;
            video_display_data->available_height = dis_info->availableHeight;
            video_display_data->density_dpi = dis_info->densityDPI;
            video_display_data->density_pixels = dis_info->densityPixels;
            video_display_data->scaled_density = dis_info->scaledDensity;
            video_display_data->x_dpi = dis_info->xDPI;
            video_display_data->y_dpi = dis_info->yDPI;
            video_display_data->rotation = dis_info->rotation;

            SDL_zero(display);
            display.desktop_mode = mode;
            display.current_mode = mode;
            display.orientation = OHOS_RotationToOrientation(video_display_data->rotation);
            display.driverdata = video_display_data;

            if (SDL_AddVideoDisplay(&display, SDL_FALSE) < 0) {
                SDL_free(video_display_data);
            }
        }

        OH_NativeDisplayManager_DestroyAllDisplays(all_displays);
    }

    if (SDL_GetNumVideoDisplays() == 0) {
#ifdef SDL_VIDEO_OPENGL_EGL
        eglTerminate(video_device_data->egl_display);
#endif
        SDL_free(video_device_data);
        _this->driverdata = NULL;
        return SDL_SetError("OHOS VideoInit: No displays available");
    }

    return 0;
}

/**
 * @brief Shuts down the OHOS video subsystem.
 *
 * Releases all resources allocated by OHOS_VideoInit in reverse order:
 * 1. Frees each display's private data (OHOS_VideoDisplayData) that was
 *    attached to SDL_VideoDisplay.driverdata during registration.
 * 2. Terminates the EGL connection (eglTerminate) when SDL_VIDEO_OPENGL_EGL
 *    is enabled.
 * 3. Frees the device-level private data (OHOS_VideoDeviceData) and clears
 *    _this->driverdata.
 *
 * @param _this The video device handle.
 *
 * @note This function is safe to call even if VideoInit failed partially,
 *       as it checks for NULL before freeing each resource.
 */
static void OHOS_VideoQuit(_THIS)
{
    OHOS_VideoDeviceData *video_device_data = NULL;
    SDL_VideoDisplay *display = NULL;
    int num_displays = 0;
    int i;

    video_device_data = (OHOS_VideoDeviceData *)_this->driverdata;
    num_displays = SDL_GetNumVideoDisplays();
    for (i = 0; i < num_displays; i++) {
        display = SDL_GetDisplay(i);
        if (display && display->driverdata) {
            SDL_free(display->driverdata);
            display->driverdata = NULL;
        }
    }

#ifdef SDL_VIDEO_OPENGL_EGL
    if (video_device_data->egl_display != EGL_NO_DISPLAY) {
        eglTerminate(video_device_data->egl_display);
        video_device_data->egl_display = EGL_NO_DISPLAY;
    }
#endif

    SDL_free(video_device_data);
    _this->driverdata = NULL;
}

static SDL_VideoDevice *OHOS_CreateDevice(void)
{
    SDL_VideoDevice *device;
    SDL_bool block_on_pause;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->VideoInit = OHOS_VideoInit;
    device->VideoQuit = OHOS_VideoQuit;

    device->CreateSDLWindow = OHOS_CreateSDLWindow;

#ifdef SDL_VIDEO_OPENGL_EGL
#endif

#ifdef SDL_VIDEO_VULKAN
#endif

    device->driverdata = NULL; /* Create and populate in videoinit */
    return device;
}

VideoBootStrap OHOS_bootstrap = {
    OHOS_VID_DRIVER_NAME,
    "SDL OHOS video driver",
    OHOS_CreateDevice,
    /* OHOS_ShowMessageBox */
    NULL
};

#endif

/* vi: set ts=4 sw=4 expandtab: */