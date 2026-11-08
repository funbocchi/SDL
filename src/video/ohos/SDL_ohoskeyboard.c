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
#include "../../events/SDL_events_c.h"
#include "SDL_ohosevents.h"
#include "SDL_ohoswindow.h"

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

static int OHOS_KeyboardTool_KeyEventTypeToState(ArkUI_KeyEventType key_event_type)
{
    switch (key_event_type) {
    case ARKUI_KEY_EVENT_DOWN:
    case ARKUI_KEY_EVENT_LONG_PRESS:
        return SDL_PRESSED;
    case ARKUI_KEY_EVENT_UP:
        return SDL_RELEASED;
    case ARKUI_KEY_EVENT_CLICK:
    case ARKUI_KEY_EVENT_UNKNOWN:
    default:
        return -1;
    }
}

static void OHOS_Keyboard_HandleKeyEvent(ArkUI_NodeEvent *event)
{
    ArkUI_UIInputEvent *input_event;
    ArkUI_KeyEventType event_type;
    ArkUI_KeySourceType source_type;
    int32_t key_code;
    int state;
    Uint32 id;
    SDL_Event sdl_event;

    input_event = OH_ArkUI_NodeEvent_GetInputEvent(event);
    if (input_event == NULL) {
        return;
    }
    OH_ArkUI_UIInputEvent_GetType(input_event);
    source_type = OH_ArkUI_KeyEvent_GetKeySource(input_event);

    event_type = OH_ArkUI_KeyEvent_GetType(input_event);
    state = OHOS_KeyboardTool_KeyEventTypeToState(event_type);
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
    sdl_event.user.code = (Sint32)source_type;
    sdl_event.user.data1 = (void *)(intptr_t)key_code;
    sdl_event.user.data2  = (void *)(intptr_t)state;

    SDL_PushEvent(&sdl_event);
}

void OHOS_Keyboard_RegisterEvent(ArkUI_NodeHandle handle, int32_t id){
    int32_t result;
    
    result = OH_ArkUI_NativeModule_RegisterCommonEvent(handle, NODE_ON_KEY_EVENT, (void *)(intptr_t)id, OHOS_Keyboard_HandleKeyEvent);
    if (result != ARKUI_ERROR_CODE_NO_ERROR)
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "key register failed %d", (int)result);
}

void OHOS_Keyboard_HandleEvent(SDL_VideoDevice *_this, const SDL_Event *event){
    SDL_Scancode scancode;
    int state;

    scancode = (SDL_Scancode)(intptr_t)event->user.data1;
    state = (int)(intptr_t)event->user.data2;
    SDL_SendKeyboardKey(state, scancode);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "OHOS key: window=%u scancode=%d(%s) state=%s", event->user.windowID, (int)scancode, SDL_GetScancodeName(scancode), (state == SDL_PRESSED) ? "down" : "up");
}

/* SDL hooks */

SDL_bool OHOS_HasScreenKeyboardSupport(SDL_VideoDevice *_this){
    /* TODO: 待完成逻辑验证后补充 */
    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
