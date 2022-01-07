/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_PS4

#include <orbis/libkernel.h>
#include "SDL_video.h"
#include "SDL_ps4opengles.h"
#include "SDL_ps4video.h"

/* EGL implementation of SDL OpenGL support */

extern uint32_t PS4_PigletModId;

void
PS4_GLES_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor) {
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
}

int
PS4_GLES_LoadLibrary(_THIS, const char *path) {
    return SDL_EGL_LoadLibrary(_this, path, EGL_DEFAULT_DISPLAY, 0);
}

void *
PS4_GLES_GetProcAddress(_THIS, const char *proc) {
    void *ptr = NULL;
    int res;

    res = sceKernelDlsym((int) PS4_PigletModId, proc, (void **) &ptr);
    if (res != 0) {
        SDL_Log("PS4_GLES_GetProcAddress: sceKernelDlsym failed: 0x%08x (%s == %p)\n", res, proc, ptr);
    }

    return ptr;
}

SDL_EGL_CreateContext_impl(PS4)

SDL_EGL_MakeCurrent_impl(PS4)

SDL_EGL_SwapWindow_impl(PS4)

#endif /* SDL_VIDEO_DRIVER_PS4 */

/* vi: set ts=4 sw=4 expandtab: */
