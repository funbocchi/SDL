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

#include "SDL_ohosmouse.h"
#include "SDL_ohoswindow.h"

#include "../../core/ohos/SDL_ohos.h"

#include <multimedia/image_framework/image/pixelmap_native.h>
#include <multimodalinput/oh_input_manager.h>
#include <multimodalinput/oh_pointer_style.h>
#include <arkui/ui_input_event.h>

#include "../../events/SDL_mouse_c.h"

/* Every cursor the application creates stands for one ArkUI pointer style. */
typedef struct OHOS_CursorData
{
    int32_t style;                /* Input_PointerStyle, used when custom is NULL */
    Input_CustomCursor *custom;   /* set for cursors created from a surface */
} OHOS_CursorData;

static int32_t OHOS_MouseTool_PointerStyleFromSystemCursor(SDL_SystemCursor id)
{
    switch (id) {
    case SDL_SYSTEM_CURSOR_ARROW:
        return DEFAULT;
    case SDL_SYSTEM_CURSOR_IBEAM:
        return TEXT_CURSOR;
    case SDL_SYSTEM_CURSOR_WAIT:
    case SDL_SYSTEM_CURSOR_WAITARROW:
        return LOADING;
    case SDL_SYSTEM_CURSOR_CROSSHAIR:
        return CROSS;
    case SDL_SYSTEM_CURSOR_SIZENWSE:
        return NORTH_WEST_SOUTH_EAST;
    case SDL_SYSTEM_CURSOR_SIZENESW:
        return NORTH_EAST_SOUTH_WEST;
    case SDL_SYSTEM_CURSOR_SIZEWE:
        return WEST_EAST;
    case SDL_SYSTEM_CURSOR_SIZENS:
        return NORTH_SOUTH;
    case SDL_SYSTEM_CURSOR_SIZEALL:
        return MOVE;
    case SDL_SYSTEM_CURSOR_NO:
        return CURSOR_FORBID;
    case SDL_SYSTEM_CURSOR_HAND:
        return HAND_POINTING;
    default:
        return DEFAULT;
    }
}

SDL_Cursor *OHOS_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor;
    OHOS_CursorData *data;

    cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(SDL_Cursor));
    if (cursor == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (OHOS_CursorData *)SDL_calloc(1, sizeof(OHOS_CursorData));
    if (data == NULL) {
        SDL_free(cursor);
        SDL_OutOfMemory();
        return NULL;
    }
    data->style = OHOS_MouseTool_PointerStyleFromSystemCursor(id);

    cursor->driverdata = data;
    return cursor;
}

SDL_Cursor *OHOS_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor = NULL;
    OHOS_CursorData *data = NULL;
    OH_Pixelmap_InitializationOptions *options = NULL;
    OH_PixelmapNative *pixelmap = NULL;
    Input_CustomCursor *custom = NULL;
    uint8_t *pixels = NULL;
    size_t length;
    int row;

    if (surface == NULL) {
        SDL_InvalidParamError("surface");
        return NULL;
    }
    if (surface->format->format != SDL_PIXELFORMAT_ARGB8888) {
        SDL_Unsupported();
        return NULL;
    }

    /* The pixelmap expects tightly packed rows; SDL surfaces may pad them. */
    length = (size_t)surface->w * (size_t)surface->h * 4;
    pixels = (uint8_t *)SDL_malloc(length);
    if (pixels == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }
    for (row = 0; row < surface->h; ++row) {
        SDL_memcpy(pixels + (size_t)row * (size_t)surface->w * 4,
                   (uint8_t *)surface->pixels + (size_t)row * (size_t)surface->pitch,
                   (size_t)surface->w * 4);
    }

    if (OH_PixelmapInitializationOptions_Create(&options) != IMAGE_SUCCESS) {
        goto done;
    }
    OH_PixelmapInitializationOptions_SetWidth(options, surface->w);
    OH_PixelmapInitializationOptions_SetHeight(options, surface->h);
    OH_PixelmapInitializationOptions_SetPixelFormat(options, PIXEL_FORMAT_RGBA_8888);
    OH_PixelmapInitializationOptions_SetAlphaType(options, PIXELMAP_ALPHA_TYPE_UNPREMULTIPLIED);

    if (OH_PixelmapNative_CreatePixelmap(pixels, length, options, &pixelmap) != IMAGE_SUCCESS) {
        goto done;
    }

    custom = OH_Input_CustomCursor_Create(pixelmap, hot_x, hot_y);
    if (custom == NULL) {
        goto done;
    }

    cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(SDL_Cursor));
    if (cursor == NULL) {
        SDL_OutOfMemory();
        goto done;
    }

    data = (OHOS_CursorData *)SDL_calloc(1, sizeof(OHOS_CursorData));
    if (data == NULL) {
        SDL_free(cursor);
        cursor = NULL;
        SDL_OutOfMemory();
        goto done;
    }
    data->style = DEFAULT;
    data->custom = custom;
    custom = NULL;

    cursor->driverdata = data;

done:
    if (options != NULL) {
        OH_PixelmapInitializationOptions_Release(options);
    }
    if (pixelmap != NULL) {
        OH_PixelmapNative_Release(pixelmap);
    }
    if (custom != NULL) {
        OH_Input_CustomCursor_Destroy(&custom);
    }
    SDL_free(pixels);

    return cursor;
}

int OHOS_ShowCursor(SDL_Cursor *cursor)
{
    SDL_Window *window;
    SDL_WindowData *window_data;
    OHOS_CursorData *cursor_data;
    Input_CursorConfig *config;
    Input_Result result;

    if (cursor == NULL) {
        result = OH_Input_SetPointerVisible(false);
        if (result != INPUT_SUCCESS) {
            return SDL_SetError("OH_Input_SetPointerVisible failed (%d)", (int)result);
        }
        return 0;
    }

    /* The window id is delivered asynchronously, so the style can only be applied
       once it has arrived; the pointer is still shown without it. */
    window = SDL_GetMouseFocus();
    window_data = (window != NULL) ? (SDL_WindowData *)window->driverdata : NULL;
    cursor_data = (OHOS_CursorData *)cursor->driverdata;

    if (window_data != NULL && window_data->ohos_window_id != 0 && cursor_data != NULL) {
        if (cursor_data->custom != NULL) {
            config = OH_Input_CursorConfig_Create(SDL_TRUE);
            result = OH_Input_SetCustomCursor(window_data->ohos_window_id, cursor_data->custom, config);
            if (config != NULL) {
                OH_Input_CursorConfig_Destroy(&config);
            }
        } else {
            result = OH_Input_SetPointerStyle(window_data->ohos_window_id, cursor_data->style);
        }

        if (result != INPUT_SUCCESS) {
            SDL_SetError("setting the pointer style failed (%d)", (int)result);
        }
    }

    result = OH_Input_SetPointerVisible(true);
    if (result != INPUT_SUCCESS) {
        return SDL_SetError("OH_Input_SetPointerVisible failed (%d)", (int)result);
    }

    return 0;
}

/* ArkUI numbers the buttons LEFT, RIGHT, MIDDLE, BACK, FORWARD, while SDL has
   MIDDLE and RIGHT the other way around, so this must never be a plain cast. */
static Uint8 OHOS_MouseTool_ButtonFromArkUI(int32_t arkui_button)
{   
    switch (arkui_button) {
    case UI_MOUSE_EVENT_BUTTON_LEFT:
        return SDL_BUTTON_LEFT;
    case UI_MOUSE_EVENT_BUTTON_RIGHT:
        return SDL_BUTTON_RIGHT;
    case UI_MOUSE_EVENT_BUTTON_MIDDLE:
        return SDL_BUTTON_MIDDLE;
    case UI_MOUSE_EVENT_BUTTON_BACK:
        return SDL_BUTTON_X1;
    case UI_MOUSE_EVENT_BUTTON_FORWARD:
        return SDL_BUTTON_X2;
    default:
        return 0;
    }
}

static void OHOS_Mouse_HandleMove(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    OHOS_FloatPair payload;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
        /* The raw delta keeps changing once the pointer is locked or at the edge. */
        payload.u = (Uint64)(intptr_t)event->user.data2;
        SDL_SendMouseMotion(window, 0, 1,
                            (int)SDL_lroundf(payload.f[0]),
                            (int)SDL_lroundf(payload.f[1]));
    } else {
        payload.u = (Uint64)(intptr_t)event->user.data1;
        SDL_SendMouseMotion(window, 0, 0,
                            (int)SDL_lroundf(payload.f[0]),
                            (int)SDL_lroundf(payload.f[1]));
    }
}

static void OHOS_Mouse_HandleButton(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    Uint8 button;
    Uint8 state;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    button = OHOS_MouseTool_ButtonFromArkUI((int32_t)(intptr_t)event->user.data1);
    if (button == 0) {
        return;
    }
    state = (Uint8)(intptr_t)event->user.data2;

    SDL_SendMouseButton(window, 0, state, button);
}

static void OHOS_Mouse_HandleCancel(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    Uint32 pressed;
    int button;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    /* The gesture was cancelled, so the matching release never arrives. Report
       every button SDL still considers held as released, or the application
       keeps dragging forever. */
    pressed = SDL_GetMouseState(NULL, NULL);
    for (button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; ++button) {
        if (pressed & SDL_BUTTON(button)) {
            SDL_SendMouseButton(window, 0, SDL_RELEASED, (Uint8)button);
        }
    }
}

void OHOS_Mouse_HandleEvent(_THIS, const SDL_Event *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->user.code) {
    case OHOS_MOUSE_EVENT_MOVE:
        OHOS_Mouse_HandleMove(_this, event);
        break;
    case OHOS_MOUSE_EVENT_BUTTON:
        OHOS_Mouse_HandleButton(_this, event);
        break;
    case OHOS_MOUSE_EVENT_CANCEL:
        OHOS_Mouse_HandleCancel(_this, event);
        break;
    default:
        break;
    }
}

void OHOS_MoveCursor(SDL_Cursor *cursor)
{
    /* The system moves the pointer; there is nothing to do per motion event. */
    (void)cursor;
}

void OHOS_FreeCursor(SDL_Cursor *cursor)
{
    OHOS_CursorData *data;

    if (cursor == NULL) {
        return;
    }

    data = (OHOS_CursorData *)cursor->driverdata;
    if (data != NULL) {
        if (data->custom != NULL) {
            OH_Input_CustomCursor_Destroy(&data->custom);
        }
        SDL_free(data);
    }
    SDL_free(cursor);
}

void OHOS_WarpMouse(SDL_Window *window, int x, int y)
{
    (void)window;
    (void)x;
    (void)y;

    SDL_Unsupported();
}

int OHOS_WarpMouseGlobal(int x, int y)
{
    (void)x;
    (void)y;

    return SDL_Unsupported();
}

int OHOS_SetRelativeMouseMode(SDL_bool enabled)
{
    /* There is no pointer lock, so relative mode is emulated: the mouse callback
       reports the RAW_DELTA values and SDL_Mouse consumes them as relative motion. */
    (void)enabled;

    return 0;
}

int OHOS_CaptureMouse(SDL_Window *window)
{
    (void)window;

    return SDL_Unsupported();
}

Uint32 OHOS_GetGlobalMouseState(int *x, int *y)
{
    SDL_Window *window;
    int window_x = 0, window_y = 0;
    Uint32 state;

    /* SDL keeps the pointer relative to the window it was last reported for. */
    state = SDL_GetMouseState(x, y);

    window = SDL_GetMouseFocus();
    if (window != NULL) {
        SDL_GetWindowPosition(window, &window_x, &window_y);
    }

    *x += window_x;
    *y += window_y;

    return state;
}

void OHOS_MouseInit(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = OHOS_CreateCursor;
    mouse->CreateSystemCursor = OHOS_CreateSystemCursor;
    mouse->ShowCursor = OHOS_ShowCursor;
    mouse->MoveCursor = OHOS_MoveCursor;
    mouse->FreeCursor = OHOS_FreeCursor;
    mouse->WarpMouse = OHOS_WarpMouse;
    mouse->WarpMouseGlobal = OHOS_WarpMouseGlobal;
    mouse->SetRelativeMouseMode = OHOS_SetRelativeMouseMode;
    mouse->CaptureMouse = OHOS_CaptureMouse;
    mouse->GetGlobalMouseState = OHOS_GetGlobalMouseState;
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
