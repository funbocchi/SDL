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

#include "SDL_ohosvideo.h"

#include <fcntl.h>
#include <unistd.h>
#include <window_manager/oh_display_manager.h>

#include "../../core/ohos/SDL_ohos.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "SDL_ohosgles.h"
#endif
#ifdef SDL_VIDEO_VULKAN
#include "SDL_ohosvulkan.h"
#endif
#include "SDL_ohosevents.h"
#include "SDL_ohosmessagebox.h"
#include "SDL_ohoswindow.h"

#define OHOS_VID_DRIVER_NAME "OHOS"

const char *OHOS_Tool_DisplayManagerErrorToString(NativeDisplayManager_ErrorCode code)
{
    switch (code) {
    case DISPLAY_MANAGER_OK:
        return "OK";
    case DISPLAY_MANAGER_ERROR_NO_PERMISSION:
        return "Permission verification failed";
    case DISPLAY_MANAGER_ERROR_NOT_SYSTEM_APP:
        return "Non-system application attempts to call a system API";
    case DISPLAY_MANAGER_ERROR_INVALID_PARAM:
        return "Parameter check failed";
    case DISPLAY_MANAGER_ERROR_DEVICE_NOT_SUPPORTED:
        return "Device does not support the API";
    case DISPLAY_MANAGER_ERROR_INVALID_SCREEN:
        return "Invalid display";
    case DISPLAY_MANAGER_ERROR_INVALID_CALL:
        return "Operation object does not have the operation permission";
    case DISPLAY_MANAGER_ERROR_SYSTEM_ABNORMAL:
        return "System service is abnormal";
    case DISPLAY_MANAGER_ERROR_ILLEGAL_PARAM:
        return "Illegal parameter";
    default:
        return "Unknown display manager error";
    }
}

static SDL_DisplayOrientation OHOS_Tool_OrientationToSDL(NativeDisplayManager_Orientation orientation)
{
    switch (orientation) {
    case DISPLAY_MANAGER_PORTRAIT:
        return SDL_ORIENTATION_PORTRAIT;
    case DISPLAY_MANAGER_LANDSCAPE:
        return SDL_ORIENTATION_LANDSCAPE;
    case DISPLAY_MANAGER_PORTRAIT_INVERTED:
        return SDL_ORIENTATION_PORTRAIT_FLIPPED;
    case DISPLAY_MANAGER_LANDSCAPE_INVERTED:
        return SDL_ORIENTATION_LANDSCAPE_FLIPPED;
    default:
        return SDL_ORIENTATION_UNKNOWN;
    }
}

float OHOS_Display_GetDensityPixels(SDL_Window *window)
{
    SDL_VideoDisplay *display;
    float retval = 1.0f;

    if (!window) {
        SDL_SetError("invalid param");
        goto endfunc;
    }

    display = SDL_GetDisplayForWindow(window);
    if (!display) {
        goto endfunc;
    }
    retval = ((SDL_DisplayData *)display->driverdata)->density_pixels;

endfunc:
    return retval;
}

static int OHOS_GetDisplayBounds(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    SDL_DisplayData *data;

    data = (SDL_DisplayData *)display->driverdata;
    rect->x = data->x_offset;
    rect->y = data->y_offset;
    rect->w = data->width;
    rect->h = data->height;
    return 0;
}

static int OHOS_VideoInit(_THIS)
{
    int retval = 0;

    SDL_VideoData *video_data = (SDL_VideoData *)(_this->driverdata);

    SDL_DisplayData *dis_data;

    SDL_VideoDisplay display;
    SDL_DisplayMode mode;

    NativeDisplayManager_ErrorCode err_code;
    NativeDisplayManager_DisplaysInfo *all_displays = NULL;
    NativeDisplayManager_DisplayInfo *display_info = NULL;
    int display_index;
    int fds[2];

    video_data->wake_fd = -1;
    video_data->wake_write = -1;
    video_data->events_ready = SDL_FALSE;

    if (pipe(fds) == 0) {
        fcntl(fds[0], F_SETFL, O_NONBLOCK);
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFL, O_NONBLOCK);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);

        video_data->wake_fd = fds[0];
        video_data->wake_write = fds[1];
        video_data->events_ready = SDL_TRUE;
    }

    if ((err_code = OH_NativeDisplayManager_CreateAllDisplays(&all_displays)) != DISPLAY_MANAGER_OK) {
        retval = SDL_SetError("OHOS VideoInit: OH_NativeDisplayManager_CreateAllDisplays failed, err_code: %s (%d)",
                              OHOS_Tool_DisplayManagerErrorToString(err_code), (int)err_code);
        goto errfunc;
    }

    if (all_displays == NULL) {
        retval = SDL_SetError("OHOS VideoInit: OH_NativeDisplayManager_CreateAllDisplays returned OK but all_displays is NULL (no display info available)");
        goto errfunc;
    }

    for (display_index = 0; display_index < all_displays->displaysLength; display_index++) {
        display_info = &(all_displays->displaysInfo[display_index]);

        if (!display_info->isAlive) {
            continue;
        }

        if (!(dis_data = (SDL_DisplayData *)SDL_calloc(1, sizeof(SDL_DisplayData)))) {
            SDL_OutOfMemory();
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "SDL_calloc failed: %s", SDL_GetError());
            continue;
        }

        SDL_zero(mode);
        SDL_zero(display);

        mode.refresh_rate = (int)(display_info->refreshRate);

        mode.w = display_info->width;
        mode.h = display_info->height;

        mode.format = SDL_PIXELFORMAT_RGBA8888;

        display.desktop_mode = mode;
        display.current_mode = mode;

        display.name = display_info->name; /* Pass it to the subsequent SDL_AddVideoDisplay for memory allocation */

        display.orientation = OHOS_Tool_OrientationToSDL(display_info->orientation);

        dis_data->id = display_info->id;

        dis_data->width = display_info->width;
        dis_data->height = display_info->height;

        dis_data->physical_width = display_info->physicalWidth;
        dis_data->physical_height = display_info->physicalHeight;

        dis_data->refresh_rate = display_info->refreshRate;

        dis_data->available_width = display_info->availableWidth;
        dis_data->available_height = display_info->availableHeight;

        dis_data->density_dpi = display_info->densityDPI;

        dis_data->density_pixels = display_info->densityPixels;

        dis_data->scaled_density = display_info->scaledDensity;

        dis_data->x_dpi = display_info->xDPI;
        dis_data->y_dpi = display_info->yDPI;

        dis_data->rotation = display_info->rotation;

        dis_data->state = display_info->state;

        dis_data->orientation = display_info->orientation;

        if ((err_code = OH_NativeDisplayManager_GetDisplayPosition(dis_data->id, &dis_data->x_offset, &dis_data->y_offset)) != DISPLAY_MANAGER_OK) {
            dis_data->x_offset = 0;
            dis_data->y_offset = 0;
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "OH_NativeDisplayManager_GetDisplayPosition failed, error_code: %d", err_code);
        }

        display.driverdata = dis_data;

        if (SDL_AddVideoDisplay(&display, SDL_FALSE) < 0) {
            SDL_SetError("OHOS VideoInit: failed to add display '%s' (index %d)",
                         display.name ? display.name : "unknown", display_index);
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "%s", SDL_GetError());
            SDL_free(display.driverdata);
            continue;
        }

        SDL_AddDisplayMode(&_this->displays[_this->num_displays - 1], &mode);
    }

    OH_NativeDisplayManager_DestroyAllDisplays(all_displays);
    all_displays = NULL;

    if (SDL_GetNumVideoDisplays() == 0) {
        retval = SDL_SetError("OHOS VideoInit: no active display found");
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "%s", SDL_GetError());
    }

    OHOS_Event_Register();

    return retval;

errfunc:
    if (all_displays) {
        OH_NativeDisplayManager_DestroyAllDisplays(all_displays);
        all_displays = NULL;
    }
    return retval;
}

static void OHOS_VideoQuit(_THIS)
{
    SDL_VideoData *video_data = (SDL_VideoData *)(_this->driverdata);
    int num_displays = SDL_GetNumVideoDisplays();
    int i;

    for (i = 0; i < num_displays; i++) {
        SDL_VideoDisplay *display = SDL_GetDisplay(i);
        if (display && display->driverdata) {
            SDL_free(display->driverdata);
            display->driverdata = NULL;
        }
    }

    /* TODO: 注销显示变化监听器 */

    if (video_data) {
        char byte = 0;

        if (video_data->events_ready && video_data->wake_write >= 0) {
            (void)write(video_data->wake_write, &byte, 1);
        }
        video_data->events_ready = SDL_FALSE;
        if (video_data->wake_fd >= 0) {
            close(video_data->wake_fd);
            video_data->wake_fd = -1;
        }
        if (video_data->wake_write >= 0) {
            close(video_data->wake_write);
            video_data->wake_write = -1;
        }
        SDL_free(video_data);
    }
    _this->driverdata = NULL;
}

static int OHOS_GetDisplayDPI(_THIS, SDL_VideoDisplay *display, float *ddpi, float *hdpi, float *vdpi)
{
    SDL_DisplayData *data = (SDL_DisplayData *)(display->driverdata);
    if (!data) {
        return SDL_SetError("OHOS_GetDisplayDPI: no driverdata");
    }

    if (hdpi) {
        *hdpi = data->x_dpi;
    }
    if (vdpi) {
        *vdpi = data->y_dpi;
    }

    if (ddpi) {
        if (data->x_dpi > 0.0f && data->y_dpi > 0.0f && display->desktop_mode.w > 0 && display->desktop_mode.h > 0) {
            *ddpi = SDL_ComputeDiagonalDPI(display->desktop_mode.w, display->desktop_mode.h,
                                           (float)(display->desktop_mode.w) / (data->x_dpi), (float)(display->desktop_mode.h) / (data->y_dpi));
        } else {
            *ddpi = 0.0f;
        }
    }
    return 0;
}

static void OHOS_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *OHOS_CreateDevice(void)
{
    SDL_VideoDevice *device = NULL;
    SDL_VideoData *data = NULL;

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "OHOS_CreateDevice: begin");

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        goto errfunc;
    }

    data = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_OutOfMemory();
        goto errfunc;
    }

    device->driverdata = data;

    /* 绑定接口实现 */
    device->VideoInit = OHOS_VideoInit;
    device->VideoQuit = OHOS_VideoQuit;
    device->ResetTouch;

    device->RefreshDisplays;
    device->GetDisplayBounds = OHOS_GetDisplayBounds;
    device->GetDisplayUsableBounds;
    device->GetDisplayDPI = OHOS_GetDisplayDPI;
    device->GetDisplayModes;
    device->SetDisplayMode;

    device->CreateSDLWindow = OHOS_CreateWindow;
    device->CreateSDLWindowFrom = OHOS_CreateWindowFrom;
    device->SetWindowTitle = OHOS_SetWindowTitle;
    device->SetWindowIcon;
    device->SetWindowPosition = OHOS_SetWindowPosition;
    device->SetWindowSize = OHOS_SetWindowSize;
    device->SetWindowMinimumSize;
    device->SetWindowMaximumSize;
    device->GetWindowBordersSize;
    device->GetWindowSizeInPixels;
    device->SetWindowOpacity;
    device->SetWindowModalFor;
    device->SetWindowInputFocus;
    device->ShowWindow = OHOS_ShowWindow;
    device->HideWindow = OHOS_HideWindow;
    device->RaiseWindow;
    device->MaximizeWindow;
    device->MinimizeWindow;
    device->RestoreWindow;
    device->SetWindowBordered;
    device->SetWindowResizable;
    device->SetWindowAlwaysOnTop;
    device->SetWindowFullscreen;
    device->SetWindowGammaRamp;
    device->GetWindowGammaRamp;
    device->GetWindowICCProfile;
    device->GetWindowDisplayIndex = OHOS_GetWindowDisplayIndex;
    device->SetWindowMouseRect;
    device->SetWindowMouseGrab;
    device->SetWindowKeyboardGrab;
    device->DestroyWindow = OHOS_DestroyWindow;
    device->CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer;
    device->OnWindowEnter;
    device->FlashWindow;

    /* GLES */
#ifdef SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = OHOS_GL_LoadLibrary;
    device->GL_GetProcAddress = OHOS_GL_GetProcAddress;
    device->GL_UnloadLibrary = OHOS_GL_UnloadLibrary;
    device->GL_CreateContext = OHOS_GL_CreateContext;
    device->GL_MakeCurrent = OHOS_GL_MakeCurrent;
    device->GL_GetDrawableSize = OHOS_GL_GetDrawableSize;
    device->GL_SetSwapInterval = OHOS_GL_SetSwapInterval;
    device->GL_GetSwapInterval = OHOS_GL_GetSwapInterval;
    device->GL_SwapWindow = OHOS_GL_SwapWindow;
    device->GL_DeleteContext = OHOS_GL_DeleteContext;
    device->GL_DefaultProfileConfig = OHOS_GL_DefaultProfileConfig;
#endif

    /* Vulkan*/
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface;
    device->Vulkan_GetDrawableSize;
#endif

    device->WaitEventTimeout = OHOS_WaitEventTimeout;
    device->SendWakeupEvent = OHOS_SendWakeupEvent;
    device->PumpEvents = OHOS_PumpEvents;

    device->free = OHOS_DeleteDevice;

    return device;

errfunc:
    if (device) {
        SDL_free(device);
    }
    if (data) {
        SDL_free(data);
    }
    return NULL;
}

VideoBootStrap OHOS_bootstrap = {
    OHOS_VID_DRIVER_NAME,
    "SDL OHOS video driver",
    OHOS_CreateDevice,
    OHOS_ShowMessageBox
};

#endif

/* vi: set ts=4 sw=4 expandtab: */