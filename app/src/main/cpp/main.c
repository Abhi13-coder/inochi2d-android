<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
// Inochi2D puppet viewer — Android NativeActivity + GLES2.
//
// Loads the first *.inp model found in app/src/main/assets/, renders it
// via Inochi2D's "bring your own renderer" C FFI (include/inochi2d.h from
// the dynamic dub build config: https://github.com/Inochi2D/inochi2d),
// and lets you pan (one-finger drag) and zoom (two-finger pinch).
//
// Render-loop structure (texture upload, drawlist -> VBO/IBO, draw-command
// walk with composite/mask depth tracking) follows Inochi2D's own
// examples/basic-gl/src/main.c, adapted from desktop GL 3.2 to GLES2:
//   - GLES2 has no glDrawElementsBaseVertex, so we call
//     in_drawlist_set_use_base_vertex(drawlist, false) once, which makes
//     the drawlist bake absolute indices we can feed to plain glDrawElements.
//   - GLES2 has no GL_CLAMP_TO_BORDER (desktop-only); textures use
//     GL_CLAMP_TO_EDGE instead.
//   - Shaders are rewritten from GLSL 330/420 to GLSL ES 100
//     (attribute/varying, gl_FragColor, no layout(location=...) —
//     attribute locations are bound explicitly before linking instead).

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/input.h>
<<<<<<< HEAD
=======
=======
#include <android_native_app_glue.h>
#include <android/log.h>
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <time.h>
#include <string.h>
<<<<<<< HEAD
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
=======
<<<<<<< HEAD
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
=======
#include <stdbool.h>
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd

#include "inochi2d.h"   // rename here if your generated header differs

#define LOG_TAG "inochi2d-app"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------
// Tiny column-major mat4 (matches GLSL's default memory layout, so we
// upload with glUniformMatrix4fv(..., GL_FALSE, ...)).
// ---------------------------------------------------------------------
typedef struct { float m[16]; } mat4;

static mat4 mat4_identity(void) {
    mat4 r = {{ 1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1 }};
    return r;
}

static mat4 mat4_ortho(float l, float r, float b, float t, float n, float f) {
    mat4 out = mat4_identity();
    out.m[0]  = 2.0f / (r - l);
    out.m[5]  = 2.0f / (t - b);
    out.m[10] = -2.0f / (f - n);
    out.m[12] = -(r + l) / (r - l);
    out.m[13] = -(t + b) / (t - b);
    out.m[14] = -(f + n) / (f - n);
    return out;
}

static mat4 mat4_translate(float x, float y, float z) {
    mat4 out = mat4_identity();
    out.m[12] = x; out.m[13] = y; out.m[14] = z;
    return out;
}

static mat4 mat4_scale(float x, float y, float z) {
    mat4 out = mat4_identity();
    out.m[0] = x; out.m[5] = y; out.m[10] = z;
    return out;
}

static mat4 mat4_mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) sum += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

// ---------------------------------------------------------------------
// GLSL ES 100 shaders (ported from Inochi2D's examples/basic-gl shaders)
// ---------------------------------------------------------------------
static const char* kVertexShaderSrc =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "uniform mat4 uMVP;\n"
    "varying vec2 vUV;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char* kFragmentShaderSrc =
    "precision mediump float;\n"
    "varying vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(uTex, vUV);\n"
    "}\n";

#define ATTRIB_POS 0
#define ATTRIB_UV  1

// ---------------------------------------------------------------------
// Engine state
// ---------------------------------------------------------------------
struct engine {
    struct android_app* app;

    // EGL
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int width, height;

    // Inochi2D (CPU-side; survives EGL context recreation)
    bool puppetLoaded;
    in_puppet_t* puppet;
    in_drawlist_t* drawlist;

    // GL resources (GPU-side; recreated every time the EGL context is)
    bool glReady;
    GLuint program;
    GLint uMVPLoc, uTexLoc;
    GLuint vbo, ibo;
    GLuint* textureIds;
    uint32_t textureCount;

    // Camera: pan in screen pixels, zoom as a scale factor.
    float panX, panY, zoom;

    // Touch state
    bool dragging;
    float lastX, lastY;
    bool pinching;
    float lastPinchDist;
};

// ---------------------------------------------------------------------
// Shader helpers
// ---------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 1) {
            char* log = (char*)malloc((size_t)logLen);
            glGetShaderInfoLog(shader, logLen, NULL, log);
            LOGE("shader compile failed: %s", log);
            free(log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint buildProgram(void) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (!vs || !fs) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    // GLSL ES 100 has no layout(location=...); bind attribute locations
    // explicitly before linking instead.
    glBindAttribLocation(program, ATTRIB_POS, "aPos");
    glBindAttribLocation(program, ATTRIB_UV, "aUV");
    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (status != GL_TRUE) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 1) {
            char* log = (char*)malloc((size_t)logLen);
            glGetProgramInfoLog(program, logLen, NULL, log);
            LOGE("program link failed: %s", log);
            free(log);
        }
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// ---------------------------------------------------------------------
// Inochi2D message sink -> logcat
// ---------------------------------------------------------------------
static void sinkInfo(const char* msg, const char* file, uint32_t line) {
    LOGI("inochi2d %s:%u: %s", file ? file : "?", line, msg ? msg : "");
}
static void sinkWarning(const char* msg, const char* file, uint32_t line) {
    LOGW("inochi2d %s:%u: %s", file ? file : "?", line, msg ? msg : "");
}
static void sinkError(const char* msg, const char* file, uint32_t line) {
    LOGE("inochi2d %s:%u: %s", file ? file : "?", line, msg ? msg : "");
}
static io_sink_t g_sink = { &sinkError, &sinkWarning, &sinkInfo };

// ---------------------------------------------------------------------
// Asset loading: grab the first *.inp found in app/src/main/assets/
// ---------------------------------------------------------------------
static bool findFirstInpAsset(AAssetManager* mgr, char* outName, size_t outSize) {
    AAssetDir* dir = AAssetManager_openDir(mgr, "");
    if (!dir) return false;

    bool found = false;
    const char* name;
    while ((name = AAssetDir_getNextFileName(dir)) != NULL) {
        size_t len = strlen(name);
        if (len > 4 &&
            (name[len - 4] == '.') &&
            (name[len - 3] == 'i' || name[len - 3] == 'I') &&
            (name[len - 2] == 'n' || name[len - 2] == 'N') &&
            (name[len - 1] == 'p' || name[len - 1] == 'P')) {
            strncpy(outName, name, outSize - 1);
            outName[outSize - 1] = '\0';
            found = true;
            break;
        }
    }
    AAssetDir_close(dir);
    return found;
}

static in_puppet_t* loadPuppetFromAssets(AAssetManager* mgr) {
    char assetName[256];
    if (!findFirstInpAsset(mgr, assetName, sizeof(assetName))) {
        LOGW("No .inp model found in app/src/main/assets/ - drop one there "
             "(e.g. assets/model.inp) to see it render. Showing a blank "
             "screen until then.");
        return NULL;
    }

    AAsset* asset = AAssetManager_open(mgr, assetName, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open asset '%s'", assetName);
        return NULL;
    }

    off64_t length = AAsset_getLength64(asset);
    const void* data = AAsset_getBuffer(asset); // valid until AAsset_close
    if (!data || length <= 0) {
        LOGE("Failed to read asset '%s'", assetName);
        AAsset_close(asset);
        return NULL;
    }

    in_puppet_t* puppet =
        in_puppet_load_from_memory((const uint8_t*)data, (uint32_t)length, &g_sink);
    AAsset_close(asset);

    if (!puppet) {
        LOGE("in_puppet_load_from_memory('%s') failed", assetName);
    } else {
        LOGI("Loaded puppet '%s'", assetName);
    }
    return puppet;
}

// ---------------------------------------------------------------------
// GL (GPU-side) resource setup / teardown — tied to the EGL context's
// lifetime, independent of whether a puppet is loaded.
// ---------------------------------------------------------------------
static void uploadTextures(struct engine* e) {
    if (!e->puppet) return;

    in_texture_cache_t* cache = in_puppet_get_texture_cache(e->puppet);
    uint32_t count = 0;
    in_texture_t** textures = in_texture_cache_get_textures(cache, &count);

    e->textureIds = (GLuint*)calloc(count, sizeof(GLuint));
    e->textureCount = count;

    for (uint32_t i = 0; i < count; i++) {
        in_texture_t* tex = textures[i];
        uint32_t w = in_texture_get_width(tex);
        uint32_t h = in_texture_get_height(tex);
        uint32_t channels = in_texture_get_channels(tex);
        GLenum format = (channels == 3) ? GL_RGB : GL_RGBA;

        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, (GLsizei)w, (GLsizei)h,
                     0, format, GL_UNSIGNED_BYTE, in_texture_get_pixels(tex));
        // GLES2 has no GL_CLAMP_TO_BORDER — use CLAMP_TO_EDGE instead.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        e->textureIds[i] = id;
        // Stash the GL texture id on the resource so the draw-command loop
        // can look it up later, mirroring Inochi2D's own basic-gl example.
        in_resource_set_id((in_resource_t*)tex, (void*)(uintptr_t)id);
    }
}

static void setBlendMode(in_blend_mode_t mode) {
    switch (mode) {
        case IN_BLEND_MODE_MULTIPLY:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case IN_BLEND_MODE_SCREEN:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
            break;
        case IN_BLEND_MODE_LINEAR_DODGE:
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case IN_BLEND_MODE_ADD_GLOW:
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case IN_BLEND_MODE_NORMAL:
        default:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
}

static bool setupGL(struct engine* e) {
    e->program = buildProgram();
    if (!e->program) return false;

    e->uMVPLoc = glGetUniformLocation(e->program, "uMVP");
    e->uTexLoc = glGetUniformLocation(e->program, "uTex");

    glGenBuffers(1, &e->vbo);
    glGenBuffers(1, &e->ibo);

    if (e->puppet) {
        uploadTextures(e);
        if (!e->drawlist) {
            e->drawlist = in_puppet_get_drawlist(e->puppet);
            // GLES2 has no glDrawElementsBaseVertex: bake absolute indices
            // into the drawlist so plain glDrawElements works.
            in_drawlist_set_use_base_vertex(e->drawlist, false);
        }
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);

    e->glReady = true;
    return true;
}

static void teardownGL(struct engine* e) {
    if (e->textureIds) {
        glDeleteTextures((GLsizei)e->textureCount, e->textureIds);
        free(e->textureIds);
        e->textureIds = NULL;
        e->textureCount = 0;
    }
    if (e->vbo) { glDeleteBuffers(1, &e->vbo); e->vbo = 0; }
    if (e->ibo) { glDeleteBuffers(1, &e->ibo); e->ibo = 0; }
    if (e->program) { glDeleteProgram(e->program); e->program = 0; }
    e->glReady = false;
}

// ---------------------------------------------------------------------
// EGL display lifecycle
// ---------------------------------------------------------------------
static int initDisplay(struct engine* e) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
<<<<<<< HEAD
=======
=======
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    bool inochiReady;
};

static double timingFunc(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int initDisplay(struct engine* e) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(e->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, e->app->window, NULL);
    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext context = eglCreateContext(display, config, NULL, ctxAttribs);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("eglMakeCurrent failed");
        return -1;
    }

    e->display = display;
    e->surface = surface;
    e->context = context;
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    eglQuerySurface(display, surface, EGL_WIDTH, &e->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &e->height);

    if (!e->puppetLoaded) {
        e->puppet = loadPuppetFromAssets(e->app->activity->assetManager);
        e->puppetLoaded = true; // true = "we tried"; e->puppet may be NULL
        e->panX = e->width * 0.5f;
        e->panY = e->height * 0.5f;
        e->zoom = 1.0f;
    }

    setupGL(e);
<<<<<<< HEAD
=======
=======

    if (!e->inochiReady) {
        inInit(&timingFunc);
        e->inochiReady = true;
        LOGI("inInit() succeeded");
        // TODO once inochi2d.h is confirmed: inLoadPuppet("model.inp") from assets, then draw it.
    }

    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    return 0;
}

static void drawFrame(struct engine* e) {
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    if (e->display == EGL_NO_DISPLAY || !e->glReady) return;

    static struct timespec lastTime = {0, 0};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    float dt = 0.0f;
    if (lastTime.tv_sec != 0) {
        dt = (float)(now.tv_sec - lastTime.tv_sec) +
             (float)(now.tv_nsec - lastTime.tv_nsec) / 1e9f;
        if (dt < 0.0f || dt > 0.25f) dt = 0.0f; // clamp huge gaps (e.g. backgrounded)
    }
    lastTime = now;

    glViewport(0, 0, e->width, e->height);
    glClear(GL_COLOR_BUFFER_BIT);

    if (e->puppet && e->drawlist) {
        in_puppet_update(e->puppet, dt);
        in_puppet_draw(e->puppet, dt);

        uint32_t vtxBytes = 0, idxBytes = 0;
        in_vtxdata_t* vertices = in_drawlist_get_vertex_data(e->drawlist, &vtxBytes);
        void* indices = in_drawlist_get_index_data(e->drawlist, &idxBytes);

        glBindBuffer(GL_ARRAY_BUFFER, e->vbo);
        glBufferData(GL_ARRAY_BUFFER, vtxBytes, vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, e->ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxBytes, indices, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(ATTRIB_POS);
        glVertexAttribPointer(ATTRIB_POS, 2, GL_FLOAT, GL_FALSE,
                               sizeof(in_vtxdata_t), (void*)0);
        glEnableVertexAttribArray(ATTRIB_UV);
        glVertexAttribPointer(ATTRIB_UV, 2, GL_FLOAT, GL_FALSE,
                               sizeof(in_vtxdata_t), (void*)(sizeof(float) * 2));

        // Screen-space camera: drag to pan, pinch to zoom.
        mat4 proj = mat4_ortho(0, (float)e->width, (float)e->height, 0, -1.0f, 1.0f);
        mat4 view = mat4_mul(mat4_translate(e->panX, e->panY, 0.0f),
                              mat4_scale(e->zoom, e->zoom, 1.0f));
        mat4 mvp = mat4_mul(proj, view);

        glUseProgram(e->program);
        glUniformMatrix4fv(e->uMVPLoc, 1, GL_FALSE, mvp.m);
        glUniform1i(e->uTexLoc, 0);
        glActiveTexture(GL_TEXTURE0);

        uint32_t cmdCount = 0;
        in_drawcmd_t* cmds = in_drawlist_get_commands(e->drawlist, &cmdCount);
        int compositeDepth = 0;
        for (uint32_t i = 0; i < cmdCount; i++) {
            switch (cmds[i].state) {
                case IN_DRAW_STATE_NORMAL: {
                    if (compositeDepth > 0) break; // this basic renderer skips composites
                    GLuint texId = (GLuint)(uintptr_t)in_resource_get_id((in_resource_t*)cmds[i].sources[0]);
                    glBindTexture(GL_TEXTURE_2D, texId);
                    setBlendMode(cmds[i].blendMode);
                    size_t byteOffset = (size_t)cmds[i].idxOffset * 4; // indices are uint32
                    glDrawElements(GL_TRIANGLES, (GLsizei)cmds[i].elemCount,
                                   GL_UNSIGNED_INT, (void*)byteOffset);
                    break;
                }
                case IN_DRAW_STATE_COMPOSITE_BEGIN: compositeDepth++; break;
                case IN_DRAW_STATE_COMPOSITE_END:   compositeDepth--; break;
                default: break; // masks: not handled by this basic renderer
            }
        }
    }

<<<<<<< HEAD
=======
=======
    if (e->display == EGL_NO_DISPLAY) return;
    glClear(GL_COLOR_BUFFER_BIT);
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    eglSwapBuffers(e->display, e->surface);
}

static void termDisplay(struct engine* e) {
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    if (e->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        teardownGL(e);
        e->drawlist = NULL; // owned by the puppet; just drop our cached pointer
<<<<<<< HEAD
=======
=======
    if (e->inochiReady) {
        inCleanup();
        e->inochiReady = false;
    }
    if (e->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
        if (e->context != EGL_NO_CONTEXT) eglDestroyContext(e->display, e->context);
        if (e->surface != EGL_NO_SURFACE) eglDestroySurface(e->display, e->surface);
        eglTerminate(e->display);
    }
    e->display = EGL_NO_DISPLAY;
    e->context = EGL_NO_CONTEXT;
    e->surface = EGL_NO_SURFACE;
}

<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
static void freePuppet(struct engine* e) {
    if (e->puppet) {
        in_puppet_free(e->puppet);
        e->puppet = NULL;
    }
    e->drawlist = NULL;
    e->puppetLoaded = false;
}

// ---------------------------------------------------------------------
// Touch input: one-finger drag to pan, two-finger pinch to zoom.
// ---------------------------------------------------------------------
static float pointerDistance(AInputEvent* event, size_t a, size_t b) {
    float dx = AMotionEvent_getX(event, a) - AMotionEvent_getX(event, b);
    float dy = AMotionEvent_getY(event, a) - AMotionEvent_getY(event, b);
    return sqrtf(dx * dx + dy * dy);
}

static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    struct engine* e = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    size_t pointerCount = AMotionEvent_getPointerCount(event);

    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            if (pointerCount >= 2) {
                e->pinching = true;
                e->dragging = false;
                e->lastPinchDist = pointerDistance(event, 0, 1);
            } else {
                e->dragging = true;
                e->pinching = false;
                e->lastX = AMotionEvent_getX(event, 0);
                e->lastY = AMotionEvent_getY(event, 0);
            }
            return 1;

        case AMOTION_EVENT_ACTION_MOVE:
            if (e->pinching && pointerCount >= 2) {
                float d = pointerDistance(event, 0, 1);
                if (e->lastPinchDist > 1.0f) {
                    float factor = d / e->lastPinchDist;
                    e->zoom *= factor;
                    if (e->zoom < 0.05f) e->zoom = 0.05f;
                    if (e->zoom > 20.0f) e->zoom = 20.0f;
                }
                e->lastPinchDist = d;
            } else if (e->dragging) {
                float x = AMotionEvent_getX(event, 0);
                float y = AMotionEvent_getY(event, 0);
                e->panX += (x - e->lastX);
                e->panY += (y - e->lastY);
                e->lastX = x;
                e->lastY = y;
            }
            return 1;

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            e->dragging = false;
            e->pinching = false;
            return 1;

        default:
            return 0;
    }
}

<<<<<<< HEAD
=======
=======
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
static void onAppCmd(struct android_app* app, int32_t cmd) {
    struct engine* e = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (e->app->window != NULL) initDisplay(e);
            break;
        case APP_CMD_TERM_WINDOW:
            termDisplay(e);
            break;
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
        case APP_CMD_DESTROY:
            freePuppet(e);
            break;
        default:
            break;
<<<<<<< HEAD
=======
=======
        default: break;
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    }
}

void android_main(struct android_app* app) {
    struct engine engine;
    memset(&engine, 0, sizeof(engine));
    engine.display = EGL_NO_DISPLAY;
    engine.surface = EGL_NO_SURFACE;
    engine.context = EGL_NO_CONTEXT;
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    engine.zoom = 1.0f;
    app->userData = &engine;
    app->onAppCmd = onAppCmd;
    app->onInputEvent = handleInput;
<<<<<<< HEAD
=======
=======
    app->userData = &engine;
    app->onAppCmd = onAppCmd;
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
    engine.app = app;

    while (1) {
        int events;
        struct android_poll_source* source;
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
        int pollTimeoutMillis = engine.display != EGL_NO_DISPLAY ? 0 : -1;
        int pollResult;
        while ((pollResult = ALooper_pollOnce(pollTimeoutMillis, NULL, &events, (void**)&source)) >= 0) {
            if (pollResult == ALOOPER_POLL_ERROR) {
                LOGE("ALooper_pollOnce returned an error");
                break;
            }
            if (source != NULL) source->process(app, source);
            if (app->destroyRequested != 0) {
                termDisplay(&engine);
                freePuppet(&engine);
                return;
            }
            pollTimeoutMillis = engine.display != EGL_NO_DISPLAY ? 0 : -1;
<<<<<<< HEAD
=======
=======
        while (ALooper_pollAll(engine.display != EGL_NO_DISPLAY ? 0 : -1, NULL, &events, (void**)&source) >= 0) {
            if (source != NULL) source->process(app, source);
            if (app->destroyRequested != 0) {
                termDisplay(&engine);
                return;
            }
>>>>>>> 1f70925c0ba4d18a444b36074540a44005b4e891
>>>>>>> a1ead2375bd5fe693bf46f0a3e26dc0b054592bd
        }
        drawFrame(&engine);
    }
}
