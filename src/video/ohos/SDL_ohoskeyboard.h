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

#ifndef SDL_OHOSKEYBOARD_H
#define SDL_OHOSKEYBOARD_H

#include <arkui/common_type.h>

#include "../SDL_sysvideo.h"
#include "SDL_events.h"

extern void OHOS_Keyboard_RegisterEvent(ArkUI_NodeHandle handle, int32_t id);

extern void OHOS_Keyboard_HandleEvent(_THIS, const SDL_Event *event);

/* SDL Hooks */

extern SDL_bool OHOS_HasScreenKeyboardSupport(_THIS);

extern void OHOS_ShowScreenKeyboard(_THIS, SDL_Window *window);

extern void OHOS_HideScreenKeyboard(_THIS, SDL_Window *window);

extern SDL_bool OHOS_IsScreenKeyboardShown(_THIS, SDL_Window *window);

#endif /* SDL_OHOSKEYBOARD_H */

/* vi: set ts=4 sw=4 expandtab: */