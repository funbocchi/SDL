/*
    SDL_ohos_main.c, placed in the public domain by funbocchi  11/7/26
*/

#include "../../SDL_internal.h"

/* Include the SDL main definition header */
#include "SDL_main.h"

#include <dlfcn.h>
#include <pthread.h>

#include "../../core/ohos/SDL_ohos.h"

static char g_ohos_app_library[256] = "libmain.so";

void OHOS_Main_SetAppLibrary(const char *library)
{
    if (library && library[0]) {
        SDL_strlcpy(g_ohos_app_library, library, sizeof(g_ohos_app_library));
    }
}

static void *OHOS_StartMainThread(void *unused)
{
    static char prog[] = "app";
    char *argv[] = { prog, NULL };
    SDL_main_func p_main;
    void *handle;

    SDL_SetMainReady();

    handle = dlopen(g_ohos_app_library, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return NULL;
    }

    p_main = (SDL_main_func)dlsym(handle, "SDL_main");
    if (!p_main) {
        dlclose(handle);
        return NULL;
    }

    p_main(1, argv);
    dlclose(handle);
    return NULL;
}

void OHOS_Main_StartApp(void)
{
    static SDL_bool started = SDL_FALSE;
    pthread_t thread;
    int rc;

    if (started) {
        return;
    }
    started = SDL_TRUE;

    rc = pthread_create(&thread, NULL, OHOS_StartMainThread, NULL);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OHOS_Main_StartApp: pthread_create=%d", rc);
    if (rc == 0) {
        pthread_detach(thread);
    }
}

/* vi: set ts=4 sw=4 expandtab: */