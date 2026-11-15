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

#include "SDL_ohostouch.h"

#include "../../events/SDL_touch_c.h"

#include "../../core/ohos/SDL_ohos.h"

/* Only one touchscreen is reported; the id just has to stay stable. */
#define OHOS_TOUCH_ID ((SDL_TouchID)1)

/* ArkUI_UIInputEvent action values. */
#define OHOS_TOUCH_ACTION_CANCEL 0
#define OHOS_TOUCH_ACTION_DOWN   1
#define OHOS_TOUCH_ACTION_MOVE   2
#define OHOS_TOUCH_ACTION_UP     3

void OHOS_Touch_HandleEvent(_THIS, const SDL_Event *event)
{
    SDL_Window *window;
    OHOS_FloatPair position;
    Uint64 packed;
    int32_t action;
    SDL_FingerID finger_id;
    float x, y;

    (void)_this;

    window = SDL_GetWindowFromID(event->user.windowID);
    if (window == NULL) {
        return;
    }

    position.u = (Uint64)(intptr_t)event->user.data1;
    packed = (Uint64)(intptr_t)event->user.data2;
    action = (int32_t)(packed >> 32);
    finger_id = (SDL_FingerID)(packed & 0xFFFFFFFF);

    /* The payload is in pixels; SDL normalises touch coordinates to the window. */
    x = (window->w > 0) ? (position.f[0] / (float)window->w) : 0.0f;
    y = (window->h > 0) ? (position.f[1] / (float)window->h) : 0.0f;

    SDL_AddTouch(OHOS_TOUCH_ID, SDL_TOUCH_DEVICE_DIRECT, "OHOS touchscreen");

    switch (action) {
    case OHOS_TOUCH_ACTION_DOWN:
        SDL_SendTouch(OHOS_TOUCH_ID, finger_id, window, SDL_TRUE, x, y, 1.0f);
        break;
    case OHOS_TOUCH_ACTION_MOVE:
        SDL_SendTouchMotion(OHOS_TOUCH_ID, finger_id, window, x, y, 1.0f);
        break;
    case OHOS_TOUCH_ACTION_UP:
    case OHOS_TOUCH_ACTION_CANCEL:
        /* SDL has no cancel, so a cancelled touch is reported as lifted. */
        SDL_SendTouch(OHOS_TOUCH_ID, finger_id, window, SDL_FALSE, x, y, 1.0f);
        break;
    default:
        break;
    }
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
