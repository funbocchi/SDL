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
#include "SDL_video.h"

#ifndef SDL2_OHOS_SDL_OHOS_H
#define SDL2_OHOS_SDL_OHOS_H

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    OHOS_EVENT_WINDOW,
    OHOS_EVENT_GL,
    OHOS_EVENT_MOUSE,
    OHOS_EVENT_KEYBOARD,
    OHOS_EVENT_TOUCH,
    OHOS_EVENT_COUNT
} OHOSEventType;

typedef enum OHOS_MouseEventCode
{
    OHOS_MOUSE_EVENT_MOVE = 0,   /* data1 = {x, y}, data2 = {dx, dy} */
    OHOS_MOUSE_EVENT_BUTTON,     /* data1 = button, data2 = state */
    OHOS_MOUSE_EVENT_CANCEL      /* no payload, every pressed button is released */
} OHOS_MouseEventCode;

/* Packs two floats into one pointer-sized slot; only valid where void * is 64 bits. */
typedef union OHOS_FloatPair
{
    float  f[2];
    Uint64 u;
} OHOS_FloatPair;

typedef enum OHOS_KeyboardEventCode
{
    OHOS_KEYBOARD_EVENT_KEY = 0   /* data1 = key code, data2 = ArkUI_KeyEventType */
} OHOS_KeyboardEventCode;

typedef enum OHOS_TouchEventCode
{
    OHOS_TOUCH_EVENT_POINT = 0    /* data1 = {x, y} px, data2 = action << 32 | finger id */
} OHOS_TouchEventCode;

typedef enum OHOS_GLSurfaceEventCode
{
    OHOS_GL_EVENT_SURFACE_CREATED = 0,
    OHOS_GL_EVENT_SURFACE_DESTROYED,
    OHOS_GL_EVENT_SURFACE_CHANGED,
    OHOS_GL_EVENT_SURFACE_SHOWN,
    OHOS_GL_EVENT_SURFACE_HIDDEN
} OHOS_GLSurfaceEventCode;

typedef enum OHOS_WindowEventCode
{
    OHOS_WINDOW_EVENT_STATE_CHANGED = 0,      /* windowEvent,            data1 = WindowEventType */
    OHOS_WINDOW_EVENT_RECT_CHANGED,           /* windowRectChange,       data1 = SDL_Rect *, data2 = displayId */
    OHOS_WINDOW_EVENT_STATUS_CHANGED,         /* windowStatusChange,     data1 = WindowStatusType */
    OHOS_WINDOW_EVENT_DISPLAY_CHANGED,        /* displayIdChange,        data1 = displayId */
    OHOS_WINDOW_EVENT_VISIBILITY_CHANGED,     /* windowVisibilityChange, data1 = visible */
    OHOS_WINDOW_EVENT_FOCUS_CHANGED,          /* XComponent focus/blur,  data1 = SDL_TRUE when focused */
    OHOS_WINDOW_EVENT_ID_ASSIGNED             /* window obtained,        data1 = OHOS window id */
} OHOS_WindowEventCode;


/* Copies the cached system locale, "language_REGION"; safe from any SDL thread. */
extern void OHOS_Locale_GetPreferredLocales(char *buf, size_t buflen);

/* SDL_Window related operations */

extern int   OHOS_Window_Create          (Uint32 sdl_window_id, SDL_bool bordered, int width, int height, int pos_x, int pos_y, SDL_bool resizable);
extern void  OHOS_Window_Destroy         (Uint32 sdl_window_id);
extern void  OHOS_Window_Show            (Uint32 sdl_window_id);
extern void  OHOS_Window_Hide            (Uint32 sdl_window_id);
extern void  OHOS_Window_SetSize         (Uint32 sdl_window_id, int width, int height);
extern void  OHOS_Window_SetPos          (Uint32 sdl_window_id, int pos_x, int pos_y);
extern void  OHOS_Window_SetTitle        (Uint32 sdl_window_id, const char *title);
extern void  OHOS_Window_SetBordered     (Uint32 sdl_window_id, SDL_bool bordered, int width, int height, int pos_x, int pos_y);
extern void  OHOS_Window_SetMinimumSize  (Uint32 sdl_window_id, int width, int height);
extern void  OHOS_Window_SetMaximumSize  (Uint32 sdl_window_id, int width, int height);
extern int   OHOS_Window_SetOpacity      (Uint32 sdl_window_id, float opacity);
extern int   OHOS_Window_SetModalFor     (Uint32 modal_window_id, Uint32 parent_window_id);
extern int   OHOS_Window_SetInputFocus   (Uint32 sdl_window_id);
extern void  OHOS_Window_Raise           (Uint32 sdl_window_id);
extern void  OHOS_Window_Maximize        (Uint32 sdl_window_id);
extern void  OHOS_Window_Minimize        (Uint32 sdl_window_id);
extern void  OHOS_Window_Restore         (Uint32 sdl_window_id);
extern void  OHOS_Window_SetResizable    (Uint32 sdl_window_id, SDL_bool resizable);
extern void  OHOS_Window_SetAlwaysOnTop  (Uint32 sdl_window_id, SDL_bool on_top);
extern void  OHOS_Window_SetFullscreen   (Uint32 sdl_window_id, SDL_bool fullscreen);
extern void  OHOS_Window_SetMouseGrab    (Uint32 sdl_window_id, SDL_bool grabbed);
extern void  OHOS_Window_SetKeyboardGrab (Uint32 sdl_window_id, SDL_bool grabbed);
extern int   OHOS_Window_SetGammaRamp    (Uint32 sdl_window_id, const Uint16 *ramp);
extern int   OHOS_Window_GetGammaRamp    (Uint32 sdl_window_id, Uint16 *ramp);
extern void *OHOS_Window_GetICCProfile   (Uint32 sdl_window_id, size_t *size);
extern void  OHOS_Window_SetMouseRect    (Uint32 sdl_window_id);
extern int   OHOS_Window_Flash           (Uint32 sdl_window_id, SDL_FlashOperation operation);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif // SDL2_OHOS_SDL_OHOS_H
