#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <Shader.h>

class Sky {
public:
    Sky();
    ~Sky();

    // Initialize textures and VAO
    void init();

    // Advance time of day: call each frame with deltaTime
    void update(float deltaTime);

    // Render the sun and moon billboards
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

    // Getters for lighting uniforms
    glm::vec3 getSunDirection() const;
    glm::vec3 getSunColor() const;
    float getAmbientStrength() const;
    glm::vec3 getSkyColor() const;

    // Time of day: 0.0 = Noon, 0.25 = Sunset, 0.5 = Midnight, 0.75 = Sunrise
    float timeOfDay = 0.0f;

    // Day cycle speed: full day in seconds
    float dayLengthSeconds = 600.0f; // 10 minute day cycle

    // Time Control
    void togglePause() { paused = !paused; }
    bool isPaused() const { return paused; }

private:
    bool paused = false;
    void loadTexture(const char* path, GLuint& texID);

    Shader* skyShader = nullptr;
    GLuint sunTexture = 0;
    GLuint moonTexture = 0;
    GLuint quadVAO = 0, quadVBO = 0;

    void initQuad();
    void renderBillboard(GLuint texture, const glm::vec3& direction, float size,
                         const glm::mat4& view, const glm::mat4& projection, 
                         const glm::vec3& cameraPos, float brightness);
};
