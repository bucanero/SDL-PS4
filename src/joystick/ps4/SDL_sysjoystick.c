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

#include "../SDL_sysjoystick.h"
#include "SDL_events.h"
#include "SDL_error.h"

#define ORBIS_USER_SERVICE_USER_ID_INVALID 0xFFFFFFFF
#define ORBIS_PAD_ERROR_ALREADY_OPENED 0x80920004
#define ORBIS_PAD_ERROR_DEVICE_NOT_CONNECTED 0x80920007

// strange axis settings, but we want to reflect "Sony DualShock 4 V2" game controller mapping
#define ORBIS_PAD_AXIS_LX   0
#define ORBIS_PAD_AXIS_LY   1
#define ORBIS_PAD_AXIS_RX   2
#define ORBIS_PAD_AXIS_RY   5
#define ORBIS_PAD_AXIS_L2   3
#define ORBIS_PAD_AXIS_R2   4

// TODO
#define ORBIS_PAD_BUTTON_SHARE 0
#define ORBIS_PAD_BUTTON_DUMMY 0
// https://github.com/MasonLeeBack/RSEngine/blob/9ac4a0380898cb59a48d29dc7250dfb70430932f/src/Engine/Input/libScePad/RSScePad.cpp#L23
// int scePadSetParticularMode(int enable); // enable use of SHARE and PS buttons

extern void PS4_LoadModules();

/* Current pad state */
static OrbisPadData pads[ORBIS_USER_SERVICE_MAX_LOGIN_USERS];
static int pads_handles[ORBIS_USER_SERVICE_MAX_LOGIN_USERS]; // index: sdl joy number, entry: sce pad handle

static int SDL_numjoysticks = 0;

// "Sony DualShock 4 V2" buttons layout (with game controller mapping for information)
static const unsigned int button_map[] = {
        ORBIS_PAD_BUTTON_SQUARE,    // x:b0
        ORBIS_PAD_BUTTON_CROSS,     // a:b1
        ORBIS_PAD_BUTTON_CIRCLE,    // b:b2
        ORBIS_PAD_BUTTON_TRIANGLE,  // y:b3
        ORBIS_PAD_BUTTON_L1,        // leftshoulder:b4
        ORBIS_PAD_BUTTON_R1,        // rightshoulder:b5
        ORBIS_PAD_BUTTON_DUMMY,     // ??:b6 (unused in game controller db)
        ORBIS_PAD_BUTTON_DUMMY,     // ??:b7 (unused in game controller db)
        ORBIS_PAD_BUTTON_DUMMY,     // ??:b8 (unused in game controller db)
        ORBIS_PAD_BUTTON_SHARE,     // start:b9
        ORBIS_PAD_BUTTON_L3,        // leftstick:b10
        ORBIS_PAD_BUTTON_R3,        // rightstick:b11
        ORBIS_PAD_BUTTON_DUMMY,     // guide:b12
        ORBIS_PAD_BUTTON_OPTIONS,   // back:b13
        ORBIS_PAD_BUTTON_DOWN,      // used as analog input in game controller db
        ORBIS_PAD_BUTTON_LEFT,      // used as analog input in game controller db
        ORBIS_PAD_BUTTON_UP,        // used as analog input in game controller db
        ORBIS_PAD_BUTTON_RIGHT,     // used as analog input in game controller db
        ORBIS_PAD_BUTTON_L2,        // used as analog input in game controller db
        ORBIS_PAD_BUTTON_R2         // used as analog input in game controller db
};

/* Map analog inputs to -32768 -> 32767 */
static int analog_map[256];

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
    int pad_handle;
    OrbisUserServiceLoginUserIdList users;
    SDL_numjoysticks = 0;

    ret = sceUserServiceGetLoginUserIdList(&users);
    if (ret != 0) {
        SDL_Log("PS4_JoystickDetect: sceUserServiceGetLoginUserIdList failed (0x%08x)\n", ret);
        return;
    }

    for (i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++) {
        if (users.userId[i] == ORBIS_USER_SERVICE_USER_ID_INVALID) {
            continue;
        }

        //scePadSetParticularMode(1);
        pad_handle = scePadOpen(users.userId[i], 0, 0, NULL);
        if (pad_handle == ORBIS_PAD_ERROR_DEVICE_NOT_CONNECTED) {
            //SDL_Log("PS4_JoystickDetect: scePadOpen(%i) == ORBIS_PAD_ERROR_DEVICE_NOT_CONNECTED\n", i);
            continue;
        }
        if (pad_handle == ORBIS_PAD_ERROR_ALREADY_OPENED) {
            //SDL_Log("PS4_JoystickDetect: scePadOpen(%i) == ORBIS_PAD_ERROR_ALREADY_OPENED\n", i);
            SDL_numjoysticks++;
            continue;
        }

        pads_handles[i] = pad_handle;
        SDL_numjoysticks++;
        SDL_Log("PS4_JoystickDetect: new joystick detected (%i) (joysticks count: %i)\n",
                i, SDL_numjoysticks);
    }
}

/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * It should return number of joysticks, or -1 on an unrecoverable fatal error.
 */
int PS4_JoystickInit(void) {
    int i;
    uint32_t ret;

    // initialize modules if not already done
    PS4_LoadModules();

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
    joystick->nhats = 1;
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
    unsigned char hat = 0;
    unsigned char lx, ly, rx, ry, l2, r2;
    static unsigned int old_buttons[] = {0, 0, 0, 0};
    static unsigned char old_lx[] = {0, 0, 0, 0};
    static unsigned char old_ly[] = {0, 0, 0, 0};
    static unsigned char old_rx[] = {0, 0, 0, 0};
    static unsigned char old_ry[] = {0, 0, 0, 0};
    static unsigned char old_lt[] = {0, 0, 0, 0};
    static unsigned char old_rt[] = {0, 0, 0, 0};
    OrbisPadData pad;

    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        SDL_Log("PS4_JoystickUpdate: invalid joystick index: %i\n", index);
        return;
    }

    if (pads_handles[index] < 1) {
        SDL_Log("PS4_JoystickUpdate: invalid pad handle: %i\n", index);
        return;
    }

    pad = pads[index];
    scePadReadState(pads_handles[index], &pad);

    buttons = pad.buttons;

    lx = pad.leftStick.x;
    ly = pad.leftStick.y;
    rx = pad.rightStick.x;
    ry = pad.rightStick.y;
    l2 = pad.analogButtons.l2;
    r2 = pad.analogButtons.r2;

    // Axes
    if (old_lx[index] != lx) {
        SDL_PrivateJoystickAxis(joystick, ORBIS_PAD_AXIS_LX, analog_map[lx]);
        old_lx[index] = lx;
    }
    if (old_ly[index] != ly) {
        SDL_PrivateJoystickAxis(joystick, ORBIS_PAD_AXIS_LY, analog_map[ly]);
        old_ly[index] = ly;
    }
    if (old_rx[index] != rx) {
        SDL_PrivateJoystickAxis(joystick, ORBIS_PAD_AXIS_RX, analog_map[rx]);
        old_rx[index] = rx;
    }
    if (old_ry[index] != ry) {
        SDL_PrivateJoystickAxis(joystick, ORBIS_PAD_AXIS_RY, analog_map[ry]);
        old_ry[index] = ry;
    }
    if (old_lt[index] != l2) {
        SDL_PrivateJoystickAxis(joystick, ORBIS_PAD_AXIS_L2, analog_map[l2]);
        old_lt[index] = l2;
    }
    if (old_rt[index] != r2) {
        SDL_PrivateJoystickAxis(joystick, ORBIS_PAD_AXIS_R2, analog_map[r2]);
        old_rt[index] = r2;
    }

    // Buttons
    changed = old_buttons[index] ^ buttons;
    old_buttons[index] = buttons;

    if (changed) {
        for (i = 0; i < SDL_arraysize(button_map); i++) {
            if (changed & button_map[i]) {
                SDL_PrivateJoystickButton(
                        joystick, i, (buttons & button_map[i]) ? SDL_PRESSED : SDL_RELEASED);
                // handle hat
                if (i == 14) {
                    hat |= SDL_HAT_DOWN;
                } else if (i == 15) {
                    hat |= SDL_HAT_LEFT;
                } else if (i == 16) {
                    hat |= SDL_HAT_UP;
                } else if (i == 17) {
                    hat |= SDL_HAT_RIGHT;
                }
            }
        }
        SDL_PrivateJoystickHat(joystick, 0, hat);
    }
}

/* Function to close a joystick after use */
void PS4_JoystickClose(SDL_Joystick *joystick) {
    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > ORBIS_USER_SERVICE_MAX_LOGIN_USERS) {
        SDL_Log("PS4_JoystickClose: invalid joystick index: %i\n", index);
        return;
    }

    if (pads_handles[index] < 1) {
        SDL_Log("PS4_JoystickClose: invalid pad handle: 0x%08x\n", index);
        return;
    }

    scePadClose(pads_handles[index]);
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
    scePadSetVibration(pads_handles[index], &param);

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
    scePadSetLightBar(pads_handles[index], &color);

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

static SDL_bool
PS4_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out) {
    out->a.kind = EMappingKind_Button;
    out->a.target = 1;
    out->b.kind = EMappingKind_Button;
    out->b.target = 2;
    out->x.kind = EMappingKind_Button;
    out->x.target = 0;
    out->y.kind = EMappingKind_Button;
    out->y.target = 3;
    out->back.kind = EMappingKind_Button;
    out->back.target = 13;
    //out->guide.kind = EMappingKind_Button;
    //out->guide.target = 0;
    out->start.kind = EMappingKind_Button;
    out->start.target = 9;
    out->leftstick.kind = EMappingKind_Button;
    out->leftstick.target = 10;
    out->rightstick.kind = EMappingKind_Button;
    out->rightstick.target = 10;
    out->leftshoulder.kind = EMappingKind_Button;
    out->leftshoulder.target = 4;
    out->rightshoulder.kind = EMappingKind_Button;
    out->rightshoulder.target = 5;
    out->dpup.kind = EMappingKind_Button;
    out->dpup.target = 16;
    out->dpdown.kind = EMappingKind_Button;
    out->dpdown.target = 14;
    out->dpleft.kind = EMappingKind_Button;
    out->dpleft.target = 15;
    out->dpright.kind = EMappingKind_Button;
    out->dpright.target = 17;
    out->leftx.kind = EMappingKind_Axis;
    out->leftx.target = ORBIS_PAD_AXIS_LX;
    out->lefty.kind = EMappingKind_Axis;
    out->lefty.target = ORBIS_PAD_AXIS_LY;
    out->rightx.kind = EMappingKind_Axis;
    out->rightx.target = ORBIS_PAD_AXIS_RX;
    out->righty.kind = EMappingKind_Axis;
    out->righty.target = ORBIS_PAD_AXIS_RY;
    out->lefttrigger.kind = EMappingKind_Axis;
    out->lefttrigger.target = ORBIS_PAD_AXIS_L2;
    out->righttrigger.kind = EMappingKind_Axis;
    out->righttrigger.target = ORBIS_PAD_AXIS_R2;

    return SDL_TRUE;
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
        PS4_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_PS4 */

/* vi: set ts=4 sw=4 expandtab: */
