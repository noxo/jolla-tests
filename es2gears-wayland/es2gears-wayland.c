/*
 * Copyright (C) 2011 Benjamin Franzke
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * Ported to GLES2.
 * Kristian Høgsberg <krh@bitplanet.net>
 * May 3, 2010
 *
 * Improve GLES2 port:
 *   * Refactor gear drawing.
 *   * Use correct normals for surfaces.
 *   * Improve shader.
 *   * Use perspective projection transformation.
 *   * Add FPS count.
 *   * Add comments.
 * Alexandros Frantzis <alexandros.frantzis@linaro.org>
 * Jul 13, 2010
 */

/*
 * Jan/2015 Erkki Nokso-Koivisto
 *
 * Merging Wayland example and es2gears
 *
 * es2gears:
 * ========
 * http://cgit.freedesktop.org/mesa/demos/tree/src/egl/opengles2/es2gears.c
 *
 * wayland example
 * ===============
 * https://raw.githubusercontent.com/4DA/glesv2-binary-shader/master/simple-egl.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_EXT_swap_buffers_with_damage
#define EGL_EXT_swap_buffers_with_damage 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
#endif

#ifndef EGL_EXT_buffer_age
#define EGL_EXT_buffer_age 1
#define EGL_BUFFER_AGE_EXT			0x313D
#endif

struct window;
struct seat;

struct display {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_touch *touch;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor *default_cursor;
    struct wl_surface *cursor_surface;
    struct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLConfig conf;
    } egl;
    struct window *window;

    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;
};

struct geometry {
    int width, height;
};

struct window {
    struct display *display;
    struct geometry geometry, window_size;
    struct {
        GLuint rotation_uniform;
        GLuint pos;
        GLuint col;
    } gl;

    uint32_t benchmark_time, frames;
    struct wl_egl_window *native;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    EGLSurface egl_surface;
    struct wl_callback *callback;
    int fullscreen, configured, opaque, buffer_size, frame_sync;
};

static int running = 1;

#define STRIPS_PER_TOOTH 7
#define VERTICES_PER_TOOTH 34
#define GEAR_VERTEX_STRIDE 6

/**
 * Struct describing the vertices in triangle strip
 */
struct vertex_strip {
    /** The first vertex in the strip */
    GLint first;
    /** The number of consecutive vertices in the strip after the first */
    GLint count;
};

/* Each vertex consist of GEAR_VERTEX_STRIDE GLfloat attributes */
typedef GLfloat GearVertex[GEAR_VERTEX_STRIDE];

/**
 * Struct representing a gear.
 */
struct gear {
    /** The array of vertices comprising the gear */
    GearVertex *vertices;
    /** The number of vertices comprising the gear */
    int nvertices;
    /** The array of triangle strips comprising the gear */
    struct vertex_strip *strips;
    /** The number of triangle strips comprising the gear */
    int nstrips;
    /** The Vertex Buffer Object holding the vertices in the graphics card */
    GLuint vbo;
};

/** The view rotation [x, y, z] */
static GLfloat view_rot[3] = { 20.0, 30.0, 0.0 };
/** The gears */
static struct gear *gear1, *gear2, *gear3;
/** The current gear rotation angle */
static GLfloat angle = 0.0;
/** The location of the shader uniforms */
static GLuint ModelViewProjectionMatrix_location,
NormalMatrix_location,
LightSourcePosition_location,
MaterialColor_location;
/** The projection matrix */
static GLfloat ProjectionMatrix[16];
/** The direction of the directional light for the scene */
static const GLfloat LightSourcePosition[4] = { 5.0, 5.0, 10.0, 1.0};

/**
 * Fills a gear vertex.
 *
 * @param v the vertex to fill
 * @param x the x coordinate
 * @param y the y coordinate
 * @param z the z coortinate
 * @param n pointer to the normal table
 *
 * @return the operation error code
 */
static GearVertex *
vert(GearVertex *v, GLfloat x, GLfloat y, GLfloat z, GLfloat n[3])
{
    v[0][0] = x;
    v[0][1] = y;
    v[0][2] = z;
    v[0][3] = n[0];
    v[0][4] = n[1];
    v[0][5] = n[2];

    return v + 1;
}

/**
 *  Create a gear wheel.
 *
 *  @param inner_radius radius of hole at center
 *  @param outer_radius radius at center of teeth
 *  @param width width of gear
 *  @param teeth number of teeth
 *  @param tooth_depth depth of tooth
 *
 *  @return pointer to the constructed struct gear
 */
static struct gear *
        create_gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
                    GLint teeth, GLfloat tooth_depth)
{
    GLfloat r0, r1, r2;
    GLfloat da;
    GearVertex *v;
    struct gear *gear;
    double s[5], c[5];
    GLfloat normal[3];
    int cur_strip = 0;
    int i;

    /* Allocate memory for the gear */
    gear = malloc(sizeof *gear);
    if (gear == NULL)
        return NULL;

    /* Calculate the radii used in the gear */
    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    r2 = outer_radius + tooth_depth / 2.0;

    da = 2.0 * M_PI / teeth / 4.0;

    /* Allocate memory for the triangle strip information */
    gear->nstrips = STRIPS_PER_TOOTH * teeth;
    gear->strips = calloc(gear->nstrips, sizeof (*gear->strips));

    /* Allocate memory for the vertices */
    gear->vertices = calloc(VERTICES_PER_TOOTH * teeth, sizeof(*gear->vertices));
    v = gear->vertices;

    for (i = 0; i < teeth; i++) {
        /* Calculate needed sin/cos for varius angles */
        sincos(i * 2.0 * M_PI / teeth, &s[0], &c[0]);
        sincos(i * 2.0 * M_PI / teeth + da, &s[1], &c[1]);
        sincos(i * 2.0 * M_PI / teeth + da * 2, &s[2], &c[2]);
        sincos(i * 2.0 * M_PI / teeth + da * 3, &s[3], &c[3]);
        sincos(i * 2.0 * M_PI / teeth + da * 4, &s[4], &c[4]);

        /* A set of macros for making the creation of the gears easier */
#define  GEAR_POINT(r, da) { (r) * c[(da)], (r) * s[(da)] }
#define  SET_NORMAL(x, y, z) do { \
    normal[0] = (x); normal[1] = (y); normal[2] = (z); \
    } while(0)

#define  GEAR_VERT(v, point, sign) vert((v), p[(point)].x, p[(point)].y, (sign) * width * 0.5, normal)

#define START_STRIP do { \
    gear->strips[cur_strip].first = v - gear->vertices; \
    } while(0);

#define END_STRIP do { \
    int _tmp = (v - gear->vertices); \
    gear->strips[cur_strip].count = _tmp - gear->strips[cur_strip].first; \
    cur_strip++; \
    } while (0)

#define QUAD_WITH_NORMAL(p1, p2) do { \
    SET_NORMAL((p[(p1)].y - p[(p2)].y), -(p[(p1)].x - p[(p2)].x), 0); \
    v = GEAR_VERT(v, (p1), -1); \
    v = GEAR_VERT(v, (p1), 1); \
    v = GEAR_VERT(v, (p2), -1); \
    v = GEAR_VERT(v, (p2), 1); \
    } while(0)

        struct point {
            GLfloat x;
            GLfloat y;
        };

        /* Create the 7 points (only x,y coords) used to draw a tooth */
        struct point p[7] = {
            GEAR_POINT(r2, 1), // 0
                    GEAR_POINT(r2, 2), // 1
                    GEAR_POINT(r1, 0), // 2
                    GEAR_POINT(r1, 3), // 3
                    GEAR_POINT(r0, 0), // 4
                    GEAR_POINT(r1, 4), // 5
                    GEAR_POINT(r0, 4), // 6
        };

        /* Front face */
        START_STRIP;
        SET_NORMAL(0, 0, 1.0);
        v = GEAR_VERT(v, 0, +1);
        v = GEAR_VERT(v, 1, +1);
        v = GEAR_VERT(v, 2, +1);
        v = GEAR_VERT(v, 3, +1);
        v = GEAR_VERT(v, 4, +1);
        v = GEAR_VERT(v, 5, +1);
        v = GEAR_VERT(v, 6, +1);
        END_STRIP;

        /* Inner face */
        START_STRIP;
        QUAD_WITH_NORMAL(4, 6);
        END_STRIP;

        /* Back face */
        START_STRIP;
        SET_NORMAL(0, 0, -1.0);
        v = GEAR_VERT(v, 6, -1);
        v = GEAR_VERT(v, 5, -1);
        v = GEAR_VERT(v, 4, -1);
        v = GEAR_VERT(v, 3, -1);
        v = GEAR_VERT(v, 2, -1);
        v = GEAR_VERT(v, 1, -1);
        v = GEAR_VERT(v, 0, -1);
        END_STRIP;

        /* Outer face */
        START_STRIP;
        QUAD_WITH_NORMAL(0, 2);
        END_STRIP;

        START_STRIP;
        QUAD_WITH_NORMAL(1, 0);
        END_STRIP;

        START_STRIP;
        QUAD_WITH_NORMAL(3, 1);
        END_STRIP;

        START_STRIP;
        QUAD_WITH_NORMAL(5, 3);
        END_STRIP;
    }

    gear->nvertices = (v - gear->vertices);

    /* Store the vertices in a vertex buffer object (VBO) */
    glGenBuffers(1, &gear->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gear->vbo);
    glBufferData(GL_ARRAY_BUFFER, gear->nvertices * sizeof(GearVertex),
                 gear->vertices, GL_STATIC_DRAW);

    return gear;
}

/**
 * Multiplies two 4x4 matrices.
 *
 * The result is stored in matrix m.
 *
 * @param m the first matrix to multiply
 * @param n the second matrix to multiply
 */
static void
multiply(GLfloat *m, const GLfloat *n)
{
    GLfloat tmp[16];
    const GLfloat *row, *column;
    div_t d;
    int i, j;

    for (i = 0; i < 16; i++) {
        tmp[i] = 0;
        d = div(i, 4);
        row = n + d.quot * 4;
        column = m + d.rem;
        for (j = 0; j < 4; j++)
            tmp[i] += row[j] * column[j * 4];
    }
    memcpy(m, &tmp, sizeof tmp);
}

/**
 * Rotates a 4x4 matrix.
 *
 * @param[in,out] m the matrix to rotate
 * @param angle the angle to rotate
 * @param x the x component of the direction to rotate to
 * @param y the y component of the direction to rotate to
 * @param z the z component of the direction to rotate to
 */
static void
rotate(GLfloat *m, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    double s, c;

    sincos(angle, &s, &c);
    GLfloat r[16] = {
        x * x * (1 - c) + c,     y * x * (1 - c) + z * s, x * z * (1 - c) - y * s, 0,
        x * y * (1 - c) - z * s, y * y * (1 - c) + c,     y * z * (1 - c) + x * s, 0,
        x * z * (1 - c) + y * s, y * z * (1 - c) - x * s, z * z * (1 - c) + c,     0,
        0, 0, 0, 1
    };

    multiply(m, r);
}


/**
 * Translates a 4x4 matrix.
 *
 * @param[in,out] m the matrix to translate
 * @param x the x component of the direction to translate to
 * @param y the y component of the direction to translate to
 * @param z the z component of the direction to translate to
 */
static void
translate(GLfloat *m, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat t[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 };

    multiply(m, t);
}

/**
 * Creates an identity 4x4 matrix.
 *
 * @param m the matrix make an identity matrix
 */
static void
identity(GLfloat *m)
{
    GLfloat t[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    };

    memcpy(m, t, sizeof(t));
}

/**
 * Transposes a 4x4 matrix.
 *
 * @param m the matrix to transpose
 */
static void
transpose(GLfloat *m)
{
    GLfloat t[16] = {
        m[0], m[4], m[8],  m[12],
        m[1], m[5], m[9],  m[13],
        m[2], m[6], m[10], m[14],
        m[3], m[7], m[11], m[15]};

    memcpy(m, t, sizeof(t));
}

/**
 * Inverts a 4x4 matrix.
 *
 * This function can currently handle only pure translation-rotation matrices.
 * Read http://www.gamedev.net/community/forums/topic.asp?topic_id=425118
 * for an explanation.
 */
static void
invert(GLfloat *m)
{
    GLfloat t[16];
    identity(t);

    // Extract and invert the translation part 't'. The inverse of a
    // translation matrix can be calculated by negating the translation
    // coordinates.
    t[12] = -m[12]; t[13] = -m[13]; t[14] = -m[14];

    // Invert the rotation part 'r'. The inverse of a rotation matrix is
    // equal to its transpose.
    m[12] = m[13] = m[14] = 0;
    transpose(m);

    // inv(m) = inv(r) * inv(t)
    multiply(m, t);
}

/**
 * Calculate a perspective projection transformation.
 *
 * @param m the matrix to save the transformation in
 * @param fovy the field of view in the y direction
 * @param aspect the view aspect ratio
 * @param zNear the near clipping plane
 * @param zFar the far clipping plane
 */
void perspective(GLfloat *m, GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
    GLfloat tmp[16];
    identity(tmp);

    double sine, cosine, cotangent, deltaZ;
    GLfloat radians = fovy / 2 * M_PI / 180;

    deltaZ = zFar - zNear;
    sincos(radians, &sine, &cosine);

    if ((deltaZ == 0) || (sine == 0) || (aspect == 0))
        return;

    cotangent = cosine / sine;

    tmp[0] = cotangent / aspect;
    tmp[5] = cotangent;
    tmp[10] = -(zFar + zNear) / deltaZ;
    tmp[11] = -1;
    tmp[14] = -2 * zNear * zFar / deltaZ;
    tmp[15] = 0;

    memcpy(m, tmp, sizeof(tmp));
}

/**
 * Draws a gear.
 *
 * @param gear the gear to draw
 * @param transform the current transformation matrix
 * @param x the x position to draw the gear at
 * @param y the y position to draw the gear at
 * @param angle the rotation angle of the gear
 * @param color the color of the gear
 */
static void
draw_gear(struct gear *gear, GLfloat *transform,
          GLfloat x, GLfloat y, GLfloat angle, const GLfloat color[4])
{
    GLfloat model_view[16];
    GLfloat normal_matrix[16];
    GLfloat model_view_projection[16];

    /* Translate and rotate the gear */
    memcpy(model_view, transform, sizeof (model_view));
    translate(model_view, x, y, 0);
    rotate(model_view, 2 * M_PI * angle / 360.0, 0, 0, 1);

    /* Create and set the ModelViewProjectionMatrix */
    memcpy(model_view_projection, ProjectionMatrix, sizeof(model_view_projection));
    multiply(model_view_projection, model_view);

    glUniformMatrix4fv(ModelViewProjectionMatrix_location, 1, GL_FALSE,
                       model_view_projection);

    /*
    * Create and set the NormalMatrix. It's the inverse transpose of the
    * ModelView matrix.
    */
    memcpy(normal_matrix, model_view, sizeof (normal_matrix));
    invert(normal_matrix);
    transpose(normal_matrix);
    glUniformMatrix4fv(NormalMatrix_location, 1, GL_FALSE, normal_matrix);

    /* Set the gear color */
    glUniform4fv(MaterialColor_location, 1, color);

    /* Set the vertex buffer object to use */
    glBindBuffer(GL_ARRAY_BUFFER, gear->vbo);

    /* Set up the position of the attributes in the vertex buffer object */
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(GLfloat), NULL);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(GLfloat), (GLfloat *) 0 + 3);

    /* Enable the attributes */
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    /* Draw the triangle strips that comprise the gear */
    int n;
    for (n = 0; n < gear->nstrips; n++)
        glDrawArrays(GL_TRIANGLE_STRIP, gear->strips[n].first, gear->strips[n].count);

    /* Disable the attributes */
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}

static void
init_egl(struct display *display, struct window *window)
{
    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    const char *extensions;

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_DEPTH_SIZE,1,
        EGL_NONE
    };

    EGLint major, minor, n, count, i, size;
    EGLConfig *configs;
    EGLBoolean ret;

    if (window->opaque || window->buffer_size == 16)
        config_attribs[9] = 0;

    display->egl.dpy = eglGetDisplay(display->display);
    assert(display->egl.dpy);

    ret = eglInitialize(display->egl.dpy, &major, &minor);
    assert(ret == EGL_TRUE);
    ret = eglBindAPI(EGL_OPENGL_ES_API);
    assert(ret == EGL_TRUE);

    if (!eglGetConfigs(display->egl.dpy, NULL, 0, &count) || count < 1)
        assert(0);

    configs = calloc(count, sizeof *configs);
    assert(configs);

    ret = eglChooseConfig(display->egl.dpy, config_attribs,
                          configs, count, &n);
    assert(ret && n >= 1);

    for (i = 0; i < n; i++) {
        eglGetConfigAttrib(display->egl.dpy,
                           configs[i], EGL_BUFFER_SIZE, &size);
        if (window->buffer_size == size) {
            display->egl.conf = configs[i];
            break;
        }
    }
    free(configs);
    if (display->egl.conf == NULL) {
        fprintf(stderr, "did not find config with buffer size %d\n",
                window->buffer_size);
        exit(EXIT_FAILURE);
    }

    display->egl.ctx = eglCreateContext(display->egl.dpy,
                                        display->egl.conf,
                                        EGL_NO_CONTEXT, context_attribs);
    assert(display->egl.ctx);

    display->swap_buffers_with_damage = NULL;
    extensions = eglQueryString(display->egl.dpy, EGL_EXTENSIONS);
    if (extensions &&
            strstr(extensions, "EGL_EXT_swap_buffers_with_damage") &&
            strstr(extensions, "EGL_EXT_buffer_age"))
        display->swap_buffers_with_damage =
                (PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
                eglGetProcAddress("eglSwapBuffersWithDamageEXT");

    if (display->swap_buffers_with_damage)
        printf("has EGL_EXT_buffer_age and EGL_EXT_swap_buffers_with_damage\n");

}

static void
fini_egl(struct display *display)
{
    eglTerminate(display->egl.dpy);
    eglReleaseThread();
}

static const char vertex_shader[] =
        "attribute vec3 position;\n"
        "attribute vec3 normal;\n"
        "\n"
        "uniform mat4 ModelViewProjectionMatrix;\n"
        "uniform mat4 NormalMatrix;\n"
        "uniform vec4 LightSourcePosition;\n"
        "uniform vec4 MaterialColor;\n"
        "\n"
        "varying vec4 Color;\n"
        "\n"
        "void main(void)\n"
        "{\n"
        "    // Transform the normal to eye coordinates\n"
        "    vec3 N = normalize(vec3(NormalMatrix * vec4(normal, 1.0)));\n"
        "\n"
        "    // The LightSourcePosition is actually its direction for directional light\n"
        "    vec3 L = normalize(LightSourcePosition.xyz);\n"
        "\n"
        "    // Multiply the diffuse value by the vertex color (which is fixed in this case)\n"
        "    // to get the actual color that we will use to draw this vertex with\n"
        "    float diffuse = max(dot(N, L), 0.0);\n"
        "    Color = diffuse * MaterialColor;\n"
        "\n"
        "    // Transform the position to clip coordinates\n"
        "    gl_Position = ModelViewProjectionMatrix * vec4(position, 1.0);\n"
        "}";

static const char fragment_shader[] =
        "precision mediump float;\n"
        "varying vec4 Color;\n"
        "\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = Color;\n"
        "}";

static void
init_gl(struct window *window)
{
    GLuint v, f, program;
    const char *p;
    char msg[512];

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    /* Compile the vertex shader */
    p = vertex_shader;
    v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &p, NULL);
    glCompileShader(v);
    glGetShaderInfoLog(v, sizeof msg, NULL, msg);
    printf("vertex shader info: %s\n", msg);

    /* Compile the fragment shader */
    p = fragment_shader;
    f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &p, NULL);
    glCompileShader(f);
    glGetShaderInfoLog(f, sizeof msg, NULL, msg);
    printf("fragment shader info: %s\n", msg);

    /* Create and link the shader program */
    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "normal");


    glLinkProgram(program);
    glGetProgramInfoLog(program, sizeof msg, NULL, msg);
    printf("info: %s\n", msg);

    /* Enable the shaders */
    glUseProgram(program);

    /* Get the locations of the uniforms so we can access them */
    ModelViewProjectionMatrix_location = glGetUniformLocation(program, "ModelViewProjectionMatrix");
    NormalMatrix_location = glGetUniformLocation(program, "NormalMatrix");
    LightSourcePosition_location = glGetUniformLocation(program, "LightSourcePosition");
    MaterialColor_location = glGetUniformLocation(program, "MaterialColor");

    /* Set the LightSourcePosition uniform which is constant throught the program */
    glUniform4fv(LightSourcePosition_location, 1, LightSourcePosition);

    /* make the gears */
    gear1 = create_gear(1.0, 4.0, 1.0, 20, 0.7);
    gear2 = create_gear(0.5, 2.0, 2.0, 10, 0.7);
    gear3 = create_gear(1.3, 2.0, 0.5, 10, 0.7);

}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
            uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
                 uint32_t edges, int32_t width, int32_t height)
{
    struct window *window = data;

    if (window->native)
        wl_egl_window_resize(window->native, width, height, 0, 0);

    window->geometry.width = width;
    window->geometry.height = height;

    if (!window->fullscreen)
        window->window_size = window->geometry;

    perspective(ProjectionMatrix, 60.0, width / (float)height, 1.0, 1024.0);
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done
};

static void
configure_callback(void *data, struct wl_callback *callback, uint32_t  time)
{
    struct window *window = data;

    wl_callback_destroy(callback);

    window->configured = 1;
}

static struct wl_callback_listener configure_callback_listener = {
    configure_callback,
};

static void
set_fullscreen(struct window *window, int fullscreen)
{
    struct wl_callback *callback;

    window->fullscreen = fullscreen;
    window->configured = 0;

    if (fullscreen) {
        wl_shell_surface_set_fullscreen(window->shell_surface,
                                        WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                        0, NULL);
        callback = wl_display_sync(window->display->display);
        wl_callback_add_listener(callback,
                                 &configure_callback_listener,
                                 window);

    } else {
        wl_shell_surface_set_toplevel(window->shell_surface);
        handle_configure(window, window->shell_surface, 0,
                         window->window_size.width,
                         window->window_size.height);
        window->configured = 1;
    }
}

static void
create_surface(struct window *window)
{
    struct display *display = window->display;
    EGLBoolean ret;

    window->surface = wl_compositor_create_surface(display->compositor);
    window->shell_surface = wl_shell_get_shell_surface(display->shell,
                                                       window->surface);

    wl_shell_surface_add_listener(window->shell_surface,
                                  &shell_surface_listener, window);

    window->native =
            wl_egl_window_create(window->surface,
                                 window->window_size.width,
                                 window->window_size.height);
    window->egl_surface =
            eglCreateWindowSurface(display->egl.dpy,
                                   display->egl.conf,
                                   window->native, NULL);

    wl_shell_surface_set_title(window->shell_surface, "simple-egl");

    ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
                         window->egl_surface, window->display->egl.ctx);
    assert(ret == EGL_TRUE);

    if (!window->frame_sync)
        eglSwapInterval(display->egl.dpy, 0);

    set_fullscreen(window, window->fullscreen);
}

static void
destroy_surface(struct window *window)
{
    /* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
     * on eglReleaseThread(). */
    eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    eglDestroySurface(window->display->egl.dpy, window->egl_surface);
    wl_egl_window_destroy(window->native);

    wl_shell_surface_destroy(window->shell_surface);
    wl_surface_destroy(window->surface);

    if (window->callback)
        wl_callback_destroy(window->callback);
}

static const struct wl_callback_listener frame_listener;

static void
calc_gear_angle(uint32_t time)
{
   static double tRot0 = -1.0;
   double dt, t = time / 1000.0;

   if (tRot0 < 0.0)
      tRot0 = t;
   dt = t - tRot0;
   tRot0 = t;

   /* advance rotation for next frame */
   angle += 70.0 * dt;  /* 70 degrees per second */
   if (angle > 3600.0)
      angle -= 3600.0;

}

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{

    struct window *window = data;
    struct display *display = window->display;
    static const int32_t speed_div = 5, benchmark_interval = 5;

    struct wl_region *region;
    EGLint rect[4];
    EGLint buffer_age = 0;
    struct timeval tv;

    assert(window->callback == callback);
    window->callback = NULL;

    if (callback)
        wl_callback_destroy(callback);

    if (!window->configured)
        return;

    gettimeofday(&tv, NULL);
    time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    calc_gear_angle(time);
    view_rot[1] -= 0.2;

    if (window->frames == 0)
        window->benchmark_time = time;

    if (time - window->benchmark_time > (benchmark_interval * 1000)) {
        printf("%d frames in %d seconds: %f fps\n",
               window->frames,
               benchmark_interval,
               (float) window->frames / benchmark_interval);
        window->benchmark_time = time;
        window->frames = 0;
    }


    if (display->swap_buffers_with_damage)
        eglQuerySurface(display->egl.dpy, window->egl_surface,
                        EGL_BUFFER_AGE_EXT, &buffer_age);

    glViewport(0, 0, window->geometry.width, window->geometry.height);

    const static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
    const static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
    const static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

    GLfloat transform[16];
    identity(transform);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Translate and rotate the view */
    translate(transform, 0, 0, -20);
    rotate(transform, 2 * M_PI * view_rot[0] / 360.0, 1, 0, 0);
    rotate(transform, 2 * M_PI * view_rot[1] / 360.0, 0, 1, 0);
    rotate(transform, 2 * M_PI * view_rot[2] / 360.0, 0, 0, 1);

    /* Draw the gears */
    draw_gear(gear1, transform, -3.0, -2.0, angle, red);
    draw_gear(gear2, transform, 3.1, -2.0, -2 * angle - 9.0, green);
    draw_gear(gear3, transform, -3.1, 4.2, -2 * angle - 25.0, blue);

    if (window->opaque || window->fullscreen) {
        region = wl_compositor_create_region(window->display->compositor);
        wl_region_add(region, 0, 0,
                      window->geometry.width,
                      window->geometry.height);
        wl_surface_set_opaque_region(window->surface, region);
        wl_region_destroy(region);
    } else {
        wl_surface_set_opaque_region(window->surface, NULL);
    }

    if (display->swap_buffers_with_damage && buffer_age > 0) {
        rect[0] = window->geometry.width / 4 - 1;
        rect[1] = window->geometry.height / 4 - 1;
        rect[2] = window->geometry.width / 2 + 2;
        rect[3] = window->geometry.height / 2 + 2;
        display->swap_buffers_with_damage(display->egl.dpy,
                                          window->egl_surface,
                                          rect, 1);
    } else {
        eglSwapBuffers(display->egl.dpy, window->egl_surface);
    }

    window->frames++;

}

static const struct wl_callback_listener frame_listener = {
    redraw
};

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
                     uint32_t serial, struct wl_surface *surface,
                     wl_fixed_t sx, wl_fixed_t sy)
{
    struct display *display = data;
    struct wl_buffer *buffer;
    struct wl_cursor *cursor = display->default_cursor;
    struct wl_cursor_image *image;

    if (display->window->fullscreen)
        wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
    else if (cursor) {
        image = display->default_cursor->images[0];
        buffer = wl_cursor_image_get_buffer(image);
        wl_pointer_set_cursor(pointer, serial,
                              display->cursor_surface,
                              image->hotspot_x,
                              image->hotspot_y);
        wl_surface_attach(display->cursor_surface, buffer, 0, 0);
        wl_surface_damage(display->cursor_surface, 0, 0,
                          image->width, image->height);
        wl_surface_commit(display->cursor_surface);
    }
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
                     uint32_t serial, struct wl_surface *surface)
{
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
                      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    printf("pointer_handle_motion sx=%d sy=%d", sx, sy);
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                      uint32_t serial, uint32_t time, uint32_t button,
                      uint32_t state)
{
    struct display *display = data;

    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
        wl_shell_surface_move(display->window->shell_surface,
                              display->seat, serial);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void
touch_handle_down(void *data, struct wl_touch *wl_touch,
                  uint32_t serial, uint32_t time, struct wl_surface *surface,
                  int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct display *d = (struct display *)data;

    wl_shell_surface_move(d->window->shell_surface, d->seat, serial);
}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch,
                uint32_t serial, uint32_t time, int32_t id)
{
}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch,
                    uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
}

static const struct wl_touch_listener touch_listener = {
    touch_handle_down,
    touch_handle_up,
    touch_handle_motion,
    touch_handle_frame,
    touch_handle_cancel,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface,
                      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface)
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                    uint32_t serial, uint32_t time, uint32_t key,
                    uint32_t state)
{
    struct display *d = data;

    if (key == KEY_F11 && state)
        set_fullscreen(d->window, d->window->fullscreen ^ 1);
    else if (key == KEY_ESC && state)
        running = 0;
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         enum wl_seat_capability caps)
{
    struct display *d = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
        d->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(d->pointer, &pointer_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
        wl_pointer_destroy(d->pointer);
        d->pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
        d->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
        wl_keyboard_destroy(d->keyboard);
        d->keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !d->touch) {
        d->touch = wl_seat_get_touch(seat);
        wl_touch_set_user_data(d->touch, d);
        wl_touch_add_listener(d->touch, &touch_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && d->touch) {
        wl_touch_destroy(d->touch);
        d->touch = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
                       uint32_t name, const char *interface, uint32_t version)
{
    struct display *d = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor =
                wl_registry_bind(registry, name,
                                 &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        d->shell = wl_registry_bind(registry, name,
                                    &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        d->seat = wl_registry_bind(registry, name,
                                   &wl_seat_interface, 1);
        wl_seat_add_listener(d->seat, &seat_listener, d);
    } else if (strcmp(interface, "wl_shm") == 0) {
        d->shm = wl_registry_bind(registry, name,
                                  &wl_shm_interface, 1);
        d->cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
        d->default_cursor =
                wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
                              uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void
signal_int(int signum)
{
    running = 0;
}

static void
usage(int error_code)
{
    fprintf(stderr, "Usage: simple-egl [OPTIONS]\n\n"
            "  -o\tCreate an opaque surface\n"
            "  -s\tUse a 16 bpp EGL config\n"
            "  -b\tDon't sync to compositor redraw (eglSwapInterval 0)\n"
            "  -h\tThis help text\n\n");

    exit(error_code);
}

int
main(int argc, char **argv)
{
    struct sigaction sigint;
    struct display display = { 0 };
    struct window  window  = { 0 };
    int i, ret = 0;

    window.display = &display;
    display.window = &window;
    window.window_size.width  = 250;
    window.window_size.height = 250;
    window.buffer_size = 32;
    window.frame_sync = 1;
    window.fullscreen = 1;

    for (i = 1; i < argc; i++) {
        if (strcmp("-o", argv[i]) == 0)
            window.opaque = 1;
        else if (strcmp("-s", argv[i]) == 0)
            window.buffer_size = 16;
        else if (strcmp("-b", argv[i]) == 0)
            window.frame_sync = 0;
        else if (strcmp("-h", argv[i]) == 0)
            usage(EXIT_SUCCESS);
        else
            usage(EXIT_FAILURE);
    }

    display.display = wl_display_connect(NULL);
    assert(display.display);

    display.registry = wl_display_get_registry(display.display);
    wl_registry_add_listener(display.registry,
                             &registry_listener, &display);

    wl_display_dispatch(display.display);

    init_egl(&display, &window);
    create_surface(&window);
    init_gl(&window);

    display.cursor_surface =
            wl_compositor_create_surface(display.compositor);

    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);

    /* The mainloop here is a little subtle.  Redrawing will cause
     * EGL to read events so we can just call
     * wl_display_dispatch_pending() to handle any events that got
     * queued up as a side effect. */
    while (running && ret != -1) {
        wl_display_dispatch_pending(display.display);
        while (!window.configured)
            wl_display_dispatch(display.display);
        redraw(&window, NULL, 0);
    }

    fprintf(stderr, "simple-egl exiting\n");

    destroy_surface(&window);
    fini_egl(&display);

    wl_surface_destroy(display.cursor_surface);
    if (display.cursor_theme)
        wl_cursor_theme_destroy(display.cursor_theme);

    if (display.shell)
        wl_shell_destroy(display.shell);

    if (display.compositor)
        wl_compositor_destroy(display.compositor);

    wl_registry_destroy(display.registry);
    wl_display_flush(display.display);
    wl_display_disconnect(display.display);

    return 0;
}
