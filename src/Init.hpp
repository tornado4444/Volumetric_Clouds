#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <memory>
#include <string>
#include <cstdint>

#include "Window.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "Shader.hpp"
#include "Texture.hpp"

class Init : public Window {
public:
    Init();
    ~Init();

    Init(const Init&) = delete;
    Init& operator=(const Init&) = delete;
    Init(Init&&) noexcept = default;
    Init& operator=(Init&&) noexcept = default;

    void initialize();
    void render();

    void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void processInput(GLFWwindow* window);
    void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

private:
    static void calcAverageNormals(
        unsigned int* indexes,
        unsigned int indiceCount,
        GLfloat* vertices,
        unsigned int verticeCount,
        unsigned int vLength,
        unsigned int normalOffset
    );

    static float HaltonSequenceAt(int index, int base);
    static glm::vec2 Halton2D(int frameIndex);

    std::unique_ptr<Mesh> CreateTriangle();
    std::unique_ptr<Mesh> CreateQuad();

private:
    void bindCommonUniforms(Shader& s, int w, int h, float t, bool taaEnabled);
    void bindTextures(Shader& s);

private:
    void ensureTaaTargets(int w, int h);
    void destroyTaaTargets();
    void renderSceneTo(GLuint fbo, Shader& s, int w, int h, float t, bool taaEnabled);
    void renderTaaComposite(int w, int h);

private:
    std::unique_ptr<Camera> camera;

    // single-pass shaders
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Shader> fragmentv2;
    std::unique_ptr<Shader> rmarching;
    std::unique_ptr<Shader> rmarching2;
    std::unique_ptr<Shader> singlecloudfrag;
    std::unique_ptr<Shader> water;
    std::unique_ptr<Shader> watersky;

    // overlay clouds (alpha) for combined mode
    std::unique_ptr<Shader> cloudsOver;

    // textures
    std::unique_ptr<Texture> lowfreq3D;
    std::unique_ptr<Texture> highfreq3D;
    std::unique_ptr<Texture> weathermap2D;
    std::unique_ptr<Texture> curlnoise2D;
    std::unique_ptr<Texture> gradient_stratus;
    std::unique_ptr<Texture> gradient_cumulus;
    std::unique_ptr<Texture> gradient_cumulonimbus;

    // meshes
    std::unique_ptr<Mesh> triangle;
    std::unique_ptr<Mesh> quad;

    glm::mat4 projection{ 1.0f };
    glm::mat4 view{ 1.0f };

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    bool firstMouse = true;
    float lastX = 0.0f;
    float lastY = 0.0f;

    float aspectRatio = 1.0f;

    // 1..7 = твои одиночные шейдеры, 8 = OCEAN+SKY + CLOUDS OVERLAY
    int activeShader = 8;

    // combined cloud heights (meters)
    float cloudBottom = 300.0f;
    float cloudTop = 2500.0f;

    // planar movement tuning (meters/sec-ish)
    float moveSpeed = 120.0f;

private:
    // TAA (optional)
    bool taaEnabled = false;
    float taaHistoryWeight = 0.92f;
    bool taaHistoryValid = false;

    std::unique_ptr<Shader> taaShader;
    GLuint taaFbo = 0;
    GLuint taaColor[2] = { 0, 0 };
    int taaIndex = 0;
    int taaW = 0;
    int taaH = 0;

    uint64_t frameCounter = 0;
};
