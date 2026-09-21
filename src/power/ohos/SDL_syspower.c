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

#ifndef SDL_POWER_DISABLED
#ifdef SDL_POWER_OHOS

#include "../SDL_syspower.h"
#include "SDL_power.h"

#include <BasicServicesKit/ohbattery_info.h>

SDL_bool SDL_GetPowerInfo_OHOS(SDL_PowerState *state, int *seconds, int *percent)
{

    BatteryInfo_BatteryPluggedType plugged_type;

    // The Harmony Native interface is temporarily unable to retrieve the remaining time.
    *seconds = -1;
    *percent = -1;
    *state = SDL_POWERSTATE_UNKNOWN;

    *percent = (int) OH_BatteryInfo_GetCapacity();

    plugged_type = OH_BatteryInfo_GetPluggedType();

    switch (plugged_type) {
    case PLUGGED_TYPE_NONE:
        *state = SDL_POWERSTATE_ON_BATTERY;
        break;
    case PLUGGED_TYPE_AC:
    case PLUGGED_TYPE_USB:
    case PLUGGED_TYPE_WIRELESS:
        if (*percent >= 100) {
            *state = SDL_POWERSTATE_CHARGED;
        } else {
            *state = SDL_POWERSTATE_CHARGING;
        }
        break;
    case PLUGGED_TYPE_BUTT:
        *state = SDL_POWERSTATE_UNKNOWN;
    }
    return SDL_TRUE;
}

#endif // SDL_POWER_OHOS
#endif // SDL_POWER_DISABLED