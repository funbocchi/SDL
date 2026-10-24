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

#ifndef SDL_OHOSGLES_H
#define SDL_OHOSGLES_H

#if defined(SDL_VIDEO_DRIVER_OHOS) && defined(SDL_VIDEO_OPENGL_EGL)

#include "../SDL_sysvideo.h"

/* Hooks */

extern int OHOS_GL_LoadLibrary(_THIS, const char *path);

extern void *OHOS_GL_GetProcAddress(_THIS, const char *proc);

extern void OHOS_GL_UnloadLibrary(_THIS);

extern SDL_GLContext OHOS_GL_CreateContext(_THIS, SDL_Window *window);

extern int OHOS_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context);

extern void OHOS_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h);

extern int OHOS_GL_SetSwapInterval(_THIS, int interval);

extern int OHOS_GL_GetSwapInterval(_THIS);

extern int OHOS_GL_SwapWindow(_THIS, SDL_Window *window);

extern void OHOS_GL_DeleteContext(_THIS, SDL_GLContext context);

extern void OHOS_GL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor);

/* Private */

/**
* @brief Complete the creation of the surface
*/
extern int OHOS_GL_PreparePendingSurface(_THIS, SDL_Window *window);

extern int OHOS_GL_ActivatePendingSurface(_THIS, SDL_Window *window);

extern int OHOS_GL_DeactivatePendingSurface(_THIS, SDL_Window *window);

#endif

#endif