//
// Created by cpasjuste on 06/01/2022.
//

#ifndef __SDL_PS4PIGLET_H__
#define __SDL_PS4PIGLET_H__

#if SDL_VIDEO_DRIVER_PS4

#include <stdbool.h>

int PS4_PigletInit();

void PS4_PigletExit();

bool PS4_PigletShaccAvailable();

#endif //SDL_VIDEO_DRIVER_PS4

#endif //__SDL_PS4PIGLET_H__
