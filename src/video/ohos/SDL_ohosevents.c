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

#include "SDL_ohosvideo.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "SDL_ohosevents.h"

static void OHOS_Tool_DrainWakeup(int fd)
{
    char buf[64];

    while (read(fd, buf, sizeof(buf)) > 0) {
    }
}

void OHOS_PumpEvents(_THIS)
{
}

void OHOS_SendWakeupEvent(_THIS, SDL_Window *window)
{
    SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;
    char byte = 0;

    (void)window;

    if (!data || !data->events_ready || data->wake_write < 0) {
        return;
    }

    (void)write(data->wake_write, &byte, 1);
}

int OHOS_WaitEventTimeout(_THIS, int timeout)
{
    SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;
    struct pollfd pfd;
    int ret;

    if (!data || !data->events_ready || data->wake_fd < 0) {
        return -1;
    }

    pfd.fd = data->wake_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    ret = poll(&pfd, 1, timeout);
    if (ret > 0) {
        OHOS_Tool_DrainWakeup(data->wake_fd);
        return 1;
    }
    if (ret == 0) {
        return 0;
    }
    if (errno == EINTR) {
        return 1;
    }
    return -1;
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */