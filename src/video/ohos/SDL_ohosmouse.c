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

#include <multimedia/image_framework/image/pixelmap_native.h>
#include <arkui/native_node.h>
#include <arkui/ui_input_event.h>
#include <arkui/native_type.h>

#include "../../events/SDL_events_c.h"
#include "SDL_ohosevents.h"
#include "SDL_ohoswindow.h"

/*
 * A mouse event has to carry the window, the device, the button or position and
 * the state together, which does not fit into the two data pointers of a SDL
 * user event, so the payload is heap allocated.
 *
 * Ownership: the producer frees the payload when SDL_PushEvent() rejects it,
 * and OHOS_Mouse_HandleEvent() frees it on every path it takes.
 */
typedef enum
{
    OHOS_MOUSE_SUBTYPE_MOVE = 0,
    OHOS_MOUSE_SUBTYPE_BUTTON,
    OHOS_MOUSE_SUBTYPE_WHEEL, /* reserved: ArkUI reports the wheel as an axis event */
    OHOS_MOUSE_SUBTYPE_RESET
} OHOS_MouseSubtype;

typedef struct OHOS_MousePayload
{
    Uint32 window_id;
    Sint32 subtype;
    Sint32 mouse_id;

    union
    {
        struct
        {
            Sint32 x, y;  /* window coordinates, in px */
            float dx, dy; /* raw hardware delta, in device counts */
        } move;
        struct
        {
            Sint32 x, y; /* window coordinates, in px */
            Sint32 button;
            Sint32 state;
        } button;
        struct
        {
            float dx, dy;
            Sint32 direction;
        } wheel;
    } u;
} OHOS_MousePayload;

/* Fractional part of the raw deltas that has not been reported yet.
   Only touched from the SDL thread. */
static float s_delta_accum_x = 0.0f;
static float s_delta_accum_y = 0.0f;

/*
 * ArkUI and SDL disagree on the button numbering: ArkUI has RIGHT = 2 and
 * MIDDLE = 3, while SDL has MIDDLE = 2 and RIGHT = 3, so this must never be
 * replaced by a plain cast.
 */
static Sint32 OHOS_MouseTool_ButtonFromArkUI(int32_t arkui_button)
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

/* Device ids are signed and may be negative or missing; SDL reserves
   (Uint32)-1 for touch generated mouse events, so never map onto that. */
static Sint32 OHOS_MouseTool_MouseIdFromArkUI(int32_t device_id)
{
    return (device_id > 0) ? device_id : 0;
}

/*
 * Runs on the ArkUI (UI) thread.
 *
 * SDL's mouse state is not thread safe, so nothing here may call into
 * SDL_SendMouse*(); the event is packed and validated on the SDL thread in
 * OHOS_Mouse_HandleEvent() instead.
 *
 * ArkUI_UIInputEvent is owned by the framework and only valid for the duration
 * of this callback, so every field needed is copied out here.
 */
static void OHOS_Mouse_HandleMouseEvent(ArkUI_NodeEvent *event)
{
    ArkUI_UIInputEvent *input_event;
    OHOS_MousePayload *payload;
    SDL_Event sdl_event;
    int32_t action;
    Uint32 window_id;

    if (g_ohos_event_base == (Uint32)-1) {
        return;
    }

    input_event = OH_ArkUI_NodeEvent_GetInputEvent(event);
    if (input_event == NULL) {
        return;
    }
SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
    "MOUSE cb: ev=%p type=%d userdata=%p action=%d",
    (void *)input_event,
    input_event ? (int)OH_ArkUI_UIInputEvent_GetType(input_event) : -1,
    OH_ArkUI_NodeEvent_GetUserData(event),
    input_event ? (int)OH_ArkUI_MouseEvent_GetMouseAction(input_event) : -1);
    if (OH_ArkUI_UIInputEvent_GetType(input_event) != ARKUI_UIINPUTEVENT_TYPE_MOUSE) {
        return;
    }

    window_id = (Uint32)(intptr_t)OH_ArkUI_NodeEvent_GetUserData(event);
    if (window_id == 0) {
        return;
    }

    action = OH_ArkUI_MouseEvent_GetMouseAction(input_event);

    payload = (OHOS_MousePayload *)SDL_calloc(1, sizeof(*payload));
    if (payload == NULL) {
        SDL_OutOfMemory();
        return;
    }
    payload->window_id = window_id;
    payload->mouse_id = OHOS_MouseTool_MouseIdFromArkUI(OH_ArkUI_UIInputEvent_GetDeviceId(input_event));

    switch (action) {
    case UI_MOUSE_EVENT_ACTION_MOVE:
        payload->subtype = OHOS_MOUSE_SUBTYPE_MOVE;
        payload->u.move.x = (Sint32)SDL_lroundf(1.9*OH_ArkUI_PointerEvent_GetX(input_event));
        payload->u.move.y = (Sint32)SDL_lroundf(1.9*OH_ArkUI_PointerEvent_GetY(input_event));
         SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "MOUSE raw x=%d y=%d",
                     (int)payload->u.move.x, (int)payload->u.move.y);
        /* The raw delta is the only relative value ArkUI provides, and it is
           what relative mouse mode has to use: the absolute position stops
           changing once the pointer is locked or reaches the window edge. */
        payload->u.move.dx = OH_ArkUI_MouseEvent_GetRawDeltaX(input_event);
        payload->u.move.dy = OH_ArkUI_MouseEvent_GetRawDeltaY(input_event);
        break;

    case UI_MOUSE_EVENT_ACTION_PRESS:
    case UI_MOUSE_EVENT_ACTION_RELEASE:
        payload->subtype = OHOS_MOUSE_SUBTYPE_BUTTON;
        payload->u.button.x = (Sint32)SDL_lroundf(1.9*OH_ArkUI_PointerEvent_GetX(input_event));
        payload->u.button.y = (Sint32)SDL_lroundf(1.9*OH_ArkUI_PointerEvent_GetY(input_event));
        payload->u.button.button = OHOS_MouseTool_ButtonFromArkUI(OH_ArkUI_MouseEvent_GetMouseButton(input_event));
        payload->u.button.state = (action == UI_MOUSE_EVENT_ACTION_PRESS) ? SDL_PRESSED : SDL_RELEASED;
        if (payload->u.button.button == 0) {
            /* UI_MOUSE_EVENT_BUTTON_NONE, or a button SDL does not model. */
            SDL_free(payload);
            return;
        }
        break;

    case UI_MOUSE_EVENT_ACTION_CANCEL:
        /* The gesture was cancelled: report whatever was pressed as released,
           otherwise the button would stay down in SDL forever. */
        payload->subtype = OHOS_MOUSE_SUBTYPE_RESET;
        break;

    default:
        SDL_free(payload);
        return;
    }

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_MOUSE;
    sdl_event.user.windowID = window_id;
    sdl_event.user.data1 = payload;

    if (SDL_PushEvent(&sdl_event) <= 0) {
        SDL_free(payload);
    }
}

/*
 * Runs on the SDL thread, dispatched from OHOS_PumpEvents().
 *
 * The event is only accepted when the window it was reported for still exists
 * and is not being destroyed. Mouse events are not gated on the keyboard focus,
 * because the pointer can hover any window and SDL tracks that separately
 * through SDL_SendMouseMotion().
 */
void OHOS_Mouse_HandleEvent(SDL_VideoDevice *_this, const SDL_Event *event)
{
    OHOS_MousePayload *payload = (OHOS_MousePayload *)event->user.data1;
    SDL_Window *window;
    SDL_MouseID mouse_id;
    int x, y;

    (void)_this;

    if (payload == NULL) {
        return;
    }

    window = SDL_GetWindowFromID(payload->window_id);
    if (window == NULL || window->is_destroying) {
        SDL_free(payload);
        return;
    }
    mouse_id = (SDL_MouseID)payload->mouse_id;

    switch (payload->subtype) {
    case OHOS_MOUSE_SUBTYPE_MOVE:
        if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
            /* Raw deltas can be smaller than one unit, so the remainder is
               carried over: truncating them would make a high resolution
               device look like it stopped moving. */
            int dx, dy;

            s_delta_accum_x += payload->u.move.dx;
            s_delta_accum_y += payload->u.move.dy;
            dx = (int)s_delta_accum_x;
            dy = (int)s_delta_accum_y;
            if (dx != 0 || dy != 0) {
                s_delta_accum_x -= (float)dx;
                s_delta_accum_y -= (float)dy;
                SDL_SendMouseMotion(window, mouse_id, 1, dx, dy);
            }
        } else {
            OHOS_WindowTool_ClientPointToSDL(window, &payload->u.move.x, &x);
            OHOS_WindowTool_ClientPointToSDL(window, &payload->u.move.y, &y);
            SDL_SendMouseMotion(window, mouse_id, 0, x, y);
        }
        break;

    case OHOS_MOUSE_SUBTYPE_BUTTON:
        SDL_SendMouseButton(window, mouse_id, (Uint8)payload->u.button.state,
                            (Uint8)payload->u.button.button);
        break;

    case OHOS_MOUSE_SUBTYPE_RESET: {
        Uint32 pressed = SDL_GetMouseState(NULL, NULL);
        int button;

        for (button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; ++button) {
            if (pressed & SDL_BUTTON(button)) {
                SDL_SendMouseButton(window, mouse_id, SDL_RELEASED, (Uint8)button);
            }
        }
        break;
    }

    case OHOS_MOUSE_SUBTYPE_WHEEL:
    default:
        break;
    }

    SDL_free(payload);
}

void OHOS_Mouse_RegisterEvent(ArkUI_NodeHandle handle, int32_t id)
{
    int32_t result;

    result = OH_ArkUI_NativeModule_RegisterCommonEvent(handle, NODE_ON_MOUSE,
                                                      (void *)(intptr_t)id,
                                                      OHOS_Mouse_HandleMouseEvent);
    if (result != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS_Mouse_RegisterEvent: registering mouse event failed (%d)",
                     (int)result);
    }
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
