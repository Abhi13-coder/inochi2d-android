#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "inochi2d.h"   // rename here if your generated header differs

#define LOG_TAG "inochi2d-app"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
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

    if (!e->inochiReady) {
        inInit(&timingFunc);
        e->inochiReady = true;
        LOGI("inInit() succeeded");
        // TODO once inochi2d.h is confirmed: inLoadPuppet("model.inp") from assets, then draw it.
    }

    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    return 0;
}

static void drawFrame(struct engine* e) {
    if (e->display == EGL_NO_DISPLAY) return;
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(e->display, e->surface);
}

static void termDisplay(struct engine* e) {
    if (e->inochiReady) {
        inCleanup();
        e->inochiReady = false;
    }
    if (e->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e->context != EGL_NO_CONTEXT) eglDestroyContext(e->display, e->context);
        if (e->surface != EGL_NO_SURFACE) eglDestroySurface(e->display, e->surface);
        eglTerminate(e->display);
    }
    e->display = EGL_NO_DISPLAY;
    e->context = EGL_NO_CONTEXT;
    e->surface = EGL_NO_SURFACE;
}

static void onAppCmd(struct android_app* app, int32_t cmd) {
    struct engine* e = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (e->app->window != NULL) initDisplay(e);
            break;
        case APP_CMD_TERM_WINDOW:
            termDisplay(e);
            break;
        default: break;
    }
}

void android_main(struct android_app* app) {
    struct engine engine;
    memset(&engine, 0, sizeof(engine));
    engine.display = EGL_NO_DISPLAY;
    engine.surface = EGL_NO_SURFACE;
    engine.context = EGL_NO_CONTEXT;
    app->userData = &engine;
    app->onAppCmd = onAppCmd;
    engine.app = app;

    while (1) {
        int events;
        struct android_poll_source* source;
        while (ALooper_pollAll(engine.display != EGL_NO_DISPLAY ? 0 : -1, NULL, &events, (void**)&source) >= 0) {
            if (source != NULL) source->process(app, source);
            if (app->destroyRequested != 0) {
                termDisplay(&engine);
                return;
            }
        }
        drawFrame(&engine);
    }
}
