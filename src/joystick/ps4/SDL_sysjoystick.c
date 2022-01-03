/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_JOYSTICK_PS4

/* This is the PS4 implementation of the SDL joystick API */
#include <orbis/UserService.h>
#include <orbis/Pad.h>

#include <stdio.h>      /* For the definition of NULL */
#include <orbis/Sysmodule.h>

#include "../SDL_sysjoystick.h"
#include "SDL_events.h"
#include "SDL_error.h"

#define ORBIS_USER_SERVICE_USER_ID_INVALID 0xFFFFFFFF

/* Current pad state */
static OrbisPadData pads[ORBIS_USER_SERVICE_MAX_LOGIN_USERS];
static int port_map[ORBIS_USER_SERVICE_MAX_LOGIN_USERS]; //index: SDL joy number, entry: handle

static int SDL_numjoysticks = 1;

static const unsigned int button_map[] = {
        ORBIS_PAD_BUTTON_TRIANGLE,
        ORBIS_PAD_BUTTON_CIRCLE,
        ORBIS_PAD_BUTTON_CROSS,
        ORBIS_PAD_BUTTON_SQUARE,
        ORBIS_PAD_BUTTON_L1,
        ORBIS_PAD_BUTTON_R1,
        ORBIS_PAD_BUTTON_DOWN,
        ORBIS_PAD_BUTTON_LEFT,
        ORBIS_PAD_BUTTON_UP,
        ORBIS_PAD_BUTTON_RIGHT,
        ORBIS_PAD_BUTTON_L2,
        ORBIS_PAD_BUTTON_R2,
        ORBIS_PAD_BUTTON_L3,
        ORBIS_PAD_BUTTON_R3
};

static int analog_map[256];  /* Map analog inputs to -32768 -> 32767 */

typedef struct {
    int x;
    int y;
} point;

/* 4 points define the bezier-curve. */
/* The Vita has a good amount of analog travel, so use a linear curve */
static point a = {0, 0};
static point b = {0, 0};
static point c = {128, 32767};
static point d = {128, 32767};

/* simple linear interpolation between two points */
static SDL_INLINE void lerp(point *dest, point *first, point *second, float t) {
    dest->x = first->x + (second->x - first->x) * t;
    dest->y = first->y + (second->y - first->y) * t;
}

/* evaluate a point on a bezier-curve. t goes from 0 to 1.0 */
static int calc_bezier_y(float t) {
    point ab, bc, cd, abbc, bccd, dest;
    lerp(&ab, &a, &b, t);           /* point between a and b */
    lerp(&bc, &b, &c, t);           /* point between b and c */
    lerp(&cd, &c, &d, t);           /* point between c and d */
    lerp(&abbc, &ab, &bc, t);       /* point between ab and bc */
    lerp(&bccd, &bc, &cd, t);       /* point between bc and cd */
    lerp(&dest, &abbc, &bccd, t);   /* point on the bezier-curve */
    return dest.y;
}

void PS4_JoystickDetect() {

    uint32_t ret, i;
    OrbisUserServiceLoginUserIdList users;
    SDL_numjoysticks = 0;

    ret = sceUserServiceGetLoginUserIdList(&users);
    if (ret != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "PS4_JoystickDetect: sceUserServiceGetLoginUserIdList failed (0x%08x)\n", ret);
        return;
    }

    for (i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++) {
        port_map[i] = users.userId[i];
        if (port_map[i] == ORBIS_USER_SERVICE_USER_ID_INVALID) {
            continue;
        }
        SDL_Log("PS4_JoystickDetect: new joystick detected for user %i\n", i);
        scePadOpen(port_map[i], 0, 0, NULL);
        SDL_numjoysticks++;
    }
}

/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * It should return number of joysticks, or -1 on an unrecoverable fatal error.
 */
int PS4_JoystickInit(void) {

    int i;
    uint32_t ret;

    // load user module
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    if (ret != 0) {
        return SDL_SetError("PS4_JoystickInit: load module failed: USER_SERVICE (0x%08x)\n", ret);
    }

    // initialize user service
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    ret = sceUserServiceInitialize(&param);
    if (ret != 0) {
        return SDL_SetError("PS4_JoystickInit: sceUserServiceInitialize failed (0x%08x)\n", ret);
    }

    // load pad module
    ret = sceSysmoduleLoadModuleInternal(0x80000024);
    if (ret != 0) {
        return SDL_SetError("PS4_JoystickInit: load module failed: PAD (0x%08x)\n", ret);
    }

    ret = scePadInit();
    if (ret != 0) {
        return SDL_SetError("PS4_JoystickInit: scePadInit failed (0x%08x)\n", ret);
    }

    /* Create an accurate map from analog inputs (0 to 255)
       to SDL joystick positions (-32768 to 32767) */
    for (i = 0; i < 128; i++) {
        float t = (float) i / 127.0f;
        analog_map[i + 128] = calc_bezier_y(t);
        analog_map[127 - i] = -1 * analog_map[i + 128];
    }

    PS4_JoystickDetect();

    return SDL_numjoysticks;
}

int PS4_JoystickGetCount() {
    return SDL_numjoysticks;
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID PS4_JoystickGetDeviceInstanceID(int device_index) {
    return device_index;
}

/* Function to get the device-dependent name of a joystick */
const char *PS4_JoystickGetDeviceName(int index) {
    if (index > ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        SDL_SetError("PS4_JoystickGetDeviceName: invalid joystick index: %i\n", index);
        return NULL;
    } else {
        return "Sony DualShock 4 V2";
    }
}

static int
PS4_JoystickGetDevicePlayerIndex(int device_index) {
    return -1;
}

static void
PS4_JoystickSetDevicePlayerIndex(int device_index, int player_index) {
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int PS4_JoystickOpen(SDL_Joystick *joystick, int device_index) {
    joystick->nbuttons = SDL_arraysize(button_map);
    joystick->naxes = 6;
    joystick->nhats = 0;
    joystick->instance_id = device_index;

    return 0;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
static void PS4_JoystickUpdate(SDL_Joystick *joystick) {
    int i;
    unsigned int buttons;
    unsigned int changed;
    unsigned char lx, ly, rx, ry, lt, rt;
    static unsigned int old_buttons[] = {0, 0, 0, 0};
    static unsigned char old_lx[] = {0, 0, 0, 0};
    static unsigned char old_ly[] = {0, 0, 0, 0};
    static unsigned char old_rx[] = {0, 0, 0, 0};
    static unsigned char old_ry[] = {0, 0, 0, 0};
    static unsigned char old_lt[] = {0, 0, 0, 0};
    static unsigned char old_rt[] = {0, 0, 0, 0};
    OrbisPadData *pad = NULL;

    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "PS4_JoystickUpdate: invalid joystick index: %i\n", index);
        return;
    }

    if (port_map[index] == ORBIS_USER_SERVICE_USER_ID_INVALID) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "PS4_JoystickUpdate: invalid user service id: %i\n", index);
        return;
    }

    pad = &pads[index];
    scePadReadState(port_map[index], &pad);

    buttons = pad->buttons;

    lx = pad->leftStick.x;
    ly = pad->leftStick.y;
    rx = pad->rightStick.x;
    ry = pad->rightStick.y;
    lt = pad->analogButtons.l2;
    rt = pad->analogButtons.r2;

    // Axes

    if (old_lx[index] != lx) {
        SDL_PrivateJoystickAxis(joystick, 0, analog_map[lx]);
        old_lx[index] = lx;
    }
    if (old_ly[index] != ly) {
        SDL_PrivateJoystickAxis(joystick, 1, analog_map[ly]);
        old_ly[index] = ly;
    }
    if (old_rx[index] != rx) {
        SDL_PrivateJoystickAxis(joystick, 2, analog_map[rx]);
        old_rx[index] = rx;
    }
    if (old_ry[index] != ry) {
        SDL_PrivateJoystickAxis(joystick, 3, analog_map[ry]);
        old_ry[index] = ry;
    }

    if (old_lt[index] != lt) {
        SDL_PrivateJoystickAxis(joystick, 4, analog_map[lt]);
        old_lt[index] = lt;
    }
    if (old_rt[index] != rt) {
        SDL_PrivateJoystickAxis(joystick, 5, analog_map[rt]);
        old_rt[index] = rt;
    }

    // Buttons
    changed = old_buttons[index] ^ buttons;
    old_buttons[index] = buttons;

    if (changed) {
        for (i = 0; i < SDL_arraysize(button_map); i++) {
            if (changed & button_map[i]) {
                SDL_PrivateJoystickButton(
                        joystick, i, (buttons & button_map[i]) ? SDL_PRESSED : SDL_RELEASED);
            }
        }
    }
}

/* Function to close a joystick after use */
void PS4_JoystickClose(SDL_Joystick *joystick) {
    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "PS4_JoystickClose: invalid joystick index: %i\n", index);
        return;
    }

    if (port_map[index] == ORBIS_USER_SERVICE_USER_ID_INVALID) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "PS4_JoystickClose: invalid user service id: %i\n", index);
        return;
    }

    scePadClose(port_map[index]);
}

/* Function to perform any system-specific joystick related cleanup */
void PS4_JoystickQuit(void) {
}

SDL_JoystickGUID PS4_JoystickGetDeviceGUID(int device_index) {
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = PS4_JoystickGetDeviceName(device_index);
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

static int
PS4_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble) {
    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        return SDL_SetError("PS4_JoystickRumble: invalid joystick index: %i\n", index);
    }

    OrbisPadVibeParam param = {high_frequency_rumble / 256, low_frequency_rumble / 256};
    scePadSetVibration(port_map[index], &param);

    return 0;
}

static int
PS4_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left, Uint16 right) {
    return SDL_Unsupported();
}

static Uint32
PS4_JoystickGetCapabilities(SDL_Joystick *joystick) {
    // always return LED and rumble supported for now
    return SDL_JOYCAP_LED | SDL_JOYCAP_RUMBLE;
}


static int
PS4_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue) {
    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        return SDL_SetError("PS4_JoystickSetLED: invalid joystick index: %i\n", index);
    }

    OrbisPadColor color = {red, green, blue, 255};
    scePadSetLightBar(port_map[index], &color);

    return 0;
}

static int
PS4_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size) {
    return SDL_Unsupported();
}

static int
PS4_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled) {
    return SDL_Unsupported();
}

SDL_JoystickDriver SDL_PS4_JoystickDriver = {
        PS4_JoystickInit,
        PS4_JoystickGetCount,
        PS4_JoystickDetect,
        PS4_JoystickGetDeviceName,
        PS4_JoystickGetDevicePlayerIndex,
        PS4_JoystickSetDevicePlayerIndex,
        PS4_JoystickGetDeviceGUID,
        PS4_JoystickGetDeviceInstanceID,

        PS4_JoystickOpen,

        PS4_JoystickRumble,
        PS4_JoystickRumbleTriggers,

        PS4_JoystickGetCapabilities,
        PS4_JoystickSetLED,
        PS4_JoystickSendEffect,
        PS4_JoystickSetSensorsEnabled,

        PS4_JoystickUpdate,
        PS4_JoystickClose,
        PS4_JoystickQuit,
};

#endif /* SDL_JOYSTICK_PS4 */

/* vi: set ts=4 sw=4 expandtab: */
