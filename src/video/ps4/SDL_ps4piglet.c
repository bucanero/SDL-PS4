//
// Created by cpasjuste on 31/12/2021.
//

//#if SDL_VIDEO_DRIVER_PS4

#include <orbis/libkernel.h>
#include <sys/mman.h>

#include "../../SDL_internal.h"
#include "SDL_hints.h"
#include "SDL_error.h"
#include "SDL_ps4piglet.h"

#define PIGLET_MODULE_NAME "libScePigletv2VSH.sprx"
#define SHACC_MODULE_NAME "libSceShaccVSH.sprx"

#define ARRAY_SIZE(ar) (sizeof(ar) / sizeof((ar)[0]))

typedef void module_patch_cb_t(uint8_t *base);

uint32_t PS4_PigletModId;
static uint32_t shaccModId;

/* XXX: patches below are given for Piglet module from 4.74 Devkit PUP */
static void pgl_patches_cb(uint8_t *base) {
    /* Patch runtime compiler check */
    const uint8_t p_set_eax_to_1[] = {
            0x31, 0xC0, 0xFF, 0xC0, 0x90,
    };
    memcpy(base + 0x5451F, p_set_eax_to_1, sizeof(p_set_eax_to_1));

    /* Tell that runtime compiler exists */
    *(uint8_t *) (base + 0xB2DEC) = 0;
    *(uint8_t *) (base + 0xB2DED) = 0;
    *(uint8_t *) (base + 0xB2DEE) = 1;
    *(uint8_t *) (base + 0xB2E21) = 1;

    /* Inform Piglet that we have shader compiler module loaded */
    *(int32_t *) (base + 0xB2E24) = shaccModId;
}

static unsigned int sceKernelGetModuleInfoByName(const char *name, OrbisKernelModuleInfo *info) {
    OrbisKernelModuleInfo tmpInfo;
    OrbisKernelModule handles[256];
    size_t numModules;
    size_t i;
    int ret;

    SDL_Log("sceKernelGetModuleInfoByName(%s)\n", name);

    if (!name || !info) {
        return 0x8002000E;
    }

    memset(handles, 0, sizeof(handles));

    ret = sceKernelGetModuleList(handles, ARRAY_SIZE(handles), &numModules);
    if (ret) {
        SDL_Log("sceKernelGetModuleInfoByName: sceKernelGetModuleList failed (0x%08x)\n", ret);
        return ret;
    }

    SDL_Log("sceKernelGetModuleInfoByName: found %zu modules\n", numModules);

    for (i = 0; i < numModules; ++i) {
        memset(&tmpInfo, 0, sizeof(tmpInfo));
        tmpInfo.size = sizeof(tmpInfo);
        ret = sceKernelGetModuleInfo(handles[i], &tmpInfo);
        if (ret) {
            SDL_Log("sceKernelGetModuleInfoByName: sceKernelGetModuleInfo[%zu] failed (0x%08x)\n", i, ret);
            return ret;
        }

        SDL_Log("sceKernelGetModuleInfoByName: [%zu]: %s\n", i, tmpInfo.name);

        if (strcmp(tmpInfo.name, name) == 0) {
            SDL_Log("sceKernelGetModuleInfoByName: piglet module found: %s\n", tmpInfo.name);
            memcpy(info, &tmpInfo, sizeof(tmpInfo));
            return 0;
        }
    }

    return 0x80020002;
}

static unsigned int shaderCompilerGetModuleBase(const char *name, uint64_t *base, uint64_t *size) {
    OrbisKernelModuleInfo moduleInfo;
    unsigned int ret;

    SDL_Log("shaderCompilerGetModuleBase(%s)\n", name);

    ret = sceKernelGetModuleInfoByName(name, &moduleInfo);
    if (ret) {
        SDL_Log("shaderCompilerGetModuleBase: sceKernelGetModuleInfoByName(%s) failed: 0x%08X\n", name, ret);
        return ret;
    }

    if (base) {
        *base = (uint64_t) moduleInfo.segmentInfo[0].address;
    }
    if (size) {
        *size = moduleInfo.segmentInfo[0].size;
    }

    return 0;
}

static int shaderCompilerPatchModule(const char *name, module_patch_cb_t *cb) {
    uint64_t base, size;
    int ret;

    if (shaderCompilerGetModuleBase(name, &base, &size) != 0) {
        SDL_Log("shaderCompilerPatchModule: getModuleBase return error\n");
        return 1;
    }

    SDL_Log("shaderCompilerPatchModule: module base=0x%08lX size=%ld\n", base, size);

    ret = sceKernelMprotect((void *) base, size, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (ret) {
        SDL_Log("shaderCompilerPatchModule: sceKernelMprotect(%s) failed: 0x%08X\n", name, ret);
        return 1;
    }

    SDL_Log("shaderCompilerPatchModule: patching module\n");

    if (cb) {
        (*cb)((uint8_t *) base);
    }

    SDL_Log("shaderCompilerPatchModule: patching module done\n");

    return 0;
}

int PS4_PigletInit() {

    SDL_Log("PS4_PigletInit\n");
    char module_path[512];

    // load piglet and shader compiler module from specified path if requested
    // else load from piglet from device without shader compiler support
    const char *path = SDL_GetHint(SDL_HINT_PS4_PIGLET_MODULES_PATH);
    if (path) {
        snprintf(module_path, 511, "%s/%s", path, PIGLET_MODULE_NAME);
        SDL_Log("PS4_PigletInit: loading piglet module from: %s\n", module_path);
        PS4_PigletModId = sceKernelLoadStartModule(module_path, 0, NULL, 0, NULL, NULL);
        if (PS4_PigletModId < 0) {
            SDL_Log("PS4_PigletInit: could not piglet load module %s (0x%08x)\n", module_path, PS4_PigletModId);
            return 1;
        }
        snprintf(module_path, 511, "%s/%s", path, SHACC_MODULE_NAME);
        SDL_Log("PS4_PigletInit: loading shacc module from: %s\n", module_path);
        shaccModId = sceKernelLoadStartModule(module_path, 0, NULL, 0, NULL, NULL);
        if (shaccModId < 0) {
            SDL_Log("PS4_PigletInit: could not load shacc module %s (0x%08x)\n", module_path, shaccModId);
            return 1;
        }
        if (shaderCompilerPatchModule(PIGLET_MODULE_NAME, &pgl_patches_cb) != 0) {
            sceKernelStopUnloadModule(shaccModId, 0, NULL, 0, NULL, NULL);
            shaccModId = 0;
            SDL_Log("PS4_PigletInit: unable to patch piglet module.\n");
            return 1;
        }
    } else {
        snprintf(module_path, 511, "/%s/common/lib/libScePigletv2VSH.sprx", sceKernelGetFsSandboxRandomWord());
        SDL_Log("PS4_PigletInit: loading piglet module from: %s\n", module_path);
        PS4_PigletModId = sceKernelLoadStartModule(module_path, 0, NULL, 0, NULL, NULL);
        if (PS4_PigletModId < 0) {
            SDL_Log("PS4_PigletInit: could not piglet load module %s (0x%08x)\n", module_path, PS4_PigletModId);
            return 1;
        }
    }

    SDL_Log("PS4_PigletInit: Ok\n");

    return 0;
}

void PS4_PigletExit() {
    if (shaccModId > 0) {
        sceKernelStopUnloadModule(shaccModId, 0, NULL, 0, NULL, NULL);
    }
    if (PS4_PigletModId > 0) {
        sceKernelStopUnloadModule(PS4_PigletModId, 0, NULL, 0, NULL, NULL);
    }
}

bool PS4_PigletShaccAvailable() {
    return shaccModId > 0;
}

//#endif // SDL_VIDEO_DRIVER_PS4