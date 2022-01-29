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

#if SDL_AUDIO_DRIVER_PS4

#include <stdio.h>
#include <stdlib.h>

#include "SDL_audio.h"
#include "SDL_error.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_ps4audio.h"

#include <orbis/AudioOut.h>

extern void PS4_LoadModules();

inline static Uint16
ps4_sample_size(Uint16 size) {
    if (size >= 2048) return 2048;
    if (size >= 1792) return 1792;
    if (size >= 1536) return 1536;
    if (size >= 1280) return 1280;
    if (size >= 1024) return 1024;
    if (size >= 768) return 768;
    if (size >= 512) return 512;
    return 256;
}

static uint32_t ps4_sceAudioOutInited = -1;

static int
PS4AUD_OpenDevice(_THIS, void *handle, const char *devname, int iscapture) {
    size_t mix_len, i;
    uint8_t fmt;

    this->hidden = (struct SDL_PrivateAudioData *) SDL_malloc(sizeof(*this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden, 0, sizeof(*this->hidden));

    switch (this->spec.format) {
        case AUDIO_S16LSB:
            fmt = (this->spec.channels == 1) ? ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO
                                             : ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
            break;
        case AUDIO_F32LSB:
            fmt = (this->spec.channels == 1) ? ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO
                                             : ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;
            break;
        default:
            return SDL_SetError("PS4AUD_OpenDevice: unsupported audio format: 0x%08x", this->spec.format);
    }

    this->spec.samples = ps4_sample_size(this->spec.samples);
    // TODO: use libsamplerate
    this->spec.freq = 48000;

    /* Update the fragment size as size in bytes. */
    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate the mixing buffer.  Its size and starting address must
       be a multiple of 64 bytes.  Our sample count is already a multiple of
       64, so spec->size should be a multiple of 64 as well. */
    mix_len = this->spec.size * NUM_BUFFERS;
    this->hidden->rawbuf = (Uint8 *) memalign(64, mix_len);
    if (this->hidden->rawbuf == NULL) {
        return SDL_SetError("PS4AUD_OpenDevice: couldn't allocate mix buffer");
    }

    this->hidden->aout = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,
                                         0, this->spec.samples, 48000, fmt);
    if (this->hidden->aout < 1) {
        free(this->hidden->rawbuf);
        this->hidden->rawbuf = NULL;
        return SDL_SetError("PS4AUD_OpenDevice: sceAudioOutOpen failed (0x%08x)", this->hidden->aout);
    }

    SDL_memset(this->hidden->rawbuf, 0, mix_len);
    for (i = 0; i < NUM_BUFFERS; i++) {
        this->hidden->mixbufs[i] = &this->hidden->rawbuf[i * this->spec.size];
    }

    this->hidden->next_buffer = 0;

    return 0;
}

static void PS4AUD_PlayDevice(_THIS) {
    Uint8 *buf = this->hidden->mixbufs[this->hidden->next_buffer];
    sceAudioOutOutput(this->hidden->aout, buf);
    this->hidden->next_buffer = (this->hidden->next_buffer + 1) % NUM_BUFFERS;
}

/* This function waits until it is possible to write a full sound buffer */
static void PS4AUD_WaitDevice(_THIS) {
    sceAudioOutOutput(this->hidden->aout, NULL);
}

static Uint8 *PS4AUD_GetDeviceBuf(_THIS) {
    return this->hidden->mixbufs[this->hidden->next_buffer];
}

static void PS4AUD_CloseDevice(_THIS) {
    int res;
    if (this->hidden->aout > 0) {
        res = sceAudioOutClose(this->hidden->aout);
        if (res != 0) {
            SDL_Log("PS4AUD_CloseDevice: sceAudioOutClose failed (0x%08x)\n", res);
        }
        this->hidden->aout = -1;
    }

    if (this->hidden->rawbuf != NULL) {
        free(this->hidden->rawbuf);
        this->hidden->rawbuf = NULL;
    }
}

static void PS4AUD_ThreadInit(_THIS) {
#if 0
    /* Increase the priority of this audio thread by 1 to put it ahead of other SDL threads. */
    int priority, ret;
    OrbisPthread tid;

    tid = scePthreadSelf();
    ret = scePthreadGetprio(tid, &priority);
    if (ret != 0) {
        SDL_Log("PS4AUD_ThreadInit: scePthreadGetprio failed (0x%08x)\n", ret);
    }

    ret = scePthreadSetprio(tid, priority - 1);
    if (ret != 0) {
        SDL_Log("PS4AUD_ThreadInit: scePthreadSetprio failed (0x%08x)\n", ret);
    }
#endif
}

static int
PS4AUD_Init(SDL_AudioDriverImpl *impl) {

    // initialize modules if not already done
    PS4_LoadModules();

    // sceAudioOutInit should only be called once (do we miss a "sceAudioOutExit"?)
    if (ps4_sceAudioOutInited != 0) {
        ps4_sceAudioOutInited = sceAudioOutInit();
        if (ps4_sceAudioOutInited != 0) {
            SDL_Log("PS4AUD_OpenDevice: sceAudioOutInit failed (0x%08x)\n", ps4_sceAudioOutInited);
            return SDL_SetError("PS4AUD_OpenDevice: sceAudioOutInit failed (0x%08x)\n", ps4_sceAudioOutInited);
        }
    }

    /* Set the function pointers */
    impl->OpenDevice = PS4AUD_OpenDevice;
    impl->PlayDevice = PS4AUD_PlayDevice;
    impl->WaitDevice = PS4AUD_WaitDevice;
    impl->GetDeviceBuf = PS4AUD_GetDeviceBuf;
    impl->CloseDevice = PS4AUD_CloseDevice;
    impl->ThreadInit = PS4AUD_ThreadInit;

    /* PS4 audio device */
    impl->OnlyHasDefaultOutputDevice = 1;

    return 1;   /* this audio target is available. */
}

AudioBootStrap PS4AUD_bootstrap = {
        "ps4", "PS4 audio driver", PS4AUD_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_PS4 */

/* vi: set ts=4 sw=4 expandtab: */
