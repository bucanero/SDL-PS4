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

#ifndef __SDL_PS4VIDEO_H__
#define __SDL_PS4VIDEO_H__

#if SDL_VIDEO_DRIVER_PS4

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

#include "SDL_egl.h"

typedef struct SDL_WindowData
{
    EGLSurface egl_surface;
} SDL_WindowData;

int PS4_VideoInit(_THIS);
void PS4_VideoQuit(_THIS);
void PS4_GetDisplayModes(_THIS, SDL_VideoDisplay *display);
int PS4_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int PS4_CreateWindow(_THIS, SDL_Window *window);
int PS4_CreateWindowFrom(_THIS, SDL_Window *window, const void *data);
void PS4_SetWindowTitle(_THIS, SDL_Window *window);
void PS4_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon);
void PS4_SetWindowPosition(_THIS, SDL_Window *window);
void PS4_SetWindowSize(_THIS, SDL_Window *window);
void PS4_ShowWindow(_THIS, SDL_Window *window);
void PS4_HideWindow(_THIS, SDL_Window *window);
void PS4_RaiseWindow(_THIS, SDL_Window *window);
void PS4_MaximizeWindow(_THIS, SDL_Window *window);
void PS4_MinimizeWindow(_THIS, SDL_Window *window);
void PS4_RestoreWindow(_THIS, SDL_Window *window);
void PS4_SetWindowGrab(_THIS, SDL_Window *window, SDL_bool grabbed);
void PS4_DestroyWindow(_THIS, SDL_Window *window);
void PS4_PumpEvents(_THIS);

#endif /* SDL_VIDEO_DRIVER_PS4 */
#endif /* __SDL_PS4VIDEO_H__ */

/* vi: set ts=4 sw=4 expandtab: */