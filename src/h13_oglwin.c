#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "h13/h13.h"
#include "glad/gl.h"

typedef struct h13_ogl_win_s {
    h13_win_t h13;

    SDL_Window *window;
    SDL_GLContext glContext;

    GLuint screenTexture, shaderProgram, vbo, vao;
    GLint posAttribute;
    GLint blendBorderUniform, scaleUniform, texUniform, texDimUniform;

    GLfloat aspect_image, aspect_screen;
    int w_current, h_current, w_requested, h_requested;
    int is_legacy;
} h13_ogl_win_t;

static GLfloat attribArray[] = {
    1.0f, -1.0f,
    1.0f, 1.0f,
    -1.0f, -1.0f,
    -1.0f, 1.0f
};

static const char *strVertexShader32 =
    "#version 150\n"
    "uniform vec2 scale;\n"
    "uniform vec2 texDim;\n"
    "in vec2 pos;\n"
    "smooth out vec2 texLoc;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(pos * scale, 0.0, 1.0);\n"
    "    texLoc.x = (pos.x + 1.0) / 2.0;\n"
    "    texLoc.y = (1.0 - pos.y) / 2.0;\n"
    "    texLoc = texLoc * texDim;\n"
    "}\n";

static const char *strFragmentShader32 =
    "#version 150\n"
    "uniform sampler2D tex;\n"
    "uniform vec2 texDim;\n"
    "uniform float blendBorder;\n"
    "smooth in vec2 texLoc;\n"
    "out vec4 outputColor;\n"
    "void main()\n"
    "{\n"
    "    vec2 slope = 0.5 / (blendBorder * fwidth(texLoc));\n"
    "    vec2 subtexel = fract(texLoc);\n"
    "    vec2 scaled = clamp(slope * subtexel, 0.0, 0.5) +\n"
    "                  clamp(slope * (subtexel - 1.0) + 0.5, 0.0, 0.5);\n"
    "    outputColor = texture2D(tex, (floor(texLoc) + scaled) / texDim);\n"
    "}\n";

static const char *strVertexShader21 =
    "#version 120\n"
    "attribute vec2 pos;\n"
    "uniform vec2 texDim;\n"
    "uniform vec2 scale;\n"
    "varying vec2 texLoc;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(pos * scale, 0.0, 1.0);\n"
    "    texLoc.x = (pos.x + 1.0) / 2.0;\n"
    "    texLoc.y = (1.0 - pos.y) / 2.0;\n"
    "    texLoc = texLoc * texDim;\n"
    "}\n";

static const char *strFragmentShader21 =
    "#version 120\n"
    "uniform sampler2D tex;\n"
    "uniform vec2 texDim;\n"
    "uniform float blendBorder;\n"
    "varying vec2 texLoc;\n"
    "void main()\n"
    "{\n"
    "    vec2 slope = 0.5 / (blendBorder * fwidth(texLoc));\n"
    "    vec2 subtexel = fract(texLoc);\n"
    "    vec2 scaled = clamp(slope * subtexel, 0.0, 0.5) +\n"
    "                  clamp(slope * (subtexel - 1.0) + 0.5, 0.0, 0.5);\n"
    "    gl_FragColor = texture2D(tex, (floor(texLoc) + scaled) / texDim);\n"
    "}\n";

static char errbuf[4096];

static GLuint
compileShader(GLenum shaderType, const char *src)
{
    GLuint shader = glCreateShader(shaderType);
    GLint status;

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        fprintf(stderr, "%s shader compilation failed!\n",
            shaderType == GL_VERTEX_SHADER ? "Vertex" : "Fragment");
        glGetShaderInfoLog(shader, 4096, NULL, errbuf);
        fprintf(stderr, "%s\n", errbuf);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint
compileShaderProgram(const char *vsSrc, const char *fsSrc)
{
    GLint status;
    GLuint vShader, fShader, program;

    vShader = compileShader(GL_VERTEX_SHADER, vsSrc);
    if (!vShader) {
        return 0;
    }
    fShader = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!fShader) {
        glDeleteShader(vShader);
        return 0;
    }

    program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    glDetachShader(program, vShader);
    glDetachShader(program, fShader);
    glDeleteShader(vShader);
    glDeleteShader(fShader);
    if (status == GL_FALSE) {
        fprintf(stderr, "Program linker failed!\n");
        glGetProgramInfoLog(program, 4096, NULL, errbuf);
        fprintf(stderr, "%s\n", errbuf);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

h13_win_t *
h13_init_opengl(const char *title, int w, int h, int tex_w, int tex_h)
{
    h13_ogl_win_t *result;
    unsigned char *pixels;
    SDL_Window *window;
    SDL_GLContext ctx;
    int legacy, version;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "Could not init SDL: %s\n", SDL_GetError());
        return NULL;
    }

    /* Try to create a core-profile OpenGL 3.2 context */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    legacy = 0;
    ctx = 0;
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window) ctx = SDL_GL_CreateContext(window);

    /* If creating a 3.2 context failed, back out and try again with 2.1 */
    if (!ctx) {
        legacy = 1;
        if (window) SDL_DestroyWindow(window);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (window) ctx = SDL_GL_CreateContext(window);
    }

    /* If that, too, has failed, give up */
    if (!ctx) {
        fprintf(stderr, "Could not initialize display: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return NULL;
    }

    /* Otherwise we'll be able to return something. Allocate the
     * actual structure and fill in the values we know already. */
    result = malloc(sizeof(h13_ogl_win_t));
    pixels = malloc(tex_w * tex_h * 4);
    if (!result || !pixels) {
        /* Well, OK, we can't return it if we can't allocate it. */
        if (result) free(result);
        if (pixels) free(pixels);
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return NULL;
    }

    memset(result, 0, sizeof(h13_ogl_win_t));
    memset(pixels, 0, tex_w * tex_h * 4);
    result->window = window;
    result->glContext = ctx;
    result->h13.pixels = pixels;
    result->h13.width = tex_w;
    result->h13.height = tex_h;
    result->w_current = w;
    result->h_current = h;
    result->w_requested = w;
    result->h_requested = h;
    result->aspect_image = (GLfloat)w / (GLfloat)h;
    result->aspect_screen = result->aspect_image;
    result->is_legacy = legacy;

    /* Set up the rendering context. */
    SDL_GL_MakeCurrent(window, ctx);
    version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    printf("GLAD reports OpenGL version %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

    /* Now create the various static elements of the display. */

    /* Vertex Attribute Object */
    if (!legacy) {
        /* 2.1 doesn't guarantee VAO support and also doesn't need it */
        glGenVertexArrays(1, &result->vao);
        glBindVertexArray(result->vao);
    }

    /* Vertex Buffer Object */
    glGenBuffers(1, &result->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, result->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(attribArray), attribArray, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Screen Texture */
    glGenTextures(1, &result->screenTexture);
    glBindTexture(GL_TEXTURE_2D, result->screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Shaders and their arguments */
    if (legacy)
        result->shaderProgram = compileShaderProgram(strVertexShader21, strFragmentShader21);
    else
        result->shaderProgram = compileShaderProgram(strVertexShader32, strFragmentShader32);
    result->posAttribute = glGetAttribLocation(result->shaderProgram, "pos");
    result->blendBorderUniform = glGetUniformLocation(result->shaderProgram, "blendBorder");
    result->scaleUniform = glGetUniformLocation(result->shaderProgram, "scale");
    result->texUniform = glGetUniformLocation(result->shaderProgram, "tex");
    result->texDimUniform = glGetUniformLocation(result->shaderProgram, "texDim");
    return &result->h13;
}

void
h13_uninit_opengl(h13_win_t *ctx)
{
    h13_ogl_win_t *win = (h13_ogl_win_t *)ctx;
    if (!win) return;
    if (win->glContext) SDL_GL_DeleteContext(win->glContext);
    if (win->window) SDL_DestroyWindow(win->window);
    if (win->h13.pixels) free(win->h13.pixels);
    free(win);
    SDL_Quit();
}

void
h13_flush_opengl(h13_win_t *ctx)
{
    h13_ogl_win_t *win = (h13_ogl_win_t *)ctx;
    if (!win) return;
    glBindTexture(GL_TEXTURE_2D, win->screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, win->h13.width, win->h13.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, win->h13.pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void
h13_render_opengl(h13_win_t *ctx)
{
    h13_ogl_win_t *win = (h13_ogl_win_t *)ctx;
    if (!win) return;
    if (win->w_current != win->w_requested || win->h_current != win->h_requested) {
        win->w_current = win->w_requested;
        win->h_current = win->h_requested;
        glViewport(0, 0, win->w_current, win->h_current);
        win->aspect_screen = (GLfloat)win->w_current / (GLfloat)win->h_current;
    }
    GLfloat s_x = 1.0f, s_y = 1.0f;
    if (win->aspect_screen < win->aspect_image) {
        s_y = win->aspect_screen / win->aspect_image;
    } else {
        s_x = win->aspect_image / win->aspect_screen;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindBuffer(GL_ARRAY_BUFFER, win->vbo);
    glVertexAttribPointer(win->posAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(win->posAttribute);

    glBindTexture(GL_TEXTURE_2D, win->screenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glUseProgram(win->shaderProgram);
    glUniform1f(win->blendBorderUniform, 0.7);
    glUniform2f(win->scaleUniform, s_x, s_y);
    glUniform1i(win->texUniform, 0);
    glUniform2f(win->texDimUniform, (GLfloat)win->h13.width, (GLfloat)win->h13.height);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(win->posAttribute);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    SDL_GL_SwapWindow(win->window);
}

void
h13_resize_opengl(h13_win_t *ctx, int w, int h)
{
    h13_ogl_win_t *win = (h13_ogl_win_t *)ctx;
    if (!win) return;
    win->w_requested = w;
    win->h_requested = h;
}
