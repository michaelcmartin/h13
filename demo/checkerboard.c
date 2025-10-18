#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "h13/h13.h"

int
main(int argc, char **argv) {
    h13_win_t *h13 = h13_init_opengl("Checkerboard", 512, 512, 16, 16);
    if (!h13) return 1;
    unsigned char *p = h13->pixels;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            unsigned char c = (unsigned char)((y ^ x) & 1) * 255;
            *p++ = c;
            *p++ = c;
            *p++ = c;
            *p++ = 255;
        }
    }
    h13_flush_opengl(h13);
    int done = 0;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) done = 1;
            if (event.type = SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                h13_resize_opengl(h13, event.window.data1, event.window.data2);
            }
        }
        h13_render_opengl(h13);
        SDL_Delay(20);
    }

    h13_uninit_opengl(h13);
    return 0;
}
