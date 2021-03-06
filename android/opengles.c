/* Copyright (C) 2011 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

#include "config-host.h"
#include "android/opengles.h"
#include <android/utils/debug.h>
#include <android/utils/path.h>
#include <android/utils/bufprint.h>
#include <android/utils/dll.h>
#include <stdio.h>
#include <stdlib.h>

#define D(...)  VERBOSE_PRINT(init,__VA_ARGS__)
#define DD(...) VERBOSE_PRINT(gles,__VA_ARGS__)

/* Name of the GLES rendering library we're going to use */
#define RENDERER_LIB_NAME  "libOpenglRender"

/* These definitions *must* match those under:
 * development/tools/emulator/opengl/host/include/libOpenglRender/render_api.h
 */
#define DYNLINK_FUNCTIONS  \
  DYNLINK_FUNC(int,initLibrary,(void),(),return) \
  DYNLINK_FUNC(int,initOpenGLRenderer,(int width, int height, int port),(width,height,port),return) \
  DYNLINK_FUNC(int,createOpenGLSubwindow,(void* window, int x, int y, int width, int height, float zRot),(window,x,y,width,height,zRot),return)\
  DYNLINK_FUNC(int,destroyOpenGLSubwindow,(void),(),return)\
  DYNLINK_FUNC(void,repaintOpenGLDisplay,(void),(),)\
  DYNLINK_FUNC(void,stopOpenGLRenderer,(void),(),)


#ifndef CONFIG_STANDALONE_UI
/* Defined in android/hw-pipe-net.c */
extern int android_init_opengles_pipes(void);
#endif

static ADynamicLibrary*  rendererLib;

/* Define the pointers and the wrapper functions to call them */
#define DYNLINK_FUNC(result,name,sig,params,ret) \
    static result (*_ptr_##name) sig; \
    static result name sig { \
        ret (*_ptr_##name) params ; \
    }

DYNLINK_FUNCTIONS

#undef DYNLINK_FUNC

static int
initOpenglesEmulationFuncs(ADynamicLibrary* rendererLib)
{
    void*  symbol;
    char*  error;
#define DYNLINK_FUNC(result,name,sig,params,ret) \
    symbol = adynamicLibrary_findSymbol( rendererLib, #name, &error ); \
    if (symbol != NULL) { \
        _ptr_##name = symbol; \
    } else { \
        derror("GLES emulation: Could not find required symbol (%s): %s", #name, error); \
        free(error); \
        return -1; \
    }
DYNLINK_FUNCTIONS
#undef DYNLINK_FUNC
    return 0;
}

int
android_initOpenglesEmulation(void)
{
    char* error = NULL;

    if (rendererLib != NULL)
        return 0;

    D("Initializing hardware OpenGLES emulation support");

    rendererLib = adynamicLibrary_open(RENDERER_LIB_NAME, &error);
    if (rendererLib == NULL) {
        derror("Could not load OpenGLES emulation library: %s", error);
        return -1;
    }

#ifndef CONFIG_STANDALONE_UI
    android_init_opengles_pipes();
#endif


    /* Resolve the functions */
    if (initOpenglesEmulationFuncs(rendererLib) < 0) {
        derror("OpenGLES emulation library mismatch. Be sure to use the correct version!");
        goto BAD_EXIT;
    }

    if (!initLibrary()) {
        derror("OpenGLES initialization failed!");
        goto BAD_EXIT;
    }

    return 0;

BAD_EXIT:
    derror("OpenGLES emulation library could not be initialized!");
    adynamicLibrary_close(rendererLib);
    rendererLib = NULL;
    return -1;
}

int
android_startOpenglesRenderer(int width, int height)
{
    if (!rendererLib) {
        D("Can't start OpenGLES renderer without support libraries");
        return -1;
    }

    if (initOpenGLRenderer(width, height,ANDROID_OPENGLES_BASE_PORT) != 0) {
        D("Can't start OpenGLES renderer?");
        return -1;
    }
    return 0;
}

void
android_stopOpenglesRenderer(void)
{
    if (rendererLib) {
        stopOpenGLRenderer();
    }
}

int
android_showOpenglesWindow(void* window, int x, int y, int width, int height, float rotation)
{
    if (rendererLib) {
        return createOpenGLSubwindow(window, x, y, width, height, rotation);
    } else {
        return -1;
    }
}

int
android_hideOpenglesWindow(void)
{
    if (rendererLib) {
        return destroyOpenGLSubwindow();
    } else {
        return -1;
    }
}

void
android_redrawOpenglesWindow(void)
{
    if (rendererLib) {
        repaintOpenGLDisplay();
    }
}
