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

#include <stdbool.h>
#include <orbis/libkernel.h>
#include <orbis/Pigletv2VSH.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>

#include "../SDL_sysvideo.h"
#include "../../render/SDL_sysrender.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "SDL_timer.h"
#include "SDL_hints.h"

#include "SDL_ps4video.h"
#include "SDL_ps4opengles.h"
#include "SDL_ps4piglet.h"

/* Only one window supported */
static SDL_Window *ps4_window = NULL;
static OrbisPglWindow ps4_egl_window;
static bool ps4_init_done = false;
char log_buffer[1024];

static void *
PS4_logCb(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    snprintf(log_buffer, 1023, "<SDL2> %s\n", message);
    sceKernelDebugOutText(0, log_buffer);
    return NULL;
}

int
PS4_LoadModules() {
    uint32_t ret;

    if (ps4_init_done) {
        return 0;
    }

    // load common modules
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: SYSTEM_SERVICE (0x%08x)", ret);
    }

    // load user module
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: USER_SERVICE (0x%08x)", ret);
    }

    // load pad module
    ret = sceSysmoduleLoadModuleInternal(0x80000024);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: PAD (0x%08x)", ret);
    }

    // load audio module
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: AUDIOOUT (0x%08x)", ret);
    }

    // initialize user service (used for pad and audio drivers)
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    ret = sceUserServiceInitialize(&param);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: sceUserServiceInitialize failed (0x%08x)", ret);
    }

    // hide splash screen (is this mandatory ?)
    sceSystemServiceHideSplashScreen();

    ps4_init_done = true;
    return 0;
}

static void
PS4_Destroy(SDL_VideoDevice *device) {
    SDL_Log("PS4_Destroy\n");

    PS4_PigletExit();

    if (device != NULL) {
        if (device->driverdata != NULL) {
            SDL_free(device->driverdata);
        }
        SDL_free(device);
    }
    SDL_Log("PS4_Destroy done\n");
}

static SDL_VideoDevice *
PS4_CreateDevice(int devindex) {
    SDL_Log("PS4_CreateDevice\n");

    SDL_VideoDevice *device;

    // log to kernel
    SDL_LogSetOutputFunction((SDL_LogOutputFunction) &PS4_logCb, NULL);

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    // initialize modules if not already done
    if (PS4_LoadModules() != 0) {
        return NULL;
    }

    // load piglet
    if (PS4_PigletInit() != 0) {
        return NULL;
    }

    /* Setup amount of available displays */
    device->num_displays = 0;

    /* Set device free function */
    device->free = PS4_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = PS4_VideoInit;
    device->VideoQuit = PS4_VideoQuit;
    device->GetDisplayModes = PS4_GetDisplayModes;
    device->SetDisplayMode = PS4_SetDisplayMode;
    device->CreateSDLWindow = PS4_CreateWindow;
    device->CreateSDLWindowFrom = PS4_CreateWindowFrom;
    device->SetWindowTitle = PS4_SetWindowTitle;
    device->SetWindowIcon = PS4_SetWindowIcon;
    device->SetWindowPosition = PS4_SetWindowPosition;
    device->SetWindowSize = PS4_SetWindowSize;
    device->ShowWindow = PS4_ShowWindow;
    device->HideWindow = PS4_HideWindow;
    device->RaiseWindow = PS4_RaiseWindow;
    device->MaximizeWindow = PS4_MaximizeWindow;
    device->MinimizeWindow = PS4_MinimizeWindow;
    device->RestoreWindow = PS4_RestoreWindow;
    device->DestroyWindow = PS4_DestroyWindow;

    device->GL_LoadLibrary = PS4_GLES_LoadLibrary;
    device->GL_GetProcAddress = PS4_GLES_GetProcAddress;
    device->GL_UnloadLibrary = PS4_GLES_UnloadLibrary;
    device->GL_CreateContext = PS4_GLES_CreateContext;
    device->GL_MakeCurrent = PS4_GLES_MakeCurrent;
    device->GL_SetSwapInterval = PS4_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = PS4_GLES_GetSwapInterval;
    device->GL_SwapWindow = PS4_GLES_SwapWindow;
    device->GL_DeleteContext = PS4_GLES_DeleteContext;
    device->GL_DefaultProfileConfig = PS4_GLES_DefaultProfileConfig;

    device->PumpEvents = PS4_PumpEvents;

    return device;
}

VideoBootStrap PS4_bootstrap = {
        "PS4",
        "Sony PS4 Video Driver",
        PS4_CreateDevice
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
PS4_VideoInit(_THIS) {
    SDL_Log("PS4_VideoInit\n");

    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    SDL_zero(current_mode);
    current_mode.w = 1920;
    current_mode.h = 1080;
    current_mode.refresh_rate = 60;
    current_mode.format = SDL_PIXELFORMAT_RGBA8888;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = NULL;
    SDL_AddVideoDisplay(&display, SDL_FALSE);

    // TODO
    /*
    // init keyboard
    PS4_InitKeyboard();
    // init mouse
    PS4_InitMouse();
    // init software keyboard
    PS4_InitSwkb();
    */

    return 0;
}

void
PS4_VideoQuit(_THIS) {
    SDL_Log("PS4_VideoQuit\n");

    // TODO
    /*
    // exit keyboard
    PS4_QuitKeyboard();
    // exit mouse
    PS4_QuitMouse();
    // exit software keyboard
    PS4_QuitSwkb();
    */
}

void
PS4_GetDisplayModes(_THIS, SDL_VideoDisplay *display) {
    SDL_Log("PS4_GetDisplayModes\n");

    SDL_DisplayMode mode;

    // 1920x1080 RGBA8888, default mode
    SDL_AddDisplayMode(display, &display->current_mode);

    // 1280x720 RGBA8888
    SDL_zero(mode);
    mode.w = 1280;
    mode.h = 720;
    mode.refresh_rate = 60;
    mode.format = SDL_PIXELFORMAT_RGBA8888;
    SDL_AddDisplayMode(display, &mode);
}

int
PS4_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode) {
    SDL_Log("PS4_SetDisplayMode\n");

    SDL_WindowData *data = (SDL_WindowData *) SDL_GetFocusWindow()->driverdata;
    SDL_GLContext ctx = SDL_GL_GetCurrentContext();

    if (data != NULL && data->egl_surface != EGL_NO_SURFACE) {
        SDL_EGL_MakeCurrent(_this, NULL, NULL);
        SDL_EGL_DestroySurface(_this, data->egl_surface);

        ps4_egl_window.uWidth = mode->w;
        ps4_egl_window.uHeight = mode->h;

        data->egl_surface = SDL_EGL_CreateSurface(_this, &ps4_egl_window);
        SDL_EGL_MakeCurrent(_this, data->egl_surface, ctx);
    }

    return 0;
}

int
PS4_CreateWindow(_THIS, SDL_Window *window) {
    SDL_Log("PS4_CreateWindow\n");

    SDL_WindowData *window_data = NULL;

    if (ps4_window != NULL) {
        return SDL_SetError("ps4 only supports one window");
    }

    if (!_this->egl_data) {
        return SDL_SetError("egl not initialized");
    }

    window_data = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (window_data == NULL) {
        return SDL_OutOfMemory();
    }

    ps4_egl_window.uWidth = window->w;
    ps4_egl_window.uHeight = window->h;

    window_data->egl_surface = SDL_EGL_CreateSurface(_this, &ps4_egl_window);
    if (window_data->egl_surface == EGL_NO_SURFACE) {
        return SDL_SetError("could not create egl window surface");
    }

    /* Setup driver data for this window */
    window->driverdata = window_data;
    ps4_window = window;

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

void
PS4_DestroyWindow(_THIS, SDL_Window *window) {
    SDL_Log("PS4_DestroyWindow\n");

    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (window == ps4_window) {
        if (data != NULL) {
            if (data->egl_surface != EGL_NO_SURFACE) {
                SDL_EGL_MakeCurrent(_this, NULL, NULL);
                SDL_EGL_DestroySurface(_this, data->egl_surface);
            }
            if (window->driverdata != NULL) {
                SDL_free(window->driverdata);
                window->driverdata = NULL;
            }
        }
        ps4_window = NULL;
    }
}

int
PS4_CreateWindowFrom(_THIS, SDL_Window *window, const void *data) {
    return -1;
}

void
PS4_SetWindowTitle(_THIS, SDL_Window *window) {
}

void
PS4_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon) {
}

void
PS4_SetWindowPosition(_THIS, SDL_Window *window) {
}

void
PS4_SetWindowSize(_THIS, SDL_Window *window) {
    SDL_Log("PS4_SetWindowSize\n");

    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    SDL_GLContext ctx = SDL_GL_GetCurrentContext();

    if (data != NULL && data->egl_surface != EGL_NO_SURFACE) {
        SDL_EGL_MakeCurrent(_this, NULL, NULL);
        SDL_EGL_DestroySurface(_this, data->egl_surface);

        ps4_egl_window.uWidth = window->w;
        ps4_egl_window.uHeight = window->h;

        data->egl_surface = SDL_EGL_CreateSurface(_this, &ps4_egl_window);
        SDL_EGL_MakeCurrent(_this, data->egl_surface, ctx);
    }
}

void
PS4_ShowWindow(_THIS, SDL_Window *window) {
}

void
PS4_HideWindow(_THIS, SDL_Window *window) {
}

void
PS4_RaiseWindow(_THIS, SDL_Window *window) {
}

void
PS4_MaximizeWindow(_THIS, SDL_Window *window) {
}

void
PS4_MinimizeWindow(_THIS, SDL_Window *window) {
}

void
PS4_RestoreWindow(_THIS, SDL_Window *window) {
}

void
PS4_SetWindowGrab(_THIS, SDL_Window *window, SDL_bool grabbed) {
}

void
PS4_PumpEvents(_THIS) {

    // TODO
    /*
    // we don't want other inputs overlapping with software keyboard
    if(!SDL_IsTextInputActive()) {
        PS4_PollKeyboard();
        PS4_PollMouse();
    }
    PS4_PollSwkb();
    */
}

#endif /* SDL_VIDEO_DRIVER_PS4 */