#include "Init.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef min
#undef max
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace {
    constexpr float kEarthRadius = 6378000.0f;

    static std::filesystem::path GetExeDir() {
#ifdef _WIN32
        char buf[MAX_PATH]{};
        DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return std::filesystem::current_path();
        return std::filesystem::path(buf).parent_path();
#else
        char buf[PATH_MAX]{};
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len <= 0) return std::filesystem::current_path();
        buf[len] = 0;
        return std::filesystem::path(buf).parent_path();
#endif
    }

    static std::string FindInRoots(const char* folder, const char* name) {
        namespace fs = std::filesystem;

        fs::path exe = GetExeDir();
        fs::path cwd = fs::current_path();

        std::vector<fs::path> roots;
        roots.reserve(32);

        auto pushUnique = [&](const fs::path& p) {
            for (auto& r : roots) if (r == p) return;
            roots.push_back(p);
            };

        fs::path p = exe;
        for (int i = 0; i < 10; ++i) {
            pushUnique(p);
            if (!p.has_parent_path()) break;
            p = p.parent_path();
        }

        p = cwd;
        for (int i = 0; i < 10; ++i) {
            pushUnique(p);
            if (!p.has_parent_path()) break;
            p = p.parent_path();
        }

        for (const auto& r : roots) {
            fs::path cand = r / folder / name;
            if (fs::exists(cand) && fs::is_regular_file(cand)) return cand.string();
        }

        fs::path direct1 = fs::path(folder) / name;
        if (fs::exists(direct1) && fs::is_regular_file(direct1)) return direct1.string();

        fs::path direct2 = fs::path(name);
        if (fs::exists(direct2) && fs::is_regular_file(direct2)) return direct2.string();

        throw std::runtime_error(
            std::string("File not found: ") + folder + "/" + name +
            " | exeDir=" + exe.string() +
            " | cwd=" + cwd.string()
        );
    }

    static std::string FindShaderFile(const char* name) { return FindInRoots("shaders", name); }
    static std::string FindTextureFile(const char* name) { return FindInRoots("textures", name); }

    static GLint GetLocAny(GLuint program, std::initializer_list<const char*> names) {
        for (const char* n : names) {
            GLint loc = glGetUniformLocation(program, n);
            if (loc != -1) return loc;
        }
        return -1;
    }

    static void Set1fAny(GLuint program, float v, std::initializer_list<const char*> names) {
        GLint loc = GetLocAny(program, names);
        if (loc != -1) glUniform1f(loc, v);
    }

    static void Set2fAny(GLuint program, float x, float y, std::initializer_list<const char*> names) {
        GLint loc = GetLocAny(program, names);
        if (loc != -1) glUniform2f(loc, x, y);
    }

    static void Set3fAny(GLuint program, const glm::vec3& v, std::initializer_list<const char*> names) {
        GLint loc = GetLocAny(program, names);
        if (loc != -1) glUniform3f(loc, v.x, v.y, v.z);
    }

    static void SetMat4Any(GLuint program, const glm::mat4& m, std::initializer_list<const char*> names) {
        GLint loc = GetLocAny(program, names);
        if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
    }

    static void Bind2D(Texture& tex, Shader& sh, std::initializer_list<const char*> names, GLint unit) {
        GLint loc = GetLocAny(sh.ID, names);
        if (loc != -1) tex.UseTexture(loc, unit);
    }

    static void Bind3D(Texture& tex, Shader& sh, std::initializer_list<const char*> names, GLint unit) {
        GLint loc = GetLocAny(sh.ID, names);
        if (loc != -1) tex.UseTexture3D(loc, unit);
    }

    static void ResetFullscreenState(int w, int h) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_FALSE);
    }

    static void ClearColorOnly() {
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    static void DebugPrintPath(const char* tag, const std::string& p) {
        std::fprintf(stderr, "[%s] %s\n", tag, p.c_str());
    }
}

Init::Init() : Window() {
    camera = std::make_unique<Camera>(
        0.0f, 2000.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        -90.0f, 0.0f
    );

    activeShader = 8;
    moveSpeed = 1200.0f;
    cloudBottom = 1500.0f;
    cloudTop = 9000.0f;

    taaEnabled = false;
    taaHistoryValid = false;
    taaHistoryWeight = 0.90f;
    frameCounter = 0;
}

Init::~Init() {
    destroyTaaTargets();
}

void Init::calcAverageNormals(
    unsigned int* indexes, unsigned int indiceCount,
    GLfloat* vertices, unsigned int verticeCount,
    unsigned int vLength, unsigned int normalOffset)
{
    for (size_t i = 0; i < indiceCount; i += 3) {
        unsigned int in0 = indexes[i] * vLength;
        unsigned int in1 = indexes[i + 1] * vLength;
        unsigned int in2 = indexes[i + 2] * vLength;

        glm::vec3 v1(vertices[in1] - vertices[in0],
            vertices[in1 + 1] - vertices[in0 + 1],
            vertices[in1 + 2] - vertices[in0 + 2]);

        glm::vec3 v2(vertices[in2] - vertices[in0],
            vertices[in2 + 1] - vertices[in0 + 1],
            vertices[in2 + 2] - vertices[in0 + 2]);

        glm::vec3 normal = glm::normalize(glm::cross(v1, v2));

        in0 += normalOffset; in1 += normalOffset; in2 += normalOffset;

        vertices[in0] += normal.x; vertices[in0 + 1] += normal.y; vertices[in0 + 2] += normal.z;
        vertices[in1] += normal.x; vertices[in1 + 1] += normal.y; vertices[in1 + 2] += normal.z;
        vertices[in2] += normal.x; vertices[in2 + 1] += normal.y; vertices[in2 + 2] += normal.z;
    }

    for (size_t i = 0; i < verticeCount / vLength; i++) {
        unsigned int nOffset = (unsigned int)i * vLength + normalOffset;
        glm::vec3 vec(vertices[nOffset], vertices[nOffset + 1], vertices[nOffset + 2]);
        vec = glm::normalize(vec);
        vertices[nOffset] = vec.x;
        vertices[nOffset + 1] = vec.y;
        vertices[nOffset + 2] = vec.z;
    }
}

std::unique_ptr<Mesh> Init::CreateTriangle() {
    std::array<unsigned int, 12> indexes = { 0,3,1, 1,3,2, 0,2,3, 0,1,2 };
    std::array<GLfloat, 32> vertices = {
        -1.0f,-1.0f, 0.0f,  0.0f,0.0f,  0.0f,0.0f,0.0f,
         0.0f,-1.0f, 1.0f,  0.5f,0.0f,  0.0f,0.0f,0.0f,
         1.0f,-1.0f, 0.0f,  1.0f,0.0f,  0.0f,0.0f,0.0f,
         0.0f, 1.0f, 0.0f,  0.5f,1.0f,  0.0f,0.0f,0.0f
    };

    calcAverageNormals(indexes.data(), (unsigned)indexes.size(),
        vertices.data(), (unsigned)vertices.size(), 8, 5);

    auto mesh = std::make_unique<Mesh>();
    mesh->CreateMesh(vertices.data(), indexes.data(),
        (unsigned)vertices.size(), (unsigned)indexes.size());
    return mesh;
}

std::unique_ptr<Mesh> Init::CreateQuad() {
    std::array<unsigned int, 6> indexes = { 0,1,2, 0,2,3 };
    std::array<GLfloat, 12> vertices = {
        -1.0f,-1.0f,0.0f,
         1.0f,-1.0f,0.0f,
         1.0f, 1.0f,0.0f,
        -1.0f, 1.0f,0.0f
    };

    auto mesh = std::make_unique<Mesh>();
    mesh->CreateMesh(vertices.data(), indexes.data(),
        (unsigned)vertices.size(), (unsigned)indexes.size());
    return mesh;
}

float Init::HaltonSequenceAt(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0) {
        f = f / float(base);
        r += f * float(index % base);
        index = int(std::floor(float(index) / float(base)));
    }
    return r;
}

glm::vec2 Init::Halton2D(int frameIndex) {
    float hx = HaltonSequenceAt(frameIndex + 1, 2);
    float hy = HaltonSequenceAt(frameIndex + 1, 3);
    return glm::vec2(hx - 0.5f, hy - 0.5f);
}

void Init::initialize() {
    glfwSwapInterval(1);

    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    lastX = getWindowWidth() * 0.5f;
    lastY = getWindowHeight() * 0.5f;
    firstMouse = true;

    lastFrame = (float)glfwGetTime();
    deltaTime = 0.0f;

    glfwSetWindowUserPointer(getWindow(), this);

    glfwSetCursorPosCallback(getWindow(), [](GLFWwindow* w, double x, double y) {
        auto* self = static_cast<Init*>(glfwGetWindowUserPointer(w));
        if (self) self->cursorPosCallback(w, x, y);
        });

    glfwSetScrollCallback(getWindow(), [](GLFWwindow* w, double xoff, double yoff) {
        auto* self = static_cast<Init*>(glfwGetWindowUserPointer(w));
        if (self) self->scroll_callback(w, xoff, yoff);
        });

    glfwSetMouseButtonCallback(getWindow(), [](GLFWwindow* w, int button, int action, int mods) {
        auto* self = static_cast<Init*>(glfwGetWindowUserPointer(w));
        if (self) self->mouseButtonCallback(w, button, action, mods);
        });

    glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    const std::string vtx = FindShaderFile("vertex.glsl");
    DebugPrintPath("shader.vs", vtx);

    auto tryLoad = [&](std::unique_ptr<Shader>& dst, const char* frag) {
        try {
            const std::string fs = FindShaderFile(frag);
            DebugPrintPath("shader.fs", fs);
            dst = std::make_unique<Shader>(vtx.c_str(), fs.c_str());
        }
        catch (const std::exception& e) {
            std::fprintf(stderr, "Shader load failed (%s): %s\n", frag, e.what());
            dst.reset();
        }
        };

    tryLoad(shader, "fragment.glsl");
    tryLoad(fragmentv2, "fragmentv2.glsl");
    tryLoad(rmarching, "RayMarchingFragment.glsl");
    tryLoad(rmarching2, "RayMarching2.glsl");
    tryLoad(singlecloudfrag, "singlecloudfrag.glsl");
    tryLoad(water, "waterfrag.glsl");
    tryLoad(watersky, "waterskyfrag.glsl");
    tryLoad(cloudsOver, "clouds_over.glsl");

    try {
        const std::string tv = FindShaderFile("ttavert.glsl");
        const std::string tf = FindShaderFile("ttafrag.glsl");
        DebugPrintPath("shader.taa.vs", tv);
        DebugPrintPath("shader.taa.fs", tf);
        taaShader = std::make_unique<Shader>(tv.c_str(), tf.c_str());
    }
    catch (...) {
        taaShader.reset();
    }

    quad = CreateQuad();
    triangle = CreateTriangle();

    auto loadTex2D = [&](std::unique_ptr<Texture>& dst, const char* filename) {
        try {
            const std::string p = FindTextureFile(filename);
            DebugPrintPath("tex2D", p);
            dst = std::make_unique<Texture>(p.c_str());
            dst->LoadTextureA();
        }
        catch (const std::exception& e) {
            std::fprintf(stderr, "Texture init failed (%s): %s\n", filename, e.what());
            dst.reset();
        }
        };

    auto loadTex3D = [&](std::unique_ptr<Texture>& dst, const char* filename) {
        try {
            const std::string p = FindTextureFile(filename);
            DebugPrintPath("tex3D", p);
            dst = std::make_unique<Texture>(p.c_str());
            dst->LoadTexture3D();
        }
        catch (const std::exception& e) {
            std::fprintf(stderr, "Texture3D init failed (%s): %s\n", filename, e.what());
            dst.reset();
        }
        };

    loadTex3D(lowfreq3D, "LowFrequency3DTexture.tga");
    loadTex3D(highfreq3D, "HighFrequency3DTexture.tga");

    loadTex2D(weathermap2D, "weathermap.png");
    loadTex2D(curlnoise2D, "curlNoise.png");

    loadTex2D(gradient_stratus, "gradient_stratus.png");
    loadTex2D(gradient_cumulus, "gradient_cumulus.png");
    loadTex2D(gradient_cumulonimbus, "gradient_cumulonimbus.png");

    destroyTaaTargets();
    taaHistoryValid = false;
    frameCounter = 0;
}

void Init::processInput(GLFWwindow* window) {
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    float speed = moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) speed *= 3.0f;

    glm::vec3 fwd = glm::normalize(camera->Front);
    glm::vec3 rgt = glm::normalize(camera->Right);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera->Position += fwd * speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera->Position -= fwd * speed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera->Position += rgt * speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera->Position -= rgt * speed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)      camera->Position += up * speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera->Position -= up * speed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    auto edgeKey = [&](int key, auto&& onPress) {
        static bool prev[512]{};
        bool now = glfwGetKey(window, key) == GLFW_PRESS;
        if (key >= 0 && key < 512) {
            if (now && !prev[key]) onPress();
            prev[key] = now;
        }
        };

    edgeKey(GLFW_KEY_1, [&] { activeShader = 1; });
    edgeKey(GLFW_KEY_2, [&] { activeShader = 2; });
    edgeKey(GLFW_KEY_3, [&] { activeShader = 3; });
    edgeKey(GLFW_KEY_4, [&] { activeShader = 4; });
    edgeKey(GLFW_KEY_5, [&] { activeShader = 5; });
    edgeKey(GLFW_KEY_6, [&] { activeShader = 6; });
    edgeKey(GLFW_KEY_7, [&] { activeShader = 7; });
    edgeKey(GLFW_KEY_8, [&] { activeShader = 8; });

    edgeKey(GLFW_KEY_T, [&] {
        taaEnabled = !taaEnabled;
        taaHistoryValid = false;
        frameCounter = 0;
        });

    edgeKey(GLFW_KEY_LEFT_BRACKET, [&] {
        taaHistoryWeight = glm::clamp(taaHistoryWeight - 0.02f, 0.0f, 0.99f);
        taaHistoryValid = false;
        });

    edgeKey(GLFW_KEY_RIGHT_BRACKET, [&] {
        taaHistoryWeight = glm::clamp(taaHistoryWeight + 0.02f, 0.0f, 0.99f);
        taaHistoryValid = false;
        });

    edgeKey(GLFW_KEY_KP_ADD, [&] {
        cloudBottom += 200.0f;
        cloudTop += 200.0f;
        });

    edgeKey(GLFW_KEY_KP_SUBTRACT, [&] {
        cloudBottom = std::max(0.0f, cloudBottom - 200.0f);
        cloudTop = std::max(cloudBottom + 500.0f, cloudTop - 200.0f);
        });
}

void Init::bindCommonUniforms(Shader& s, int w, int h, float t, bool taaEnabledPass) {
    glUseProgram(s.ID);

    Set1fAny(s.ID, t, { "Time", "time", "uTime", "iTime" });

    Set1fAny(s.ID, (float)w, { "screenWidth", "ScreenWidth" });
    Set1fAny(s.ID, (float)h, { "screenHeight","ScreenHeight" });
    Set2fAny(s.ID, (float)w, (float)h, { "resolution", "uResolution", "iResolution", "Resolution" });

    Set3fAny(s.ID, camera->Position, { "cameraPosition", "camPos", "uCamPos" });
    Set3fAny(s.ID, camera->Front, { "cameraFront", "camFront", "uCamFront" });
    Set3fAny(s.ID, camera->Up, { "cameraUp", "camUp", "uCamUp" });
    Set3fAny(s.ID, camera->Right, { "cameraRight", "camRight", "uCamRight" });

    glm::vec3 earthCenter(camera->Position.x, -kEarthRadius, camera->Position.z);
    Set3fAny(s.ID, earthCenter, { "EarthCenter", "earthCenter", "uEarthCenter" });

    Set1fAny(s.ID, cloudBottom, { "CloudBottom", "uCloudBottom" });
    Set1fAny(s.ID, cloudTop, { "CloudTop", "uCloudTop" });

    glm::vec2 jitter(0.0f, 0.0f);
    if (taaEnabledPass) {
        jitter = Halton2D((int)frameCounter);
    }
    Set2fAny(s.ID, jitter.x, jitter.y, { "HaltonSequence", "uJitter", "uHalton", "halton" });

    glm::mat4 I(1.0f);
    SetMat4Any(s.ID, I, { "model", "Model" });
    SetMat4Any(s.ID, I, { "view", "View" });
    SetMat4Any(s.ID, I, { "projection", "Projection" });
}

void Init::bindTextures(Shader& s) {
    glUseProgram(s.ID);

    int unit = 0;

    if (lowfreq3D) {
        Bind3D(*lowfreq3D, s,
            { "lowFrequencyTexture", "cloudBaseShapeSampler", "cloudBaseShapeTexture", "LowFrequencyTexture" },
            unit++);
    }

    if (highfreq3D) {
        Bind3D(*highfreq3D, s,
            { "highFrequencyTexture", "cloudHighFreqSampler", "cloudHighFreqTexture", "HighFrequencyTexture" },
            unit++);
    }

    if (weathermap2D) {
        Bind2D(*weathermap2D, s,
            { "WeatherTexture", "weatherMapSampler", "weatherTexture", "WeatherMap" },
            unit++);
    }

    if (curlnoise2D) {
        Bind2D(*curlnoise2D, s,
            { "CurlNoiseTexture", "curlNoiseSampler", "curlNoiseTexture", "CurlNoise" },
            unit++);
    }

    if (gradient_stratus) {
        Bind2D(*gradient_stratus, s,
            { "GradientStratusTexture", "gradientStratusSampler", "gradientStratusTexture" },
            unit++);
    }

    if (gradient_cumulus) {
        Bind2D(*gradient_cumulus, s,
            { "GradientCumulusTexture", "gradientCumulusSampler", "gradientCumulusTexture" },
            unit++);
    }

    if (gradient_cumulonimbus) {
        Bind2D(*gradient_cumulonimbus, s,
            { "GradientCumulonimbusTexture", "gradientCumulonimbusSampler", "gradientCumulonimbusTexture" },
            unit++);
    }
}

void Init::destroyTaaTargets() {
    if (taaFbo) {
        glDeleteFramebuffers(1, &taaFbo);
        taaFbo = 0;
    }
    if (taaColor[0]) glDeleteTextures(1, &taaColor[0]);
    if (taaColor[1]) glDeleteTextures(1, &taaColor[1]);
    taaColor[0] = taaColor[1] = 0;
    taaW = taaH = 0;
    taaHistoryValid = false;
    taaIndex = 0;
}

void Init::ensureTaaTargets(int w, int h) {
    if (taaW == w && taaH == h && taaFbo && taaColor[0] && taaColor[1]) return;

    destroyTaaTargets();

    taaW = w;
    taaH = h;

    glGenFramebuffers(1, &taaFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, taaFbo);

    glGenTextures(2, taaColor);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, taaColor[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taaColor[0], 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        destroyTaaTargets();
        throw std::runtime_error("TAA framebuffer incomplete");
    }
}

void Init::renderSceneTo(GLuint fbo, Shader& s, int w, int h, float t, bool taaEnabledPass) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(s.ID);
    bindCommonUniforms(s, w, h, t, taaEnabledPass);
    bindTextures(s);

    if (quad) quad->RenderMesh();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Init::renderTaaComposite(int w, int h) {
    if (!taaShader || taaShader->ID == 0) return;

    GLuint prog = taaShader->ID;
    glUseProgram(prog);

    GLint locRes = glGetUniformLocation(prog, "uResolution");
    if (locRes != -1) glUniform2f(locRes, (float)w, (float)h);

    float alpha = taaHistoryValid ? taaHistoryWeight : 0.0f;

    GLint locAlpha = glGetUniformLocation(prog, "uAlpha");
    if (locAlpha != -1) glUniform1f(locAlpha, alpha);

    int cur = taaIndex;
    int hist = 1 - taaIndex;

    GLint locCur = glGetUniformLocation(prog, "uCurrent");
    GLint locHist = glGetUniformLocation(prog, "uHistory");
    if (locCur != -1) glUniform1i(locCur, 0);
    if (locHist != -1) glUniform1i(locHist, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, taaColor[cur]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, taaColor[hist]);

    ResetFullscreenState(w, h);
    ClearColorOnly();

    if (quad) quad->RenderMesh();

    taaHistoryValid = true;
    taaIndex = hist;
}

void Init::render() {
    processInput(getWindow());

    int w = 0, h = 0;
    glfwGetFramebufferSize(getWindow(), &w, &h);
    if (w <= 0 || h <= 0) {
        swapBuffersAndPollEvents();
        return;
    }

    float t = (float)glfwGetTime();

    if (activeShader == 8) {
        if (!watersky || watersky->ID == 0 || !cloudsOver || cloudsOver->ID == 0 || !quad) {
            swapBuffersAndPollEvents();
            return;
        }

        frameCounter++;

        if (!taaEnabled) {
            ResetFullscreenState(w, h);
            ClearColorOnly();

            glUseProgram(watersky->ID);
            bindCommonUniforms(*watersky, w, h, t, false);
            bindTextures(*watersky);
            quad->RenderMesh();

            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(cloudsOver->ID);
            bindCommonUniforms(*cloudsOver, w, h, t, false);
            bindTextures(*cloudsOver);
            quad->RenderMesh();

            glDisable(GL_BLEND);

            swapBuffersAndPollEvents();
            return;
        }

        try {
            ensureTaaTargets(w, h);
        }
        catch (...) {
            taaEnabled = false;
            taaHistoryValid = false;
            swapBuffersAndPollEvents();
            return;
        }

        int cur = taaIndex;
        glBindFramebuffer(GL_FRAMEBUFFER, taaFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taaColor[cur], 0);

        glViewport(0, 0, w, h);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_FALSE);

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(watersky->ID);
        bindCommonUniforms(*watersky, w, h, t, true);
        bindTextures(*watersky);
        quad->RenderMesh();

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(cloudsOver->ID);
        bindCommonUniforms(*cloudsOver, w, h, t, true);
        bindTextures(*cloudsOver);
        quad->RenderMesh();

        glDisable(GL_BLEND);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        renderTaaComposite(w, h);

        swapBuffersAndPollEvents();
        return;
    }

    Shader* s = nullptr;
    switch (activeShader) {
    case 1: s = shader.get(); break;
    case 2: s = fragmentv2.get(); break;
    case 3: s = rmarching.get(); break;
    case 4: s = rmarching2.get(); break;
    case 5: s = singlecloudfrag.get(); break;
    case 6: s = water.get(); break;
    case 7: s = watersky.get(); break;
    default: s = watersky.get(); break;
    }

    if (!s || s->ID == 0 || !quad) {
        swapBuffersAndPollEvents();
        return;
    }

    if (!taaEnabled) {
        frameCounter++;

        ResetFullscreenState(w, h);
        ClearColorOnly();

        glUseProgram(s->ID);
        bindCommonUniforms(*s, w, h, t, false);
        bindTextures(*s);
        quad->RenderMesh();

        swapBuffersAndPollEvents();
        return;
    }

    try {
        ensureTaaTargets(w, h);
    }
    catch (...) {
        taaEnabled = false;
        taaHistoryValid = false;
        swapBuffersAndPollEvents();
        return;
    }

    int cur = taaIndex;
    glBindFramebuffer(GL_FRAMEBUFFER, taaFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taaColor[cur], 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    frameCounter++;

    renderSceneTo(taaFbo, *s, w, h, t, true);
    renderTaaComposite(w, h);

    swapBuffersAndPollEvents();
}

void Init::cursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    camera->ProcessMouseMovement(xoffset, yoffset);
}

void Init::scroll_callback(GLFWwindow*, double, double yoffset) {
    camera->ProcessMouseScroll((float)yoffset);
}

void Init::mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }
}
