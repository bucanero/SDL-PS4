//
// Created by cpasjuste on 01/01/2022.
//

#include "../../SDL_internal.h"
#include "../../render/opengles2/SDL_shaders_gles2.h"

#include "SDL_ps4opengles_shaders.h"
#include "shaders/ps4_shader_vertex_default.h"
#include "shaders/ps4_shader_fragment_solid.h"
#include "shaders/ps4_shader_fragment_texture_abgr.h"
#include "shaders/ps4_shader_fragment_texture_argb.h"
#include "shaders/ps4_shader_fragment_texture_bgr.h"
#include "shaders/ps4_shader_fragment_texture_rgb.h"
#include "shaders/ps4_shader_fragment_texture_yuv_jpeg.h"
#include "shaders/ps4_shader_fragment_texture_yuv_bt601.h"
#include "shaders/ps4_shader_fragment_texture_yuv_bt709.h"
#include "shaders/ps4_shader_fragment_texture_nv12_jpeg.h"

const Uint8 *PS4GLES2_GetShaderBinary(GLES2_ShaderType type, int *size) {
    switch (type) {
        case GLES2_SHADER_VERTEX_DEFAULT:
            *size = PS4_SHADER_VERTEX_DEFAULT_LENGTH;
            return (Uint8 *) PS4_SHADER_VERTEX_DEFAULT;
        case GLES2_SHADER_FRAGMENT_SOLID:
            *size = PS4_SHADER_FRAGMENT_SOLID_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_SOLID;
        case GLES2_SHADER_FRAGMENT_TEXTURE_ABGR:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_ABGR_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_ABGR;
        case GLES2_SHADER_FRAGMENT_TEXTURE_ARGB:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_ARGB_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_ARGB;
        case GLES2_SHADER_FRAGMENT_TEXTURE_RGB:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_RGB_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_RGB;
        case GLES2_SHADER_FRAGMENT_TEXTURE_BGR:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_BGR_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_BGR;
        case GLES2_SHADER_FRAGMENT_TEXTURE_YUV_JPEG:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_YUV_JPEG_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_YUV_JPEG;
        case GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT601:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_YUV_BT601_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_YUV_BT601;
        case GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT709:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_YUV_BT709_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_YUV_BT709;
        case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_JPEG:
            *size = PS4_SHADER_FRAGMENT_TEXTURE_NV12_JPEG_LENGTH;
            return (Uint8 *) PS4_SHADER_FRAGMENT_TEXTURE_NV12_JPEG;
            // TODO
            /*
            case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RA_BT601:
            case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RG_BT601:
            case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RA_BT709:
            case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RG_BT709:
            case GLES2_SHADER_FRAGMENT_TEXTURE_NV21_JPEG:
            case GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT601:
            case GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT709:
            case GLES2_SHADER_FRAGMENT_TEXTURE_EXTERNAL_OES:
            */
        default:
            return NULL;
    }
}
