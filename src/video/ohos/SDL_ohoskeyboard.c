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

#include "../../events/scancodes_ohos.h"
#include "../../events/SDL_keyboard_c.h"

#include "../../core/ohos/SDL_ohos.h"

/* The ArkUI key codes inherit Android's numbering, so the table is indexed by
   (key_code - OHOS_KEYCODE_KEYPAD_BASE) over the 2000..2122 range. */
static SDL_Scancode OHOS_KeyboardTool_KeyCodeToScancode(int32_t key_code)
{
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
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

static void OHOS_Keyboard_HandleKey(_THIS, const SDL_Event *event)
{
    SDL_Scancode scancode;
    int32_t key_code;
    Uint8 state;

    (void)_this;

    key_code = (int32_t)(intptr_t)event->user.data1;
    scancode = OHOS_KeyboardTool_KeyCodeToScancode(key_code);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    state = ((intptr_t)event->user.data2 == ARKUI_KEY_EVENT_UP) ? SDL_RELEASED : SDL_PRESSED;

    SDL_SendKeyboardKey(state, scancode);
}

void OHOS_Keyboard_HandleEvent(_THIS, const SDL_Event *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->user.code) {
    case OHOS_KEYBOARD_EVENT_KEY:
        OHOS_Keyboard_HandleKey(_this, event);
        break;
    default:
        break;
    }
}

void OHOS_StartTextInput(_THIS)
{
    (void)_this;
}

void OHOS_StopTextInput(_THIS)
{
    (void)_this;
}

void OHOS_SetTextInputRect(_THIS, const SDL_Rect *rect)
{
    (void)_this;
    (void)rect;
}

SDL_bool OHOS_HasScreenKeyboardSupport(_THIS)
{
    (void)_this;

    return SDL_FALSE;
}

void OHOS_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

void OHOS_HideScreenKeyboard(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

SDL_bool OHOS_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;

    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
