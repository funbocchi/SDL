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

#ifndef SDL_OHOSMOUSE_H
#define SDL_OHOSMOUSE_H

#include "../SDL_sysvideo.h"
#include "SDL_events.h"

/* Consumes the OHOS_EVENT_MOUSE user events on the SDL thread. */
extern void OHOS_Mouse_HandleEvent(_THIS, const SDL_Event *event);

/* SDL_Mouse hooks */

extern void        OHOS_MouseInit(void);

extern SDL_Cursor *OHOS_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y);
extern SDL_Cursor *OHOS_CreateSystemCursor(SDL_SystemCursor id);
extern int         OHOS_ShowCursor(SDL_Cursor *cursor);
extern void        OHOS_MoveCursor(SDL_Cursor *cursor);
extern void        OHOS_FreeCursor(SDL_Cursor *cursor);
extern void        OHOS_WarpMouse(SDL_Window *window, int x, int y);
extern int         OHOS_WarpMouseGlobal(int x, int y);
extern int         OHOS_SetRelativeMouseMode(SDL_bool enabled);
extern int         OHOS_CaptureMouse(SDL_Window *window);
extern Uint32      OHOS_GetGlobalMouseState(int *x, int *y);

#endif /* SDL_OHOSMOUSE_H */

/* vi: set ts=4 sw=4 expandtab: */
