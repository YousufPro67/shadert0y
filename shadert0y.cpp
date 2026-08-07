#include "platform_gl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iterator>
#include <cctype>
#include <sys/stat.h>
#include <ctime>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern "C" {
#include "frei0r.h"
}

// Default tiny shader
// ---------------------------------------------------------------------------
static const char* kDefaultShader = R"GLSL(
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = vec4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
)GLSL";

static const char* kVertexSrc = R"GLSL(
#version 330 core
void main() {
    vec2 pos[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}
)GLSL";

static const char* kFragPreamble = R"GLSL(
#version 330 core

out vec4 _f0r_outColor;

uniform vec3       iResolution;
uniform float      iTime;
uniform float      iTimeDelta;
uniform int        iFrame;
uniform vec4       iMouse;
uniform vec3       iChannelResolution[4];
uniform float      iChannelTime[4];

uniform sampler2D  iChannel0;
uniform sampler2D  iChannel1;
uniform sampler2D  iChannel2;
uniform sampler2D  iChannel3;

uniform float      iParam0;
uniform float      iParam1;
uniform float      iParam2;
uniform float      iParam3;
uniform float      iParam4;
uniform float      iParam5;
uniform float      iParam6;
uniform float      iParam7;

uniform float      f0rUseShaderAlpha;
)GLSL";

static const char* kFragEpilogue = R"GLSL(
void main() {
    mainImage(_f0r_outColor, gl_FragCoord.xy);
    if (f0rUseShaderAlpha < 0.5) {
        _f0r_outColor.a = 1.0;
    }
}
)GLSL";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void flipRowsRGBA(uint32_t* buf, unsigned w, unsigned h) {
    std::vector<uint32_t> row(w);
    for (unsigned y = 0; y < h / 2; ++y) {
        uint32_t* top = buf + (size_t)y * w;
        uint32_t* bot = buf + (size_t)(h - 1 - y) * w;
        memcpy(row.data(), top, (size_t)w * 4);
        memcpy(top, bot, (size_t)w * 4);
        memcpy(bot, row.data(), (size_t)w * 4);
    }
}

static std::string loadFile(const std::string& path) {
    if (path.empty()) return "";
    std::ifstream ifs(path);
    if (!ifs) return "";
    return std::string(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
}

static std::string trimCopy(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a])) ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

static std::string cleanPath(const char* s) {
    if (!s) return "";
    return trimCopy(std::string(s));
}

static bool endsWithCI(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); ++i) {
        char a = (char)std::tolower((unsigned char)s[s.size() - suffix.size() + i]);
        char b = (char)std::tolower((unsigned char)suffix[i]);
        if (a != b) return false;
    }
    return true;
}

static bool hasShaderExtension(const std::string& path) {
    return endsWithCI(path, ".glsl") ||
           endsWithCI(path, ".frag") ||
           endsWithCI(path, ".vert") ||
           endsWithCI(path, ".comp") ||
           endsWithCI(path, ".fs") ||
           endsWithCI(path, ".shader") ||
           endsWithCI(path, ".txt");
}

static std::string prepareShader(const std::string& src) {
    std::string s = src;
    if (s.size() >= 3 &&
        (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF) {
        s.erase(0, 3);
    }
    std::istringstream in(s);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trimCopy(line);
        if (t.compare(0, 8, "#version") == 0) continue;
        out << line << '\n';
    }
    return out.str();
}

static GLuint compileStage(GLenum type, const std::string& src, std::string& log) {
    GLuint s = gl::glCreateShader(type);
    const char* csrc = src.c_str();
    gl::glShaderSource(s, 1, &csrc, nullptr);
    gl::glCompileShader(s);
    GLint ok = GL_FALSE;
    gl::glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[4096];
        GLsizei len = 0;
        gl::glGetShaderInfoLog(s, sizeof(buf), &len, buf);
        log.assign(buf, len);
        gl::glDeleteShader(s);
        return 0;
    }
    return s;
}

struct RenderPass {
    std::string compiledSource;
    bool hasCompiledOnce = false;
    GLuint program = 0;

    GLint locResolution  = -1;
    GLint locTime        = -1;
    GLint locTimeDelta   = -1;
    GLint locFrame       = -1;
    GLint locMouse       = -1;
    GLint locChannelRes  = -1;
    GLint locChannelTime = -1;
    GLint locChannel[4]  = { -1, -1, -1, -1 };
    GLint locParam[8]    = { -1, -1, -1, -1, -1, -1, -1, -1 };
    GLint locUseAlpha    = -1;
};

static bool compileShaderInto(const std::string& userSrc, RenderPass& pass, GLuint sharedVS) {
    std::string clean = prepareShader(userSrc);
    std::string fragSrc = std::string(kFragPreamble) + clean + "\n" + kFragEpilogue;

    std::string flog;
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragSrc, flog);
    if (!fs) {
        fprintf(stderr, "[shadert0y] fragment shader error:\n%s\n", flog.c_str());
        return false;
    }

    GLuint prog = gl::glCreateProgram();
    gl::glAttachShader(prog, sharedVS);
    gl::glAttachShader(prog, fs);
    gl::glLinkProgram(prog);

    GLint linked = GL_FALSE;
    gl::glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    gl::glDeleteShader(fs);

    if (!linked) {
        char buf[4096];
        GLsizei len = 0;
        gl::glGetProgramInfoLog(prog, sizeof(buf), &len, buf);
        fprintf(stderr, "[shadert0y] link error:\n%.*s\n", len, buf);
        gl::glDeleteProgram(prog);
        return false;
    }

    if (pass.program) gl::glDeleteProgram(pass.program);
    pass.program = prog;

    pass.locResolution  = gl::glGetUniformLocation(prog, "iResolution");
    pass.locTime        = gl::glGetUniformLocation(prog, "iTime");
    pass.locTimeDelta   = gl::glGetUniformLocation(prog, "iTimeDelta");
    pass.locFrame       = gl::glGetUniformLocation(prog, "iFrame");
    pass.locMouse       = gl::glGetUniformLocation(prog, "iMouse");
    pass.locChannelRes  = gl::glGetUniformLocation(prog, "iChannelResolution");
    pass.locChannelTime = gl::glGetUniformLocation(prog, "iChannelTime");

    pass.locChannel[0]  = gl::glGetUniformLocation(prog, "iChannel0");
    pass.locChannel[1]  = gl::glGetUniformLocation(prog, "iChannel1");
    pass.locChannel[2]  = gl::glGetUniformLocation(prog, "iChannel2");
    pass.locChannel[3]  = gl::glGetUniformLocation(prog, "iChannel3");

    for (int i = 0; i < 8; ++i) {
        char uname[32];
        snprintf(uname, sizeof(uname), "iParam%d", i);
        pass.locParam[i] = gl::glGetUniformLocation(prog, uname);
    }
    pass.locUseAlpha = gl::glGetUniformLocation(prog, "f0rUseShaderAlpha");

    return true;
}

static void bindChannel(RenderPass& p, int idx, GLuint tex) {
    if (p.locChannel[idx] < 0) return;
    gl::glActiveTexture(GL_TEXTURE0 + idx);
    glBindTexture(GL_TEXTURE_2D, tex);
    gl::glUniform1i(p.locChannel[idx], idx);
}

// ---------------------------------------------------------------------------
// File slot: image or basic shader buffer
// ---------------------------------------------------------------------------
struct FileSlot {
    std::string path;
    std::string lastPath;
    std::string loadedSource;
    time_t lastModTime = 0;

    // 0 = none, 1 = image, 2 = shader buffer
    int mode = 0;

    GLuint tex = 0;
    GLuint fbo = 0;
    unsigned int w = 0;
    unsigned int h = 0;

    RenderPass pass;
};

struct ShaderInstance {
    unsigned int width = 0;
    unsigned int height = 0;

    PlatformGL pg;

    GLuint sharedVS = 0;
    GLuint vao      = 0;

    GLuint outFbo = 0;
    GLuint outTex = 0;

    GLuint sceneFbo = 0;
    GLuint sceneTex = 0;
    unsigned int sceneW = 0;
    unsigned int sceneH = 0;

    GLuint inputTex = 0;
    GLuint blackTex = 0;

    FileSlot files[4];

    RenderPass passMain;

    std::string scriptPath;
    std::string lastLoadedContent;
    time_t lastFileModTime = 0;

    double speed = 1.0;
    bool flipVideoY = false;
    bool useShaderAlpha = false;

    unsigned long frameCounter = 0;
    double lastTimelineTime = 0.0;

    double mouseX = 0.0;
    double mouseY = 0.0;

    int iChannelSelect[4] = {1, 0, 0, 0};

    int projectWidthOverride = 0;
    int projectHeightOverride = 0;

    double userParams[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    bool ok() const { return pg.valid; }

    bool initGL();
    void destroy();

    void render(const uint32_t* inframe, uint32_t* outframe, double time);

    void ensureSceneTargets(unsigned int w, unsigned int h);
    void ensureFileTarget(FileSlot& f, unsigned int w, unsigned int h);
    void updateFileSlot(int idx, unsigned int rw, unsigned int rh);
    void renderFileShader(int idx, float iTime, float iTimeDelta, float mouseXPx, float mouseYPx);

    ShaderInstance() = default;
    ~ShaderInstance() { destroy(); }
};

void ShaderInstance::ensureSceneTargets(unsigned int w, unsigned int h) {
    if (sceneFbo && sceneW == w && sceneH == h) return;

    if (sceneFbo) { gl::glDeleteFramebuffers(1, &sceneFbo); sceneFbo = 0; }
    if (sceneTex) { glDeleteTextures(1, &sceneTex); sceneTex = 0; }

    glGenTextures(1, &sceneTex);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::glGenFramebuffers(1, &sceneFbo);
    gl::glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    gl::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);

    if (gl::glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[shadert0y] scene framebuffer incomplete\n");
    }

    sceneW = w;
    sceneH = h;
    gl::glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderInstance::ensureFileTarget(FileSlot& f, unsigned int w, unsigned int h) {
    if (f.fbo && f.w == w && f.h == h) return;

    if (f.fbo) { gl::glDeleteFramebuffers(1, &f.fbo); f.fbo = 0; }
    if (f.tex) { glDeleteTextures(1, &f.tex); f.tex = 0; }

    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::glGenFramebuffers(1, &f.fbo);
    gl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
    gl::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f.tex, 0);

    f.w = w;
    f.h = h;
    gl::glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderInstance::updateFileSlot(int idx, unsigned int rw, unsigned int rh) {
    FileSlot& f = files[idx];
    bool pathChanged = (f.path != f.lastPath);

    if (pathChanged) {
        if (f.pass.program) gl::glDeleteProgram(f.pass.program);
        if (f.fbo) gl::glDeleteFramebuffers(1, &f.fbo);
        if (f.tex) glDeleteTextures(1, &f.tex);

        f.pass = RenderPass();
        f.fbo = 0;
        f.tex = 0;
        f.w = 0;
        f.h = 0;
        f.loadedSource.clear();
        f.lastModTime = 0;
        f.mode = 0;
        f.lastPath = f.path;
    }

    if (f.path.empty()) {
        f.mode = 0;
        return;
    }

    struct stat st;
    time_t mod = 0;
    if (stat(f.path.c_str(), &st) == 0) mod = st.st_mtime;

    bool fileChanged = pathChanged || (mod != f.lastModTime);
    f.lastModTime = mod;

    if (hasShaderExtension(f.path)) {
        bool needCompile = fileChanged || f.pass.program == 0 || f.loadedSource.empty();
        if (needCompile) {
            std::string src = loadFile(f.path);
            if (src.empty()) { f.mode = 0; return; }
            f.loadedSource = src;
            if (!compileShaderInto(src, f.pass, sharedVS)) {
                fprintf(stderr, "[shadert0y] File %d '%s' failed to compile. Using default shader.\n",
                        idx, f.path.c_str());
                compileShaderInto(kDefaultShader, f.pass, sharedVS);
            }
        }
        ensureFileTarget(f, rw, rh);
        f.mode = 2;
        return;
    }

    // Image
    if (!fileChanged && f.mode == 1 && f.tex != 0) return;

    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(f.path.c_str(), &w, &h, &n, 4);
    if (data) {
        if (f.tex) { glDeleteTextures(1, &f.tex); f.tex = 0; }

        flipRowsRGBA((uint32_t*)data, (unsigned)w, (unsigned)h);

        glGenTextures(1, &f.tex);
        glBindTexture(GL_TEXTURE_2D, f.tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        f.w = (unsigned int)w;
        f.h = (unsigned int)h;
        f.mode = 1;
        stbi_image_free(data);
    } else {
        f.mode = 0;
    }
}

void ShaderInstance::renderFileShader(int idx, float iTime, float iTimeDelta, float mouseXPx, float mouseYPx) {
    FileSlot& f = files[idx];
    if (f.mode != 2) return;
    if (!f.fbo) return;
    if (!f.pass.program) return;

    gl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
    glViewport(0, 0, f.w, f.h);
    gl::glUseProgram(f.pass.program);

    if (f.pass.locResolution >= 0) gl::glUniform3f(f.pass.locResolution, (float)f.w, (float)f.h, 1.0f);
    if (f.pass.locTime       >= 0) gl::glUniform1f(f.pass.locTime, iTime);
    if (f.pass.locTimeDelta  >= 0) gl::glUniform1f(f.pass.locTimeDelta, iTimeDelta);
    if (f.pass.locFrame      >= 0) gl::glUniform1i(f.pass.locFrame, (int)(frameCounter & 0x7FFFFFFF));
    if (f.pass.locMouse      >= 0) gl::glUniform4f(f.pass.locMouse, mouseXPx, mouseYPx, -1.0f, -1.0f);

    for (int i = 0; i < 8; ++i) {
        if (f.pass.locParam[i] >= 0) gl::glUniform1f(f.pass.locParam[i], (float)userParams[i]);
    }
    if (f.pass.locUseAlpha >= 0) {
        gl::glUniform1f(f.pass.locUseAlpha, useShaderAlpha ? 1.0f : 0.0f);
    }

    float res[12];
    for (int i = 0; i < 4; ++i) {
        res[i*3+0] = 1.0f; res[i*3+1] = 1.0f; res[i*3+2] = 1.0f;
    }
    if (f.pass.locChannelRes >= 0) gl::glUniform3fv(f.pass.locChannelRes, 4, res);

    float t[4] = { iTime, iTime, iTime, iTime };
    if (f.pass.locChannelTime >= 0) gl::glUniform1fv(f.pass.locChannelTime, 4, t);

    for (int i = 0; i < 4; ++i) bindChannel(f.pass, i, blackTex);

    gl::glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

bool ShaderInstance::initGL() {
    if (!platformInit(pg, (int)width, (int)height)) return false;
    if (!gl::loadAll()) return false;

    std::string vlog;
    sharedVS = compileStage(GL_VERTEX_SHADER, kVertexSrc, vlog);
    if (!sharedVS) return false;

    gl::glGenVertexArrays(1, &vao);

    glGenTextures(1, &outTex);
    glBindTexture(GL_TEXTURE_2D, outTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::glGenFramebuffers(1, &outFbo);
    gl::glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
    gl::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);

    glGenTextures(1, &inputTex);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenTextures(1, &blackTex);
    glBindTexture(GL_TEXTURE_2D, blackTex);
    {
        unsigned char black[4] = {0, 0, 0, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    gl::glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void ShaderInstance::destroy() {
    if (pg.valid) {
        platformMakeCurrent(pg);

        if (passMain.program) gl::glDeleteProgram(passMain.program);
        if (sharedVS) gl::glDeleteShader(sharedVS);
        if (vao) gl::glDeleteVertexArrays(1, &vao);

        if (outFbo) gl::glDeleteFramebuffers(1, &outFbo);
        if (outTex) glDeleteTextures(1, &outTex);

        if (sceneFbo) gl::glDeleteFramebuffers(1, &sceneFbo);
        if (sceneTex) glDeleteTextures(1, &sceneTex);

        if (inputTex) glDeleteTextures(1, &inputTex);
        if (blackTex) glDeleteTextures(1, &blackTex);

        for (int i = 0; i < 4; ++i) {
            if (files[i].pass.program) gl::glDeleteProgram(files[i].pass.program);
            if (files[i].fbo) gl::glDeleteFramebuffers(1, &files[i].fbo);
            if (files[i].tex) glDeleteTextures(1, &files[i].tex);
        }

        passMain.program = 0;

        platformShutdown(pg);
    }

    vao = 0;
    outFbo = 0;
    outTex = 0;
    sceneFbo = 0;
    sceneTex = 0;
    inputTex = 0;
    blackTex = 0;
    sharedVS = 0;
}

void ShaderInstance::render(const uint32_t* inframe, uint32_t* outframe, double time) {
   if (!platformMakeCurrent(pg)) return;

    // Render Width  -> horizontal (X) resolution.
    // Render Height -> vertical   (Y) resolution.
    unsigned int rw = (projectWidthOverride  > 0) ? (unsigned int)projectWidthOverride  : width;
    unsigned int rh = (projectHeightOverride > 0) ? (unsigned int)projectHeightOverride : height;
    if (rw < 1) rw = 1;
    if (rh < 1) rh = 1;

    double iTime = time * speed;
    double iTimeDelta = (time - lastTimelineTime) * speed;
    lastTimelineTime = time;

    struct stat st;
    time_t mod = 0;
    if (!scriptPath.empty() && stat(scriptPath.c_str(), &st) == 0) mod = st.st_mtime;

    if (mod != lastFileModTime) {
        lastLoadedContent = scriptPath.empty() ? "" : loadFile(scriptPath);
        lastFileModTime = mod;
    }

    std::string activeSource = lastLoadedContent;
    if (activeSource != passMain.compiledSource || !passMain.hasCompiledOnce) {
        passMain.compiledSource = activeSource;
        passMain.hasCompiledOnce = true;
        if (activeSource.empty()) {
            compileShaderInto(kDefaultShader, passMain, sharedVS);
        } else if (!compileShaderInto(activeSource, passMain, sharedVS)) {
            fprintf(stderr, "[shadert0y] Main shader failed to compile. Using default shader.\n");
            compileShaderInto(kDefaultShader, passMain, sharedVS);
        }
    }
    if (!passMain.program) return;

    ensureSceneTargets(rw, rh);

    for (int i = 0; i < 4; ++i) updateFileSlot(i, rw, rh);

    float mouseXPx = (float)(mouseX * (double)rw);
    float mouseYPx = (float)(mouseY * (double)rh);

    for (int i = 0; i < 4; ++i) {
        if (files[i].mode == 2) {
            ensureFileTarget(files[i], rw, rh);
            renderFileShader(i, (float)iTime, (float)iTimeDelta, mouseXPx, mouseYPx);
        }
    }

    if (inframe) {
        std::vector<uint32_t> tmp(inframe, inframe + (size_t)width * height);
        if (!flipVideoY) flipRowsRGBA(tmp.data(), width, height);
        glBindTexture(GL_TEXTURE_2D, inputTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, tmp.data());
    }

    gl::glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    glViewport(0, 0, sceneW, sceneH);
    gl::glUseProgram(passMain.program);

    if (passMain.locResolution >= 0) gl::glUniform3f(passMain.locResolution, (float)sceneW, (float)sceneH, 1.0f);
    if (passMain.locTime       >= 0) gl::glUniform1f(passMain.locTime, (float)iTime);
    if (passMain.locTimeDelta  >= 0) gl::glUniform1f(passMain.locTimeDelta, (float)iTimeDelta);
    if (passMain.locFrame      >= 0) gl::glUniform1i(passMain.locFrame, (int)(frameCounter & 0x7FFFFFFF));
    if (passMain.locMouse      >= 0) gl::glUniform4f(passMain.locMouse, mouseXPx, mouseYPx, -1.0f, -1.0f);

    for (int i = 0; i < 8; ++i) {
        if (passMain.locParam[i] >= 0) gl::glUniform1f(passMain.locParam[i], (float)userParams[i]);
    }
    if (passMain.locUseAlpha >= 0) {
        gl::glUniform1f(passMain.locUseAlpha, useShaderAlpha ? 1.0f : 0.0f);
    }

    float res[12];
    float t[4] = { (float)iTime, (float)iTime, (float)iTime, (float)iTime };

    auto useFileSlot = [&](int slot, GLuint& tex, float* outRes) {
        const FileSlot& f = files[slot];
        if (f.mode != 0 && f.tex) {
            tex = f.tex;
            outRes[0] = (float)f.w; outRes[1] = (float)f.h; outRes[2] = 1.0f;
        } else {
            tex = blackTex;
            outRes[0] = 1.0f; outRes[1] = 1.0f; outRes[2] = 1.0f;
        }
    };

    for (int i = 0; i < 4; ++i) {
        GLuint tex = blackTex;
        res[i*3+0] = 1.0f; res[i*3+1] = 1.0f; res[i*3+2] = 1.0f;

        switch (iChannelSelect[i]) {
            case 0: tex = blackTex; break;
            case 1:
                tex = inputTex;
                res[i*3+0] = (float)width; res[i*3+1] = (float)height; res[i*3+2] = 1.0f;
                break;
            case 2: useFileSlot(0, tex, &res[i*3]); break;
            case 3: useFileSlot(1, tex, &res[i*3]); break;
            case 4: useFileSlot(2, tex, &res[i*3]); break;
            case 5: useFileSlot(3, tex, &res[i*3]); break;
            default: tex = blackTex; break;
        }
        bindChannel(passMain, i, tex);
    }

    if (passMain.locChannelRes >= 0) gl::glUniform3fv(passMain.locChannelRes, 4, res);
    if (passMain.locChannelTime >= 0) gl::glUniform1fv(passMain.locChannelTime, 4, t);

    gl::glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Scale the render-resolution scene down/up to the host frame size.
    gl::glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
    gl::glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outFbo);
    gl::glBlitFramebuffer(
        0, 0, (GLint)sceneW, (GLint)sceneH,
        0, 0, (GLint)width,  (GLint)height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR
    );

    gl::glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outframe);
    flipRowsRGBA(outframe, width, height);

    frameCounter++;
}

// ---------------------------------------------------------------------------
// Frei0r API
// ---------------------------------------------------------------------------
extern "C" {

int f0r_init() { return 1; }
void f0r_deinit() {}

void f0r_get_plugin_info(f0r_plugin_info_t* info) {
    info->name = (char*)"shadert0y";
    info->author = (char*)"HyperDev";
    info->plugin_type = F0R_PLUGIN_TYPE_FILTER;
    info->color_model = F0R_COLOR_MODEL_RGBA8888;
    info->frei0r_version = FREI0R_MAJOR_VERSION;
    info->major_version = 8;
    info->minor_version = 0;
    info->num_params = 24;
    info->explanation = (char*)"Shadert0y: Shadertoy-style GLSL filter with file slots, custom iParams, and alpha control.";
}

void f0r_get_param_info(f0r_param_info_t* info, int idx) {
    if (idx >= 16 && idx <= 23) {
        static const char* names[8] = {
            (char*)"iParam0", (char*)"iParam1", (char*)"iParam2", (char*)"iParam3",
            (char*)"iParam4", (char*)"iParam5", (char*)"iParam6", (char*)"iParam7"
        };
        info->name = (char*)names[idx - 16];
        info->type = F0R_PARAM_DOUBLE;
        return;
    }

    switch (idx) {
        case 0:  info->name = (char*)"Script File";      info->type = F0R_PARAM_STRING; break;
        case 1:  info->name = (char*)"Speed";            info->type = F0R_PARAM_DOUBLE; break;
        case 2:  info->name = (char*)"Flip Video Y";     info->type = F0R_PARAM_BOOL;   break;
        case 3:  info->name = (char*)"Use Shader Alpha"; info->type = F0R_PARAM_BOOL;   break;
        case 4:  info->name = (char*)"iChannel0";        info->type = F0R_PARAM_DOUBLE; break;
        case 5:  info->name = (char*)"iChannel1";        info->type = F0R_PARAM_DOUBLE; break;
        case 6:  info->name = (char*)"iChannel2";        info->type = F0R_PARAM_DOUBLE; break;
        case 7:  info->name = (char*)"iChannel3";        info->type = F0R_PARAM_DOUBLE; break;
        case 8:  info->name = (char*)"File 0";           info->type = F0R_PARAM_STRING; break;
        case 9:  info->name = (char*)"File 1";           info->type = F0R_PARAM_STRING; break;
        case 10: info->name = (char*)"File 2";           info->type = F0R_PARAM_STRING; break;
        case 11: info->name = (char*)"File 3";           info->type = F0R_PARAM_STRING; break;
        case 12: info->name = (char*)"Mouse X";          info->type = F0R_PARAM_DOUBLE; break;
        case 13: info->name = (char*)"Mouse Y";          info->type = F0R_PARAM_DOUBLE; break;
        case 14: info->name = (char*)"Render Width";     info->type = F0R_PARAM_DOUBLE; break;
        case 15: info->name = (char*)"Render Height";    info->type = F0R_PARAM_DOUBLE; break;
    }
}

f0r_instance_t f0r_construct(unsigned int width, unsigned int height) {
    ShaderInstance* inst = new ShaderInstance();
    inst->width = width;
    inst->height = height;
    inst->initGL();
    return (f0r_instance_t)inst;
}

void f0r_destruct(f0r_instance_t instance) {
    delete (ShaderInstance*)instance;
}

void f0r_set_param_value(f0r_instance_t instance, f0r_param_t param, int idx) {
    ShaderInstance* inst = (ShaderInstance*)instance;

    if (idx >= 16 && idx <= 23) {
        inst->userParams[idx - 16] = *(f0r_param_double*)param;
        return;
    }

    switch (idx) {
        case 0: {
            inst->scriptPath = cleanPath(*(const char**)param);
            if (inst->scriptPath.empty()) {
                inst->lastLoadedContent.clear();
                inst->lastFileModTime = 0;
                inst->passMain.compiledSource.clear();
                inst->passMain.hasCompiledOnce = false;
            }
            break;
        }
        case 1:  inst->speed = *(f0r_param_double*)param; break;
        case 2:  inst->flipVideoY = (*(f0r_param_double*)param) > 0.5; break;
        case 3:  inst->useShaderAlpha = (*(f0r_param_double*)param) > 0.5; break;
        case 4:  inst->iChannelSelect[0] = (int)(*(f0r_param_double*)param + 0.5); break;
        case 5:  inst->iChannelSelect[1] = (int)(*(f0r_param_double*)param + 0.5); break;
        case 6:  inst->iChannelSelect[2] = (int)(*(f0r_param_double*)param + 0.5); break;
        case 7:  inst->iChannelSelect[3] = (int)(*(f0r_param_double*)param + 0.5); break;
        case 8: {
            inst->files[0].path = cleanPath(*(const char**)param);
            if (inst->files[0].path.empty()) { inst->files[0].loadedSource.clear(); inst->files[0].lastModTime = 0; }
            break;
        }
        case 9: {
            inst->files[1].path = cleanPath(*(const char**)param);
            if (inst->files[1].path.empty()) { inst->files[1].loadedSource.clear(); inst->files[1].lastModTime = 0; }
            break;
        }
        case 10: {
            inst->files[2].path = cleanPath(*(const char**)param);
            if (inst->files[2].path.empty()) { inst->files[2].loadedSource.clear(); inst->files[2].lastModTime = 0; }
            break;
        }
        case 11: {
            inst->files[3].path = cleanPath(*(const char**)param);
            if (inst->files[3].path.empty()) { inst->files[3].loadedSource.clear(); inst->files[3].lastModTime = 0; }
            break;
        }
        case 12: inst->mouseX = *(f0r_param_double*)param; break;
        case 13: inst->mouseY = *(f0r_param_double*)param; break;
        case 14: inst->projectWidthOverride  = (int)(*(f0r_param_double*)param + 0.5); break;
        case 15: inst->projectHeightOverride = (int)(*(f0r_param_double*)param + 0.5); break;
    }
}

void f0r_get_param_value(f0r_instance_t instance, f0r_param_t param, int idx) {
    ShaderInstance* inst = (ShaderInstance*)instance;
    static thread_local std::string t;

    if (idx >= 16 && idx <= 23) {
        *(f0r_param_double*)param = (double)inst->userParams[idx - 16];
        return;
    }

    switch (idx) {
        case 0:  t = inst->scriptPath;      *(const char**)param = t.c_str(); break;
        case 1:  *(f0r_param_double*)param = inst->speed; break;
        case 2:  *(f0r_param_double*)param = inst->flipVideoY ? 1.0 : 0.0; break;
        case 3:  *(f0r_param_double*)param = inst->useShaderAlpha ? 1.0 : 0.0; break;
        case 4:  *(f0r_param_double*)param = (double)inst->iChannelSelect[0]; break;
        case 5:  *(f0r_param_double*)param = (double)inst->iChannelSelect[1]; break;
        case 6:  *(f0r_param_double*)param = (double)inst->iChannelSelect[2]; break;
        case 7:  *(f0r_param_double*)param = (double)inst->iChannelSelect[3]; break;
        case 8:  t = inst->files[0].path;   *(const char**)param = t.c_str(); break;
        case 9:  t = inst->files[1].path;   *(const char**)param = t.c_str(); break;
        case 10: t = inst->files[2].path;   *(const char**)param = t.c_str(); break;
        case 11: t = inst->files[3].path;   *(const char**)param = t.c_str(); break;
        case 12: *(f0r_param_double*)param = inst->mouseX; break;
        case 13: *(f0r_param_double*)param = inst->mouseY; break;
        case 14: *(f0r_param_double*)param = (double)inst->projectWidthOverride; break;
        case 15: *(f0r_param_double*)param = (double)inst->projectHeightOverride; break;
    }
}

void f0r_update(f0r_instance_t instance, double time, const uint32_t* inframe, uint32_t* outframe) {
    ShaderInstance* inst = (ShaderInstance*)instance;
    if (!inst->ok()) {
        memset(outframe, 0, (size_t)inst->width * inst->height * 4);
        return;
    }
    inst->render(inframe, outframe, time);
}

} // extern "C"
