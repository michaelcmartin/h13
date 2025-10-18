#ifndef H13_H_
#define H13_H_

typedef struct h13_win_s {
    int width, height;
    unsigned char *pixels; /* width * height * 4 bytes in R-G-B-A order */
} h13_win_t;

h13_win_t *h13_init_opengl(const char *title, int screen_width, int screen_height, int pixmap_width, int pixmap_height);
void h13_flush_opengl(h13_win_t *ctx);
void h13_render_opengl(h13_win_t *ctx);
void h13_resize_opengl(h13_win_t *ctx, int width, int height);
void h13_uninit_opengl(h13_win_t *ctx);

#endif
