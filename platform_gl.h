#pragma once
// Cross-platform OpenGL context + function loader for shadert0y.
// Linux = EGL, Windows = WGL, macOS = CGL.

#include <cstdio>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <GL/gl.h>
  #include <GL/glext.h>

  // WGL core-profile extension (not in gl.h)
  typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
  #ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
  #define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
  #define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
  #define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
  #define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
  #endif
#elif defined(__APPLE__)
  #include <OpenGL/gl3.h>
  #include <OpenGL/glext.h>
  #include <OpenGL/OpenGL.h>
  #include <dlfcn.h>
#else
  #include <EGL/egl.h>
  #include <EGL/eglext.h>
  #include <GL/gl.h>
  #include <GL/glext.h>
  #include <dlfcn.h>
#endif

// Portable function-pointer typedefs (don't rely on PFNGL* existing everywhere).
typedef GLuint (*PFN_createshader_t)(GLenum);
typedef void   (*PFN_shadersource_t)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (*PFN_compileshader_t)(GLuint);
typedef void   (*PFN_getshaderiv_t)(GLuint, GLenum, GLint*);
typedef void   (*PFN_getshaderinfolog_t)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (*PFN_createprogram_t)(void);
typedef void   (*PFN_attachshader_t)(GLuint, GLuint);
typedef void   (*PFN_linkprogram_t)(GLuint);
typedef void   (*PFN_getprogramiv_t)(GLuint, GLenum, GLint*);
typedef void   (*PFN_getprograminfolog_t)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (*PFN_useprogram_t)(GLuint);
typedef void   (*PFN_deleteshader_t)(GLuint);
typedef void   (*PFN_deleteprogram_t)(GLuint);
typedef void   (*PFN_genframebuffers_t)(GLsizei, GLuint*);
typedef void   (*PFN_bindframebuffer_t)(GLenum, GLuint);
typedef void   (*PFN_framebuffertexture2d_t)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (*PFN_checkframebufferstatus_t)(GLenum);
typedef void   (*PFN_deleteframebuffers_t)(GLsizei, const GLuint*);
typedef void   (*PFN_genvertexarrays_t)(GLsizei, GLuint*);
typedef void   (*PFN_bindvertexarray_t)(GLuint);
typedef void   (*PFN_deletevertexarrays_t)(GLsizei, const GLuint*);
typedef GLint  (*PFN_getuniformlocation_t)(GLuint, const GLchar*);
typedef void   (*PFN_uniform1f_t)(GLint, GLfloat);
typedef void   (*PFN_uniform1i_t)(GLint, GLint);
typedef void   (*PFN_uniform3f_t)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (*PFN_uniform4f_t)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (*PFN_uniform1fv_t)(GLint, GLsizei, const GLfloat*);
typedef void   (*PFN_uniform3fv_t)(GLint, GLsizei, const GLfloat*);
typedef void   (*PFN_activetexture_t)(GLenum);
typedef void   (*PFN_blitframebuffer_t)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);

namespace gl {

#define GL_FUNC_LIST \
    X(PFN_createshader_t,          glCreateShader) \
    X(PFN_shadersource_t,          glShaderSource) \
    X(PFN_compileshader_t,         glCompileShader) \
    X(PFN_getshaderiv_t,           glGetShaderiv) \
    X(PFN_getshaderinfolog_t,      glGetShaderInfoLog) \
    X(PFN_createprogram_t,         glCreateProgram) \
    X(PFN_attachshader_t,          glAttachShader) \
    X(PFN_linkprogram_t,           glLinkProgram) \
    X(PFN_getprogramiv_t,          glGetProgramiv) \
    X(PFN_getprograminfolog_t,     glGetProgramInfoLog) \
    X(PFN_useprogram_t,            glUseProgram) \
    X(PFN_deleteshader_t,          glDeleteShader) \
    X(PFN_deleteprogram_t,         glDeleteProgram) \
    X(PFN_genframebuffers_t,       glGenFramebuffers) \
    X(PFN_bindframebuffer_t,       glBindFramebuffer) \
    X(PFN_framebuffertexture2d_t,  glFramebufferTexture2D) \
    X(PFN_checkframebufferstatus_t,glCheckFramebufferStatus) \
    X(PFN_deleteframebuffers_t,    glDeleteFramebuffers) \
    X(PFN_genvertexarrays_t,       glGenVertexArrays) \
    X(PFN_bindvertexarray_t,       glBindVertexArray) \
    X(PFN_deletevertexarrays_t,    glDeleteVertexArrays) \
    X(PFN_getuniformlocation_t,    glGetUniformLocation) \
    X(PFN_uniform1f_t,             glUniform1f) \
    X(PFN_uniform1i_t,             glUniform1i) \
    X(PFN_uniform3f_t,             glUniform3f) \
    X(PFN_uniform4f_t,             glUniform4f) \
    X(PFN_uniform1fv_t,            glUniform1fv) \
    X(PFN_uniform3fv_t,            glUniform3fv) \
    X(PFN_activetexture_t,         glActiveTexture) \
    X(PFN_blitframebuffer_t,       glBlitFramebuffer)

#define X(type, name) type name = nullptr;
GL_FUNC_LIST
#undef X

static void* getProc(const char* name) {
#if defined(_WIN32)
    void* p = (void*)wglGetProcAddress(name);
    if (!p) p = (void*)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
    return p;
#elif defined(__APPLE__)
    return dlsym(RTLD_DEFAULT, name);
#else
    void* p = (void*)eglGetProcAddress(name);
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    return p;
#endif
}

static bool loadAll() {
#define X(type, name) \
    name = (type)getProc(#name); \
    if (!name) { fprintf(stderr, "[shadert0y] missing GL function: %s\n", #name); return false; }
    GL_FUNC_LIST
#undef X
    return true;
}

} // namespace gl

// ---------------------------------------------------------------------------
// Platform context
// ---------------------------------------------------------------------------
struct PlatformGL {
#if defined(_WIN32)
    HWND  hwnd  = nullptr;
    HDC   hdc   = nullptr;
    HGLRC hglrc = nullptr;
#elif defined(__APPLE__)
    CGLContextObj ctx = nullptr;
#else
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
#endif
    bool valid = false;
};

#if defined(_WIN32)
static LRESULT CALLBACK shadert0y_WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcA(h, m, w, l);
}
static bool platformInit(PlatformGL& pg, int width, int height) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc   = shadert0y_WndProc;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.lpszClassName = "shadert0y_hidden";
    RegisterClassA(&wc);

    pg.hwnd = CreateWindowExA(0, wc.lpszClassName, "shadert0y", WS_OVERLAPPEDWINDOW,
                              0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    if (!pg.hwnd) return false;

    pg.hdc = GetDC(pg.hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    int pf = ChoosePixelFormat(pg.hdc, &pfd);
    if (!pf) return false;
    SetPixelFormat(pg.hdc, pf, &pfd);

    HGLRC dummy = wglCreateContext(pg.hdc);
    if (!dummy) return false;
    wglMakeCurrent(pg.hdc, dummy);

    auto createAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    if (createAttribs) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        pg.hglrc = createAttribs(pg.hdc, nullptr, attribs);
    }
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummy);
    if (!pg.hglrc) return false;

    if (!wglMakeCurrent(pg.hdc, pg.hglrc)) return false;
    pg.valid = true;
    return true;
}
static bool platformMakeCurrent(PlatformGL& pg) {
    return wglMakeCurrent(pg.hdc, pg.hglrc) == TRUE;
}
static void platformShutdown(PlatformGL& pg) {
    if (pg.hglrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(pg.hglrc); }
    if (pg.hwnd)  { if (pg.hdc) ReleaseDC(pg.hwnd, pg.hdc); DestroyWindow(pg.hwnd); }
    pg = PlatformGL();
}

#elif defined(__APPLE__)
static bool platformInit(PlatformGL& pg, int width, int height) {
    CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
        kCGLPFAAccelerated,
        (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatObj pf = nullptr;
    GLint npix = 0;
    if (CGLChoosePixelFormat(attribs, &pf, &npix) != kCGLNoError || !pf) return false;
    if (CGLCreateContext(pf, nullptr, &pg.ctx) != kCGLNoError || !pg.ctx) {
        CGLDestroyPixelFormat(pf);
        return false;
    }
    CGLDestroyPixelFormat(pf);
    if (CGLSetCurrentContext(pg.ctx) != kCGLNoError) return false;
    pg.valid = true;
    return true;
}
static bool platformMakeCurrent(PlatformGL& pg) {
    return CGLSetCurrentContext(pg.ctx) == kCGLNoError;
}
static void platformShutdown(PlatformGL& pg) {
    if (pg.ctx) { CGLClearContext(pg.ctx); CGLDestroyContext(pg.ctx); }
    pg = PlatformGL();
}

#else // Linux / EGL
static bool platformInit(PlatformGL& pg, int width, int height) {
    pg.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (pg.display == EGL_NO_DISPLAY) return false;
    EGLint major, minor;
    if (!eglInitialize(pg.display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API)) return false;

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE
    };
    EGLConfig config; EGLint numConfigs = 0;
    if (!eglChooseConfig(pg.display, configAttribs, &config, 1, &numConfigs) || numConfigs < 1) return false;

    const EGLint pbufAttribs[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };
    pg.surface = eglCreatePbufferSurface(pg.display, config, pbufAttribs);
    if (pg.surface == EGL_NO_SURFACE) return false;

    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE
    };
    pg.context = eglCreateContext(pg.display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (pg.context == EGL_NO_CONTEXT) return false;

    if (!eglMakeCurrent(pg.display, pg.surface, pg.surface, pg.context)) return false;
    pg.valid = true;
    return true;
}
static bool platformMakeCurrent(PlatformGL& pg) {
    return eglMakeCurrent(pg.display, pg.surface, pg.surface, pg.context) == EGL_TRUE;
}
static void platformShutdown(PlatformGL& pg) {
    if (pg.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(pg.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (pg.context != EGL_NO_CONTEXT) eglDestroyContext(pg.display, pg.context);
        if (pg.surface != EGL_NO_SURFACE) eglDestroySurface(pg.display, pg.surface);
        eglTerminate(pg.display);
    }
    pg = PlatformGL();
}
#endif
