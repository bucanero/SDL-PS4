/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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

/* Thread management routines for SDL */

#include "SDL_thread.h"
#include "../SDL_systhread.h"
#include "SDL_systhread_c.h"

#include <orbis/libkernel.h>
//#include <orbis/_types/pthread.h>

void *ThreadEntry(void *arg) {
    SDL_Log("SDL_RunThread\n");
    SDL_RunThread(arg);
    return 0;
}

int
SDL_SYS_CreateThread(SDL_Thread *thread) {
    int priority = 0;
    int ret = 0;
    OrbisPthread thid;
    thid = scePthreadSelf();
    SDL_Log("SDL_SYS_CreateThread: this = %p\n", thid);
    if (scePthreadGetprio(thid, &priority) == 0 && priority > 1) {
        SDL_Log("scePthreadCreate: this = %p\n", thid);
        ret = scePthreadCreate(&thread->handle, NULL, ThreadEntry, thread, NULL);
        if (ret == 0) {
            scePthreadSetprio(thread->handle, priority);
            return 0;
        } else {
            return SDL_SetError("scePthreadCreate() failed");
        }
    }
    return -1;
}

void
SDL_SYS_SetupThread(const char *name) {
    return;
}

SDL_threadID
SDL_ThreadID(void) {
    return (SDL_threadID) scePthreadSelf();
}

int
SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority) {
    int ret = 0;
    OrbisPthread thid;
    thid = scePthreadSelf();
    ret = scePthreadSetprio(thid, priority);
    if (ret < 0) {
        return -1;
    }
    return ret;
}

void
SDL_SYS_WaitThread(SDL_Thread *thread) {
    scePthreadJoin(thread->handle, NULL);
}

void
SDL_SYS_DetachThread(SDL_Thread *thread) {
    scePthreadDetach(thread->handle);
}

/* vi: set ts=4 sw=4 expandtab: */
