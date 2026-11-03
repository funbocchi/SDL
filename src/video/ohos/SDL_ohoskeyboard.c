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

#include "SDL_ohoskeyboard.h"

#include <arkui/native_key_event.h>

#include "../../events/SDL_events_c.h"
#include "../../events/scancodes_ohos.h"
#include "SDL_ohosevents.h"
#include "SDL_ohoswindow.h"

/* The table itself lives in src/events/scancodes_ohos.h, next to the other
   platform scancode tables. */
SDL_COMPILE_TIME_ASSERT(OHOS_keycodes_keypad_size,
                        SDL_arraysize(ohos_keypad_scancode_table) ==
                            OHOS_KEYCODE_KEYPAD_MAX - OHOS_KEYCODE_KEYPAD_BASE + 1);

/* The key codes below 2000 are sparse, so they are handled by a switch. */
static SDL_Scancode OHOS_KeyboardTool_KeyCodeToScancode(int32_t key_code){
    if (key_code >= OHOS_KEYCODE_KEYPAD_BASE && key_code <= OHOS_KEYCODE_KEYPAD_MAX) {
        return ohos_keypad_scancode_table[key_code - OHOS_KEYCODE_KEYPAD_BASE];
    }

    switch (key_code) {
    case ARKUI_KEYCODE_VOLUME_UP:
        return SDL_SCANCODE_VOLUMEUP;
    case ARKUI_KEYCODE_VOLUME_DOWN:
        return SDL_SCANCODE_VOLUMEDOWN;
    case ARKUI_KEYCODE_POWER:
        return SDL_SCANCODE_POWER;
    case ARKUI_KEYCODE_VOLUME_MUTE:
        return SDL_SCANCODE_MUTE;
    case ARKUI_KEYCODE_MUTE:
        return SDL_SCANCODE_AUDIOMUTE;
    case ARKUI_KEYCODE_BRIGHTNESS_UP:
        return SDL_SCANCODE_BRIGHTNESSUP;
    case ARKUI_KEYCODE_BRIGHTNESS_DOWN:
        return SDL_SCANCODE_BRIGHTNESSDOWN;
    /* ARKUI_KEYCODE_FN and ARKUI_KEYCODE_CAMERA have no SDL equivalent. */
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

/* ArkUI key event type to SDL key state; -1 means "do not forward". */
static int OHOS_KeyboardTool_KeyEventTypeToState(ArkUI_KeyEventType key_event_type)
{
    switch (key_event_type) {
    case ARKUI_KEY_EVENT_DOWN:
        return SDL_PRESSED;
    case ARKUI_KEY_EVENT_LONG_PRESS:
        /* Auto-repeat: SDL sets the repeat flag itself when the key is held. */
        return SDL_PRESSED;
    case ARKUI_KEY_EVENT_UP:
        return SDL_RELEASED;
    case ARKUI_KEY_EVENT_CLICK:
    case ARKUI_KEY_EVENT_UNKNOWN:
    default:
        return -1;
    }
}

/*
 * Runs on the ArkUI (UI) thread.
 *
 * SDL's keyboard state is not thread safe, so nothing here may call into
 * SDL_SendKeyboardKey*(); the event is packed into a user event and validated
 * on the SDL thread in OHOS_Keyboard_HandleEvent() instead.
 *
 * ArkUI_UIInputEvent is owned by the framework and is only valid for the
 * duration of this callback, so the fields needed are copied out here.
 */
static void OHOS_Keyboard_HandleKeyEvent(ArkUI_NodeEvent *event)
{
    ArkUI_UIInputEvent *input_event;
    ArkUI_KeyEventType key_event_type;
    int32_t key_code;
    int state;
    Uint32 id;
    SDL_Event sdl_event;

    if (g_ohos_event_base == (Uint32)-1) {
        return;
    }

    input_event = OH_ArkUI_NodeEvent_GetInputEvent(event);
    if (input_event == NULL) {
        return;
    }
    if (OH_ArkUI_UIInputEvent_GetType(input_event) != ARKUI_UIINPUTEVENT_TYPE_KEY) {
        return;
    }
    if (OH_ArkUI_KeyEvent_GetKeySource(input_event) != ARKUI_KEY_SOURCE_TYPE_KEYBOARD) {
        return;
    }

    key_event_type = OH_ArkUI_KeyEvent_GetType(input_event);
    state = OHOS_KeyboardTool_KeyEventTypeToState(key_event_type);
    if (state < 0) {
        return;
    }

    key_code = OH_ArkUI_KeyEvent_GetKeyCode(input_event);
    if (key_code == ARKUI_KEYCODE_UNKNOWN) {
        return;
    }

    id = (Uint32)(intptr_t)OH_ArkUI_NodeEvent_GetUserData(event);
    if (id == 0) {
        return;
    }

    SDL_zero(sdl_event);
    sdl_event.type = g_ohos_event_base + OHOS_EVENT_KEYBOARD;
    sdl_event.user.windowID = id;
    sdl_event.user.code = state;
    sdl_event.user.data1 = (void *)(intptr_t)key_code;

    SDL_PushEvent(&sdl_event);
}

void OHOS_Keyboard_RegisterEvent(ArkUI_NodeHandle handle, int32_t id)
{
    int32_t result;

    result = OH_ArkUI_NativeModule_RegisterCommonEvent(handle, NODE_ON_KEY_EVENT,
                                                      (void *)(intptr_t)id,
                                                      OHOS_Keyboard_HandleKeyEvent);
    if (result != ARKUI_ERROR_CODE_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "OHOS_Keyboard_RegisterEvent: registering key event failed (%d)",
                     (int)result);
    }
}

/*
 * Runs on the SDL thread, dispatched from OHOS_PumpEvents().
 *
 * The event is only accepted when the window it was reported for still exists,
 * is not being destroyed, and is the window SDL currently gives keyboard focus
 * to. SDL attributes key events to the focus window, so accepting an event from
 * any other window would report it against the wrong one.
 */
void OHOS_Keyboard_HandleEvent(SDL_VideoDevice *_this, const SDL_Event *event)
{
    SDL_Window *window;
    SDL_Scancode scancode;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL || window->is_destroying) {
        return;
    }
    if (SDL_GetKeyboardFocus() != window) {
        return;
    }

    scancode = OHOS_KeyboardTool_KeyCodeToScancode((int32_t)(intptr_t)event->user.data1);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    SDL_SendKeyboardKeyAndKeycode((Uint8)event->user.code, scancode, SDLK_UNKNOWN);
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
