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

#ifndef SDL_OHOSEVENTS_H
#define SDL_OHOSEVENTS_H

#include "../SDL_sysvideo.h"

typedef enum
{
    OHOS_EVENT_WINDOW,
    OHOS_EVENT_GL,
    OHOS_EVENT_COUNT
} OHOSEventType;

/* Hook */

extern void OHOS_PumpEvents(_THIS);

extern void OHOS_SendWakeupEvent(_THIS, SDL_Window *window);

extern int OHOS_WaitEventTimeout(_THIS, int timeout);

/* Private */
extern void OHOS_Event_Register(void);

extern Uint32 g_ohos_event_base;

#endif /* SDL_OHOSEVENTS_H */

/* vi: set ts=4 sw=4 expandtab: */