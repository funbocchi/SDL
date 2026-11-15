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

#include "SDL_config.h"

#ifdef SDL_SENSOR_OHOS

#include "SDL_error.h"
#include "SDL_sensor.h"
#include "SDL_ohossensor.h"
#include "../SDL_syssensor.h"
#include "../SDL_sensor_c.h"

#include <sensors/oh_sensor.h>
#include <sensors/oh_sensor_type.h>

#define OHOS_SENSOR_MAX       8
#define OHOS_SENSOR_INFO_MAX 32

typedef struct
{
    Sensor_Info *info;
    Sensor_Type type;
    SDL_SensorType sdl_type;
    SDL_SensorID instance_id;
    char name[64];
} OHOS_Sensor;

static OHOS_Sensor g_ohos_sensors[OHOS_SENSOR_MAX];
static int g_ohos_sensor_count = 0;

/* Owns the Sensor_Info array the entries above point into. */
static Sensor_Info **g_ohos_sensor_infos = NULL;
static uint32_t g_ohos_sensor_info_count = 0;

/* The system callback carries no user data, so opened sensors are found by type. */
static SDL_Sensor *g_ohos_open_sensors[OHOS_SENSOR_MAX];
static Sensor_Type g_ohos_open_types[OHOS_SENSOR_MAX];

/* Only these two have an SDL equivalent, the rest are not enumerated. */
static SDL_SensorType OHOS_SensorTypeToSDL(Sensor_Type type)
{
    switch (type) {
    case SENSOR_TYPE_ACCELEROMETER:
        return SDL_SENSOR_ACCEL;
    case SENSOR_TYPE_GYROSCOPE:
        return SDL_SENSOR_GYRO;
    default:
        return SDL_SENSOR_INVALID;
    }
}

static int OHOS_Sensor_Init(void)
{
    uint32_t count = 0;
    uint32_t i;

    g_ohos_sensor_count = 0;
    SDL_zeroa(g_ohos_sensors);
    SDL_zeroa(g_ohos_open_sensors);
    SDL_zeroa(g_ohos_open_types);

    if (g_ohos_sensor_infos != NULL) {
        OH_Sensor_DestroyInfos(g_ohos_sensor_infos, g_ohos_sensor_info_count);
        g_ohos_sensor_infos = NULL;
        g_ohos_sensor_info_count = 0;
    }

    /* The array has to exist before the call, so it is sized for any device. */
    g_ohos_sensor_infos = OH_Sensor_CreateInfos(OHOS_SENSOR_INFO_MAX);
    if (g_ohos_sensor_infos == NULL) {
        return SDL_OutOfMemory();
    }

    if (OH_Sensor_GetInfos(g_ohos_sensor_infos, &count) != SENSOR_SUCCESS) {
        OH_Sensor_DestroyInfos(g_ohos_sensor_infos, OHOS_SENSOR_INFO_MAX);
        g_ohos_sensor_infos = NULL;
        return SDL_SetError("OH_Sensor_GetInfos failed");
    }
    g_ohos_sensor_info_count = count;

    for (i = 0; i < count && g_ohos_sensor_count < OHOS_SENSOR_MAX; ++i) {
        OHOS_Sensor *entry = &g_ohos_sensors[g_ohos_sensor_count];
        Sensor_Type type;
        char name[sizeof(entry->name)];
        uint32_t length = sizeof(name);

        if (OH_SensorInfo_GetType(g_ohos_sensor_infos[i], &type) != SENSOR_SUCCESS) {
            continue;
        }
        entry->sdl_type = OHOS_SensorTypeToSDL(type);
        if (entry->sdl_type == SDL_SENSOR_INVALID) {
            continue;
        }

        SDL_zeroa(name);
        if (OH_SensorInfo_GetName(g_ohos_sensor_infos[i], name, &length) != SENSOR_SUCCESS) {
            continue;
        }

        entry->info = g_ohos_sensor_infos[i];
        entry->type = type;
        entry->instance_id = SDL_GetNextSensorInstanceID();
        SDL_strlcpy(entry->name, name, sizeof(entry->name));
        ++g_ohos_sensor_count;
    }

    return 0;
}

static int OHOS_Sensor_GetCount(void)
{
    return g_ohos_sensor_count;
}

static void OHOS_Sensor_Detect(void)
{
    /* The set of sensors does not change while the application runs. */
}

static const char *OHOS_Sensor_GetDeviceName(int device_index)
{
    if (device_index < 0 || device_index >= g_ohos_sensor_count) {
        return NULL;
    }
    return g_ohos_sensors[device_index].name;
}

static SDL_SensorType OHOS_Sensor_GetDeviceType(int device_index)
{
    if (device_index < 0 || device_index >= g_ohos_sensor_count) {
        return SDL_SENSOR_INVALID;
    }
    return g_ohos_sensors[device_index].sdl_type;
}

static int OHOS_Sensor_GetDeviceNonPortableType(int device_index)
{
    if (device_index < 0 || device_index >= g_ohos_sensor_count) {
        return -1;
    }
    return (int)g_ohos_sensors[device_index].type;
}

static SDL_SensorID OHOS_Sensor_GetDeviceInstanceID(int device_index)
{
    if (device_index < 0 || device_index >= g_ohos_sensor_count) {
        return -1;
    }
    return g_ohos_sensors[device_index].instance_id;
}

static void OHOS_Sensor_OnEvent(Sensor_Event *event)
{
    Sensor_Type type;
    int64_t timestamp = 0;
    float *data = NULL;
    uint32_t length = 0;
    int i;

    if (OH_SensorEvent_GetType(event, &type) != SENSOR_SUCCESS) {
        return;
    }

    for (i = 0; i < OHOS_SENSOR_MAX; ++i) {
        if (g_ohos_open_sensors[i] != NULL && g_ohos_open_types[i] == type) {
            if (OH_SensorEvent_GetTimestamp(event, &timestamp) != SENSOR_SUCCESS) {
                return;
            }
            if (OH_SensorEvent_GetData(event, &data, &length) != SENSOR_SUCCESS) {
                return;
            }
            SDL_PrivateSensorUpdate(g_ohos_open_sensors[i], (Uint64)timestamp, data, (int)length);
            return;
        }
    }
}

static int OHOS_Sensor_Open(SDL_Sensor *sensor, int device_index)
{
    struct sensor_hwdata *hwdata;
    int i;

    if (device_index < 0 || device_index >= g_ohos_sensor_count) {
        return SDL_SetError("Invalid sensor index");
    }

    hwdata = (struct sensor_hwdata *)SDL_calloc(1, sizeof(struct sensor_hwdata));
    if (hwdata == NULL) {
        return SDL_OutOfMemory();
    }
    hwdata->type = g_ohos_sensors[device_index].type;

    hwdata->id = OH_Sensor_CreateSubscriptionId();
    hwdata->attribute = OH_Sensor_CreateSubscriptionAttribute();
    hwdata->subscriber = OH_Sensor_CreateSubscriber();
    if (hwdata->id == NULL || hwdata->attribute == NULL || hwdata->subscriber == NULL) {
        SDL_free(hwdata);
        return SDL_SetError("Couldn't create the sensor subscription objects");
    }

    OH_SensorSubscriptionId_SetType(hwdata->id, hwdata->type);
    OH_SensorSubscriber_SetCallback(hwdata->subscriber, OHOS_Sensor_OnEvent);

    if (OH_Sensor_Subscribe(hwdata->id, hwdata->attribute, hwdata->subscriber) != SENSOR_SUCCESS) {
        OH_Sensor_DestroySubscriptionId(hwdata->id);
        OH_Sensor_DestroySubscriptionAttribute(hwdata->attribute);
        OH_Sensor_DestroySubscriber(hwdata->subscriber);
        SDL_free(hwdata);
        return SDL_SetError("OH_Sensor_Subscribe failed");
    }

    for (i = 0; i < OHOS_SENSOR_MAX; ++i) {
        if (g_ohos_open_sensors[i] == NULL) {
            g_ohos_open_sensors[i] = sensor;
            g_ohos_open_types[i] = hwdata->type;
            break;
        }
    }

    sensor->hwdata = hwdata;
    return 0;
}

static void OHOS_Sensor_Update(SDL_Sensor *sensor)
{
    /* The system pushes samples through the callback, there is nothing to poll. */
    (void)sensor;
}

static void OHOS_Sensor_Close(SDL_Sensor *sensor)
{
    struct sensor_hwdata *hwdata = sensor->hwdata;
    int i;

    if (hwdata == NULL) {
        return;
    }

    for (i = 0; i < OHOS_SENSOR_MAX; ++i) {
        if (g_ohos_open_sensors[i] == sensor) {
            g_ohos_open_sensors[i] = NULL;
            break;
        }
    }

    OH_Sensor_Unsubscribe(hwdata->id, hwdata->subscriber);
    OH_Sensor_DestroySubscriptionId(hwdata->id);
    OH_Sensor_DestroySubscriptionAttribute(hwdata->attribute);
    OH_Sensor_DestroySubscriber(hwdata->subscriber);
    SDL_free(hwdata);
    sensor->hwdata = NULL;
}

static void OHOS_Sensor_Quit(void)
{
    g_ohos_sensor_count = 0;
    SDL_zeroa(g_ohos_sensors);
    SDL_zeroa(g_ohos_open_sensors);
    SDL_zeroa(g_ohos_open_types);

    if (g_ohos_sensor_infos != NULL) {
        OH_Sensor_DestroyInfos(g_ohos_sensor_infos, g_ohos_sensor_info_count);
        g_ohos_sensor_infos = NULL;
        g_ohos_sensor_info_count = 0;
    }
}

SDL_SensorDriver SDL_OHOS_SensorDriver = {
    OHOS_Sensor_Init,
    OHOS_Sensor_GetCount,
    OHOS_Sensor_Detect,
    OHOS_Sensor_GetDeviceName,
    OHOS_Sensor_GetDeviceType,
    OHOS_Sensor_GetDeviceNonPortableType,
    OHOS_Sensor_GetDeviceInstanceID,
    OHOS_Sensor_Open,
    OHOS_Sensor_Update,
    OHOS_Sensor_Close,
    OHOS_Sensor_Quit,
};

#endif /* SDL_SENSOR_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
