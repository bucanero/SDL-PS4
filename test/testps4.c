//
// Created by cpasjuste on 30/12/2021.
//

#include "../include/SDL.h"

SDL_DisplayMode modes[5];
int mode_count = 0, current_mode = 0;
Uint8 *audio_pos;
Uint32 audio_len;

void print_info(SDL_Window *window, SDL_Renderer *renderer) {
    int w, h;
    SDL_DisplayMode mode;

    SDL_GetWindowSize(window, &w, &h);
    SDL_Log("window size: %i x %i\n", w, h);
    SDL_GetRendererOutputSize(renderer, &w, &h);
    SDL_Log("renderer size: %i x %i\n", w, h);

    SDL_GetCurrentDisplayMode(0, &mode);
    SDL_Log("display mode: %i x %i @ %i bpp (%s)",
            mode.w, mode.h,
            SDL_BITSPERPIXEL(mode.format),
            SDL_GetPixelFormatName(mode.format));
}

void change_mode(SDL_Window *window) {
    current_mode++;
    if (current_mode == mode_count) {
        current_mode = 0;
    }

    SDL_SetWindowDisplayMode(window, &modes[current_mode]);
}

void draw_rects(SDL_Renderer *renderer, int x, int y) {
    // R
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect r = {x, y, 64, 64};
    SDL_RenderFillRect(renderer, &r);

    // G
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect g = {x + 64, y, 64, 64};
    SDL_RenderFillRect(renderer, &g);

    // B
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_Rect b = {x + 128, y, 64, 64};
    SDL_RenderFillRect(renderer, &b);
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    if (audio_len == 0)
        return;
    len = (len > audio_len ? audio_len : len);
    SDL_memcpy(stream, audio_pos, len);
    audio_pos += len;
    audio_len -= len;
}

void audio_test() {
    Uint32 wav_length;
    Uint8 *wav_buffer;
    SDL_AudioSpec wav_spec, got_spec;

    if (SDL_LoadWAV("/data/test.wav", &wav_spec, &wav_buffer, &wav_length) == NULL) {
        SDL_Log("SDL_LoadWAV failed\n");
        return;
    }

    wav_spec.callback = audio_callback;
    wav_spec.userdata = NULL;
    audio_pos = wav_buffer;
    audio_len = wav_length;
    if (SDL_OpenAudio(&wav_spec, &got_spec) < 0) {
        SDL_Log("couldn't open audio: %s\n", SDL_GetError());
        return;
    }

    SDL_Log("wav_spec: %i chan, %i hz, %i samples\n",
            wav_spec.channels, wav_spec.freq, wav_spec.samples);

    SDL_Log("got_spec: %i chan, %i hz, %i samples\n",
            got_spec.channels, got_spec.freq, got_spec.samples);

    SDL_Log("playing: \"/data/test.wav\"\n");
    SDL_PauseAudio(0);

    while (audio_len > 0) {
        SDL_Delay(100);
    }

    SDL_CloseAudio();
    SDL_FreeWAV(wav_buffer);
    SDL_Log("played...\n");
}

int main(int argc, char *argv[]) {

    SDL_Event event;
    SDL_Window *window;
    SDL_Renderer *renderer;
    int done = 0, x = 0, w = 0, h = 0;

    // load piglet and shacc modules from specified path (enable shader compiler)
    //SDL_SetHint(SDL_HINT_PS4_PIGLET_MODULES_PATH, "/data/self/system/common/lib");

    // mandatory at least on switch, else gfx is not properly closed
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        SDL_Log("SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    /// create a window (OpenGL always enabled)
    /// available switch SDL2 video modes :
    /// 1920 x 1080 @ 32 bpp (SDL_PIXELFORMAT_RGBA8888)
    /// 1280 x 720 @ 32 bpp (SDL_PIXELFORMAT_RGBA8888)
    ///
    /// SDL_SetWindowSize to change window size when SDL_WINDOW_FULLSCREEN is NOT used (preferably)
    /// SDL_SetDisplayMode to change display size after SDL_CreateWindow called with SDL_WINDOW_FULLSCREEN
    /// (this means window size won't change, you'll need to handle that, as any SDL2 app)
    window = SDL_CreateWindow("sdl2_gles2", 0, 0, 1920, 1080, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // create a renderer (OpenGL ES2)
    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // pint some info about display/window/renderer
    print_info(window, renderer);

    // list available display modes
    mode_count = SDL_GetNumDisplayModes(0);
    for (int i = 0; i < mode_count; i++) {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, i, &mode);
        modes[i] = mode;
        SDL_Log("found display mode: %i x %i @ %i bpp (%s)",
                mode.w, mode.h,
                SDL_BITSPERPIXEL(mode.format),
                SDL_GetPixelFormatName(mode.format));
    }

    for (int i = 0; i < 1; i++) {
        if (SDL_JoystickOpen(i) == NULL) {
            SDL_Log("SDL_JoystickOpen: %s\n", SDL_GetError());
        }
    }

    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_JOYAXISMOTION:
                    SDL_Log("Joystick %d axis %d value: %d\n",
                            event.jaxis.which,
                            event.jaxis.axis, event.jaxis.value);
                    break;
                case SDL_JOYBUTTONDOWN:
                    SDL_Log("Joystick %d button %d down\n",
                            event.jbutton.which, event.jbutton.button);
                    if (event.jbutton.which == 0) {
                        if (event.jbutton.button == 0) {
                            // joystick #0 down (/\)
                            change_mode(window);
                            print_info(window, renderer);
                        } else if (event.jbutton.button == 2) {
                            // joystick #0 down (X)
                            if (w == 1920) {
                                SDL_SetWindowSize(window, 1280, 720);
                            } else {
                                SDL_SetWindowSize(window, 1920, 1080);
                            }
                            print_info(window, renderer);
                        } else if (event.jbutton.button == 3) {
                            // joystick #0 down ([])
                            audio_test();
                        }
                    }
                    // joystick #0 down (O)
                    if (event.jbutton.which == 0 && event.jbutton.button == 1) {
                        done = 1;
                    }
                    break;
                default:
                    break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Fill renderer bounds
        SDL_SetRenderDrawColor(renderer, 111, 111, 111, 255);
        SDL_GetWindowSize(window, &w, &h);
        SDL_Rect f = {0, 0, w, h};
        SDL_RenderFillRect(renderer, &f);

        draw_rects(renderer, x, 0);
        draw_rects(renderer, x, h - 64);

        SDL_RenderPresent(renderer);

        x++;
        if (x > w - 192) {
            x = 0;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}